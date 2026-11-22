#include "cloudaccounts.h"
#include "cloudprotocol.h"
#include "cloudstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <algorithm>
#include <cstring>

namespace {
QByteArray json(const QJsonObject &value) { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QJsonObject registrations() {
    return {{"dropbox", QJsonObject{{"clientId", "quaderno-fixture-dropbox"}}},
            {"googleDrive", QJsonObject{{"clientId", "quaderno-fixture.apps.googleusercontent.com"}}},
            {"oneDrive", QJsonObject{{"clientId", "11111111-2222-3333-4444-555555555555"}, {"tenant", "common"}}}};
}
bool prepare(const QString &directory) {
    return CloudStore::privateDirectory(directory)
        && CloudStore::privateWrite(QDir(directory).filePath("oauth-registration.json"), json(registrations()));
}
bool savedAccess(const QString &directory, const QString &provider, bool expired = false) {
    return CloudStore(directory, false).save({{provider, QJsonObject{
        {"clientId", registrations().value(provider).toObject().value("clientId")},
        {"accessToken", "fixture-access-secret"}, {"refreshToken", "fixture-refresh-secret"},
        {"expiresAt", double(QDateTime::currentSecsSinceEpoch() + (expired ? -10 : 3600))}}}});
}
QByteArray tokens(const QString &refresh = QStringLiteral("fixture-new-refresh")) {
    return json({{"access_token", "fixture-new-access"}, {"refresh_token", refresh}, {"expires_in", 3600}, {"token_type", "Bearer"}});
}
struct Response {
    int status = 200;
    QByteArray body;
    QMap<QByteArray, QByteArray> headers;
    int delay = 0;
};
class Reply final : public QNetworkReply {
public:
    Reply(const QNetworkRequest &request, const Response &response, QObject *parent)
        : QNetworkReply(parent), m_body(response.body) {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        for (auto it = response.headers.cbegin(); it != response.headers.cend(); ++it) setRawHeader(it.key(), it.value());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(response.delay, this, [this, status = response.status] {
            if (m_done) return;
            if (status >= 400) setError(QNetworkReply::ContentAccessDenied, QStringLiteral("fixture failure"));
            if (!m_body.isEmpty()) emit readyRead();
            if (m_done) return;
            m_done = true; setFinished(true); emit finished();
        });
    }
    void abort() override {
        if (m_done) return;
        m_done = true; setError(QNetworkReply::OperationCanceledError, QStringLiteral("fixture cancelled"));
        setFinished(true); emit finished();
    }
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maximum) override {
        const auto count = std::min(maximum, qint64(m_body.size() - m_offset));
        if (count <= 0) return -1;
        memcpy(data, m_body.constData() + m_offset, size_t(count)); m_offset += count; return count;
    }
private:
    QByteArray m_body;
    qsizetype m_offset = 0;
    bool m_done = false;
};
class Network final : public QNetworkAccessManager {
public:
    struct Sent { QNetworkRequest request; QByteArray body; };
    QList<Response> responses;
    QList<Sent> sent;
    bool unexpected = false;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *outgoing) override {
        sent.append({request, outgoing ? outgoing->readAll() : QByteArray{}});
        if (responses.isEmpty()) { unexpected = true; return new Reply(request, {500, "{}", {}, 0}, this); }
        return new Reply(request, responses.takeFirst(), this);
    }
};
QByteArray list(const QString &provider, const QString &name = QStringLiteral("note.md"), qint64 size = 3) {
    if (provider == "dropbox") return json({{"entries", QJsonArray{QJsonObject{{".tag", "file"}, {"path_lower", "/note.md"}, {"name", name}, {"size", double(size)}}}}, {"has_more", false}});
    if (provider == "googleDrive") return json({{"files", QJsonArray{QJsonObject{{"id", "file-id"}, {"name", name}, {"size", QString::number(size)}, {"mimeType", "text/markdown"}}}}});
    return json({{"value", QJsonArray{QJsonObject{{"id", "file-id"}, {"name", name}, {"size", double(size)}, {"file", QJsonObject{{"mimeType", "text/markdown"}}}}}}});
}
}

class CloudTests : public QObject {
    Q_OBJECT
private slots:
    void supportedPathsAndEndpointOriginsAreStrict() {
        QVERIFY(CloudProtocol::supportedName("note.quaderno"));
        QVERIFY(CloudProtocol::supportedName("Image.WEBP"));
        QVERIFY(!CloudProtocol::supportedName("../note.md"));
        QVERIFY(!CloudProtocol::supportedName("a\\note.md"));
        QVERIFY(!CloudProtocol::supportedName("bad\nname.md"));
        QVERIFY(!CloudProtocol::supportedName("program.sh"));
        QVERIFY(CloudProtocol::downloadUrl("oneDrive", QUrl("https://public.dm.files.1drv.com/a?temporary=value")));
        QVERIFY(CloudProtocol::uploadUrl("oneDrive", QUrl("https://sn3302.up.1drv.com/a")));
        QVERIFY(!CloudProtocol::downloadUrl("oneDrive", QUrl("https://files.1drv.com.attacker.invalid/a")));
        QVERIFY(!CloudProtocol::downloadUrl("oneDrive", QUrl("http://public.dm.files.1drv.com/a")));
        QVERIFY(!CloudProtocol::downloadUrl("googleDrive", QUrl("https://token@www.googleapis.com/a")));
        QVERIFY(!CloudProtocol::downloadUrl("dropbox", QUrl("https://127.0.0.1/a")));
        QVERIFY(!CloudProtocol::uploadUrl("googleDrive", QUrl("https://www.googleapis.com/other")));
        QCOMPARE(CloudProtocol::challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"), QString("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
        const auto header = CloudProtocol::asciiJson({{"path", "/caffè.md"}});
        QVERIFY(header.contains("\\u00e8"));
        QCOMPARE(QJsonDocument::fromJson(header).object().value("path").toString(), QString::fromUtf8("/caffè.md"));
    }

    void privateStoreRejectsPublicPermissionsAndLinks() {
        QTemporaryDir directory;
        CloudStore store(directory.path(), false);
        QVERIFY(store.prepare());
        QVERIFY(store.save({{"dropbox", QJsonObject{{"refreshToken", "fixture-private"}}}}));
        const QString path = directory.filePath("tokens.json");
        QVERIFY(!(QFileInfo(path).permissions() & (QFile::ReadGroup | QFile::ReadOther)));
        bool ok = false;
        QVERIFY(!store.load(&ok).isEmpty()); QVERIFY(ok);
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadOther));
        QVERIFY(store.load(&ok).isEmpty()); QVERIFY(!ok);
        const QString linked = directory.filePath("linked.json");
        QVERIFY(QFile::link(path, linked));
        QVERIFY(CloudStore::privateRead(linked, 1024, &ok).isEmpty()); QVERIFY(!ok);
        QVERIFY(!CloudStore::privateWrite(linked, "changed"));
    }

    void missingRegistrationNeverPretendsConnected() {
        QTemporaryDir directory; Network network;
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        QCOMPARE(cloud.accounts().size(), 3);
        for (const QVariant &account : cloud.accounts()) {
            QVERIFY(!account.toMap().value("configured").toBool());
            QVERIFY(!account.toMap().value("connected").toBool());
        }
        QVERIFY(cloud.selectProvider("googleDrive")); cloud.beginAuthorization();
        QVERIFY(!cloud.error().isEmpty()); QVERIFY(!cloud.authPending()); QVERIFY(!cloud.busy());
        QCOMPARE(network.sent.size(), 0);
        QVERIFY(!cloud.selectProvider("unknown-provider"));
    }

    void dropboxPkceStoresOnlyPrivateTokensAndListsRealResponse() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); Network network;
        network.responses = {{200, tokens()}, {200, list("dropbox")}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        QVERIFY(cloud.selectProvider("dropbox")); cloud.beginAuthorization();
        QVERIFY(cloud.authPending()); QCOMPARE(network.sent.size(), 0);
        const QUrlQuery authorization(cloud.authorizationUrl());
        QCOMPARE(authorization.queryItemValue("code_challenge_method"), QString("S256"));
        QVERIFY(!authorization.hasQueryItem("redirect_uri"));
        cloud.submitAuthorizationCode("fixture-authorization-code");
        QTRY_VERIFY(!cloud.busy());
        QCOMPARE(cloud.entries().size(), 1); QCOMPARE(network.sent.size(), 2);
        const QUrlQuery exchange(QString::fromUtf8(network.sent[0].body));
        QCOMPARE(exchange.queryItemValue("grant_type"), QString("authorization_code"));
        QCOMPARE(CloudProtocol::challenge(exchange.queryItemValue("code_verifier")), authorization.queryItemValue("code_challenge"));
        QVERIFY(!exchange.hasQueryItem("client_secret"));
        QVERIFY(!cloud.authPending()); QVERIFY(cloud.authorizationUrl().isEmpty());
        QVERIFY(!QFileInfo::exists(directory.filePath("oauth-browser-request.json")));
        bool ok = false; const auto saved = CloudStore(directory.path(), false).load(&ok);
        QVERIFY(ok); QCOMPARE(saved.value("dropbox").toObject().value("refreshToken").toString(), QString("fixture-new-refresh"));
        QVERIFY(!QJsonDocument::fromVariant(cloud.accounts()).toJson().contains("fixture-new-refresh"));
        QVERIFY(!network.unexpected);
    }

    void refreshReplacesRotatedTokenBeforeAccessingFiles() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); QVERIFY(savedAccess(directory.path(), "googleDrive", true)); Network network;
        network.responses = {{200, tokens("fixture-rotated-refresh")}, {200, list("googleDrive")}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("googleDrive"); cloud.refreshFiles();
        QTRY_VERIFY(!cloud.busy()); QCOMPARE(network.sent.size(), 2);
        QCOMPARE(network.sent[0].request.url().host(), QString("oauth2.googleapis.com"));
        QCOMPARE(QUrlQuery(QString::fromUtf8(network.sent[0].body)).queryItemValue("grant_type"), QString("refresh_token"));
        QCOMPARE(network.sent[1].request.rawHeader("Authorization"), QByteArray("Bearer fixture-new-access"));
        bool ok = false;
        QCOMPARE(CloudStore(directory.path(), false).load(&ok).value("googleDrive").toObject().value("refreshToken").toString(), QString("fixture-rotated-refresh"));
        QVERIFY(ok);
    }

    void pinCancellationDiscardsLateOAuthAndPrivateHandoff() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); Network network;
        network.responses = {{200, tokens(), {}, 80}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("dropbox"); cloud.beginAuthorization(); cloud.submitAuthorizationCode("fixture-code");
        QVERIFY(cloud.busy()); cloud.setLocked(true);
        QTest::qWait(120);
        QVERIFY(!cloud.busy()); QVERIFY(!cloud.authPending()); QVERIFY(cloud.authorizationUrl().isEmpty());
        QVERIFY(!QFileInfo::exists(directory.filePath("tokens.json")));
        QVERIFY(!QFileInfo::exists(directory.filePath("oauth-browser-request.json")));
        QVERIFY(cloud.entries().isEmpty());
        const auto count = network.sent.size(); cloud.refreshFiles(); cloud.beginAuthorization(); cloud.disconnectAccount();
        QCOMPARE(network.sent.size(), count);
        cloud.setLocked(false); QVERIFY(cloud.selectProvider("oneDrive"));
        QVERIFY(!cloud.accounts()[2].toMap().value("connected").toBool());
    }

    void googleCallbackRejectsInvalidStateAndAcceptsOnlyCurrentPkceAttempt() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); Network network;
        network.responses = {{200, tokens()}, {200, list("googleDrive")}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("googleDrive"); cloud.beginAuthorization();
        QVERIFY(cloud.callbackPort() > 1024);
        const QUrlQuery authorization(cloud.authorizationUrl());
        const QString state = authorization.queryItemValue("state");
        QTcpSocket invalid;
        invalid.connectToHost(QHostAddress::LocalHost, quint16(cloud.callbackPort()));
        QTRY_COMPARE(invalid.state(), QAbstractSocket::ConnectedState);
        invalid.write("GET /oauth/callback?code=fixture-code&state=wrong HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QTRY_VERIFY(invalid.bytesAvailable() > 0);
        QVERIFY(invalid.readAll().contains("400 Bad Request"));
        QCOMPARE(network.sent.size(), 0); QVERIFY(cloud.authPending());
        QTcpSocket valid;
        valid.connectToHost(QHostAddress::LocalHost, quint16(cloud.callbackPort()));
        QTRY_COMPARE(valid.state(), QAbstractSocket::ConnectedState);
        valid.write("GET /oauth/callback?code=fixture-code&state=" + state.toLatin1() + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QTRY_VERIFY(!cloud.busy()); QCOMPARE(network.sent.size(), 2);
        const QUrlQuery exchange(QString::fromUtf8(network.sent[0].body));
        QCOMPARE(CloudProtocol::challenge(exchange.queryItemValue("code_verifier")), authorization.queryItemValue("code_challenge"));
        QCOMPARE(exchange.queryItemValue("redirect_uri", QUrl::FullyDecoded), authorization.queryItemValue("redirect_uri", QUrl::FullyDecoded));
        QVERIFY(!cloud.authPending()); QCOMPARE(cloud.callbackPort(), 0);
    }

    void oneDriveDeviceFlowPublishesValidNonceAndRespectsPending() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); Network network;
        network.responses = {{200, json({{"device_code", "fixture-device-secret"}, {"user_code", "ABCD-EFGH"},
            {"verification_uri", "https://microsoft.com/devicelogin"}, {"expires_in", 900}, {"interval", 1}})},
            {400, json({{"error", "authorization_pending"}})}, {200, tokens()}, {200, list("oneDrive")}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("oneDrive"); cloud.beginAuthorization();
        QTRY_COMPARE(cloud.userCode(), QString("ABCD-EFGH"));
        bool ok = false;
        const auto handoff = QJsonDocument::fromJson(CloudStore::privateRead(directory.filePath("oauth-browser-request.json"), 16384, &ok)).object();
        QVERIFY(ok); QCOMPARE(handoff.value("requestId").toString().size(), 43);
        QVERIFY(!json(handoff).contains("fixture-device-secret"));
        QTRY_VERIFY_WITH_TIMEOUT(!cloud.busy(), 4000);
        QCOMPARE(network.sent.size(), 4); QCOMPARE(cloud.entries().size(), 1);
        QVERIFY(!cloud.authPending()); QVERIFY(cloud.userCode().isEmpty());
        for (const auto &sent : network.sent) QVERIFY(!sent.body.contains("client_secret="));
    }

    void downloadRedirectNeverForwardsBearerToCdn() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); QVERIFY(savedAccess(directory.path(), "oneDrive")); Network network;
        network.responses = {{200, list("oneDrive")}, {302, {}, {{"Location", "https://public.dm.files.1drv.com/file?temporary=fixture"}}}, {200, "abc"}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        QSignalSpy downloaded(&cloud, &CloudAccounts::downloaded);
        cloud.selectProvider("oneDrive"); cloud.refreshFiles(); QTRY_VERIFY(!cloud.busy());
        cloud.downloadFile("file-id"); QTRY_COMPARE(downloaded.count(), 1);
        QCOMPARE(network.sent.size(), 3);
        QCOMPARE(network.sent[1].request.rawHeader("Authorization"), QByteArray("Bearer fixture-access-secret"));
        QVERIFY(network.sent[2].request.rawHeader("Authorization").isEmpty());
        QVERIFY(network.sent[2].request.rawHeader("Dropbox-API-Arg").isEmpty());
        const QString path = downloaded[0][0].toUrl().toLocalFile();
        QVERIFY(path.startsWith(directory.path() + "/downloads/"));
        QFile result(path); QVERIFY(result.open(QIODevice::ReadOnly)); QCOMPARE(result.readAll(), QByteArray("abc"));
        result.close();
        QVERIFY(!(QFileInfo(path).permissions() & (QFile::ReadGroup | QFile::ReadOther)));
        const QString foreign = directory.filePath("user-owned.md");
        QVERIFY(CloudStore::privateWrite(foreign, "preserve"));
        QVERIFY(!cloud.discardDownload(QUrl::fromLocalFile(foreign))); QVERIFY(QFileInfo::exists(foreign));
        QVERIFY(QFile::rename(path, path + ".hold"));
        QVERIFY(QFile::link(foreign, path));
        QVERIFY(!cloud.discardDownload(QUrl::fromLocalFile(path))); QVERIFY(QFileInfo::exists(foreign));
        QVERIFY(QFile::remove(path)); QVERIFY(QFile::rename(path + ".hold", path));
        cloud.setLocked(true); // PIN cancellation still permits safe staging cleanup.
        QVERIFY(cloud.discardDownload(QUrl::fromLocalFile(path)));
        QVERIFY(!QFileInfo::exists(path)); QVERIFY(!QDir(QFileInfo(path).absolutePath()).exists());
        QVERIFY(!cloud.discardDownload(QUrl::fromLocalFile(path))); QVERIFY(cloud.error().isEmpty());
        QVERIFY(QFileInfo::exists(foreign));
    }

    void unsafeDownloadRedirectAndChangedLengthLeaveNoImportedFile() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); QVERIFY(savedAccess(directory.path(), "oneDrive")); Network network;
        network.responses = {{200, list("oneDrive")}, {302, {}, {{"Location", "https://attacker.invalid/stolen"}}}, {200, "xy"}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network); QSignalSpy downloaded(&cloud, &CloudAccounts::downloaded);
        cloud.selectProvider("oneDrive"); cloud.refreshFiles(); QTRY_VERIFY(!cloud.busy());
        cloud.downloadFile("file-id"); QTRY_VERIFY(!cloud.busy());
        QVERIFY(!cloud.error().isEmpty()); QCOMPARE(network.sent.size(), 2); QCOMPARE(downloaded.count(), 0);
        cloud.downloadFile("file-id"); QTRY_VERIFY(!cloud.busy());
        QVERIFY(!cloud.error().isEmpty()); QCOMPARE(downloaded.count(), 0);
    }

    void listingsIgnoreUnsafeNamesAndMarkOversizedNotebooks() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); QVERIFY(savedAccess(directory.path(), "googleDrive")); Network network;
        const QJsonArray files{
            QJsonObject{{"id", "bad"}, {"name", "../secret.md"}, {"size", "1"}},
            QJsonObject{{"id", "big"}, {"name", "large.quaderno"}, {"size", "67108865"}},
            QJsonObject{{"id", "folder"}, {"name", "Books"}, {"mimeType", "application/vnd.google-apps.folder"}}};
        network.responses = {{200, json({{"files", files}})}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("googleDrive"); cloud.refreshFiles(); QTRY_VERIFY(!cloud.busy());
        QCOMPARE(cloud.entries().size(), 2);
        QVERIFY(!cloud.entries()[0].toMap().value("canDownload").toBool());
        cloud.downloadFile("bad"); cloud.downloadFile("big"); QCOMPARE(network.sent.size(), 1);
    }

    void boundedResponsesAndProviderErrorsNeverExposeSecrets() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); QVERIFY(savedAccess(directory.path(), "googleDrive")); Network network;
        network.responses = {{200, QByteArray(int(CloudProtocol::maximumJsonBytes + 1), 'x')},
            {403, json({{"error_description", "fixture-refresh-secret should never appear"}})}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("googleDrive"); cloud.refreshFiles(); QTRY_VERIFY(!cloud.busy());
        QVERIFY(!cloud.error().isEmpty()); QVERIFY(cloud.entries().isEmpty());
        cloud.refreshFiles(); QTRY_VERIFY(!cloud.busy());
        QVERIFY(!cloud.error().contains("fixture-refresh-secret"));
    }

    void uploadsCreateNewCopiesAndUseUnauthenticatedMicrosoftSession() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); QVERIFY(savedAccess(directory.path(), "oneDrive")); Network network;
        const QString path = directory.filePath("upload.md"); QVERIFY(CloudStore::privateWrite(path, "hello"));
        network.responses = {{200, json({{"uploadUrl", "https://sn3302.up.1drv.com/session?temporary=fixture"}})},
            {201, json({{"id", "new-id"}, {"name", "upload.md"}})}};
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("oneDrive"); cloud.uploadFile(QUrl::fromLocalFile(path)); QTRY_VERIFY(!cloud.busy());
        QVERIFY(cloud.error().isEmpty()); QCOMPARE(network.sent.size(), 2);
        QCOMPARE(QJsonDocument::fromJson(network.sent[0].body).object().value("item").toObject().value("@microsoft.graph.conflictBehavior").toString(), QString("fail"));
        QVERIFY(network.sent[1].request.rawHeader("Authorization").isEmpty());
        QCOMPARE(network.sent[1].request.rawHeader("Content-Range"), QByteArray("bytes 0-4/5"));
        QCOMPARE(network.sent[1].body, QByteArray("hello"));
    }

    void localDisconnectDoesNotRevokeOtherMicrosoftSessions() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); QVERIFY(savedAccess(directory.path(), "oneDrive")); Network network;
        CloudAccounts cloud(directory.path(), true, nullptr, &network);
        cloud.selectProvider("oneDrive"); cloud.disconnectAccount(true);
        QVERIFY(!cloud.error().isEmpty()); QCOMPARE(network.sent.size(), 0);
        cloud.disconnectAccount(false); QVERIFY(cloud.error().isEmpty()); QCOMPARE(network.sent.size(), 0);
        QVERIFY(!cloud.accounts()[2].toMap().value("connected").toBool());
        bool ok = false; QVERIFY(CloudStore(directory.path(), false).load(&ok).isEmpty()); QVERIFY(ok);
    }

    void secondInstanceCannotCancelAnotherLoginOrWriteItsTokens() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); Network network;
        {
            CloudAccounts first(directory.path(), true, nullptr, &network);
            first.selectProvider("dropbox"); first.beginAuthorization();
            const QString pending = directory.filePath("oauth-browser-request.json");
            QVERIFY(QFileInfo::exists(pending));
            QVERIFY(!(QFileInfo(directory.filePath(".account-access.lock")).permissions() & (QFile::ReadGroup | QFile::ReadOther)));
            {
                CloudAccounts second(directory.path(), true, nullptr, &network);
                second.selectProvider("dropbox"); second.beginAuthorization();
                QVERIFY(!second.authPending()); QVERIFY(!second.error().isEmpty());
                second.disconnectAccount(); second.cancel();
                QVERIFY(QFileInfo::exists(pending)); QVERIFY(first.authPending());
            }
            QVERIFY(QFileInfo::exists(pending));
        }
        CloudAccounts reopened(directory.path(), true, nullptr, &network);
        QVERIFY(reopened.accounts()[0].toMap().value("configured").toBool());
        QVERIFY(!QFileInfo::exists(directory.filePath("oauth-browser-request.json")));
    }

    void lockPathSymlinkIsRejectedWithoutTouchingTargetOrHandoff() {
        QTemporaryDir directory; QVERIFY(prepare(directory.path())); Network network;
        const QString target = directory.filePath("unrelated-file");
        QVERIFY(CloudStore::privateWrite(target, "preserve"));
        QVERIFY(QFile::link(target, directory.filePath(".account-access.lock")));
        QVERIFY(CloudStore::privateWrite(directory.filePath("oauth-browser-request.json"), "preserve-handoff"));
        {
            CloudAccounts blocked(directory.path(), true, nullptr, &network);
            blocked.selectProvider("dropbox"); blocked.beginAuthorization();
            QVERIFY(!blocked.authPending());
        }
        bool ok = false;
        QCOMPARE(CloudStore::privateRead(target, 100, &ok), QByteArray("preserve")); QVERIFY(ok);
        QCOMPARE(CloudStore::privateRead(directory.filePath("oauth-browser-request.json"), 100, &ok), QByteArray("preserve-handoff")); QVERIFY(ok);
    }
};

QTEST_GUILESS_MAIN(CloudTests)
#include "cloud_tests.moc"
