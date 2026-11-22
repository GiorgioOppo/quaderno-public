#include "cloudaccounts.h"
#include "cloudprotocol.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QUuid>
#include <algorithm>

namespace {
const QStringList providers{QStringLiteral("googleDrive"), QStringLiteral("dropbox"), QStringLiteral("oneDrive")};
const QString googleScope = QStringLiteral("https://www.googleapis.com/auth/drive.readonly https://www.googleapis.com/auth/drive.file");
const QString microsoftScope = QStringLiteral("offline_access Files.ReadWrite");
QByteArray json(const QJsonObject &value) { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QJsonObject object(const QByteArray &value) { return QJsonDocument::fromJson(value).object(); }
bool secret(const QString &value) {
    return !value.isEmpty() && value.size() <= 16384 && !value.contains(QChar::Null)
        && !value.contains(u'\r') && !value.contains(u'\n');
}
QString segment(const QString &value) { return QString::fromLatin1(QUrl::toPercentEncoding(value)); }
qint64 now() { return QDateTime::currentSecsSinceEpoch(); }
}

CloudAccounts::CloudAccounts(QString directory, bool tablet, QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), m_tabletMode(tablet), m_testNetwork(network != nullptr),
      m_store(std::move(directory), network == nullptr), m_network(network ? network : new QNetworkAccessManager(this))
{
    m_storeReady = m_store.prepare();
    if (m_storeReady) {
        const QString path = QDir(m_store.directory()).filePath(QStringLiteral(".account-access.lock"));
        if (QFileInfo(path).isSymLink()) m_storeReady = false;
        else {
            m_storeLock = std::make_unique<QLockFile>(path);
            m_storeReady = m_storeLock->tryLock(0);
            if (m_storeReady && !QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner)) {
                m_storeLock->unlock(); m_storeReady = false;
            }
        }
    }
    if (m_storeReady) m_credentials = m_store.load(&m_storeReady);
    if (!m_storeReady) m_error = tr("Archivio privato degli accessi non disponibile.");
    reloadRegistrations();
    m_deadline.setSingleShot(true);
    m_authDeadline.setSingleShot(true);
    m_poll.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] { fail(tr("Il servizio non risponde. Riprova più tardi.")); });
    connect(&m_authDeadline, &QTimer::timeout, this, [this] { fail(tr("Autorizzazione scaduta. Premi di nuovo Accedi.")); });
    connect(&m_poll, &QTimer::timeout, this, &CloudAccounts::pollDeviceCode);
    m_callback.setMaxPendingConnections(4);
    connect(&m_callback, &QTcpServer::newConnection, this, &CloudAccounts::acceptCallback);
}

CloudAccounts::~CloudAccounts() { cancel(); }

bool CloudAccounts::reloadRegistrations()
{
    m_registrations = {};
    const QString path = QDir(m_store.directory()).filePath(QStringLiteral("oauth-registration.json"));
    if (!QFileInfo::exists(path)) return false;
    bool ok = false;
    const QJsonObject source = object(CloudStore::privateRead(path, 64 * 1024, &ok));
    if (!ok || source.isEmpty()) return false;
    for (const QString &key : providers) {
        const auto value = source.value(key).toObject();
        if (CloudProtocol::validRegistration(key, value)) m_registrations.insert(key, value);
    }
    return !m_registrations.isEmpty();
}

QJsonObject CloudAccounts::registration() const { return m_registrations.value(m_provider).toObject(); }
QJsonObject CloudAccounts::credential() const
{
    const auto value = m_credentials.value(m_provider).toObject();
    return value.value(QStringLiteral("clientId")) == registration().value(QStringLiteral("clientId"))
        ? value : QJsonObject{};
}

bool CloudAccounts::connected(const QString &provider) const
{
    const auto token = m_credentials.value(provider).toObject();
    const auto config = m_registrations.value(provider).toObject();
    if (config.isEmpty() || token.value(QStringLiteral("clientId")) != config.value(QStringLiteral("clientId"))) return false;
    return secret(token.value(QStringLiteral("refreshToken")).toString())
        || (secret(token.value(QStringLiteral("accessToken")).toString()) && token.value(QStringLiteral("expiresAt")).toDouble() > now());
}

QVariantList CloudAccounts::accounts() const
{
    QVariantList result;
    for (const QString &key : providers) {
        const bool configured = !m_registrations.value(key).toObject().isEmpty();
        result.append(QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("title"), CloudProtocol::title(key)},
            {QStringLiteral("configured"), configured && m_storeReady}, {QStringLiteral("connected"), connected(key)},
            {QStringLiteral("status"), !m_storeReady ? tr("Archivio accessi non disponibile")
                : !configured ? tr("Accesso SSO non configurato in questa installazione")
                : connected(key) ? tr("Accesso salvato · verifica durante il collegamento") : tr("Da collegare")}});
    }
    return result;
}

QString CloudAccounts::currentPath() const
{
    QStringList parts{CloudProtocol::title(m_provider)};
    for (const auto &folder : m_folders) parts.append(folder.second);
    return parts.join(QStringLiteral(" / "));
}

void CloudAccounts::setLocked(bool value)
{
    if (m_locked == value) return;
    if (value) { cancel(); m_entries.clear(); m_cursor.clear(); m_folders.clear(); m_status.clear(); m_error.clear(); }
    m_locked = value;
    emit changed();
}

bool CloudAccounts::selectProvider(const QString &value)
{
    if (m_locked || !CloudProtocol::provider(value)) return false;
    cancel();
    reloadRegistrations();
    m_provider = value;
    m_folders.clear(); m_entries.clear(); m_cursor.clear();
    m_status = registration().isEmpty() ? tr("Accesso SSO non configurato in questa installazione.")
        : connected(value) ? tr("Accesso salvato. Aggiorna per verificare i file disponibili.") : tr("Accedi tramite la pagina ufficiale del servizio.");
    m_error.clear();
    emit changed();
    return true;
}

bool CloudAccounts::beginOperation()
{
    if (m_locked || m_busy || !CloudProtocol::provider(m_provider)) return false;
    if (!m_storeReady) { fail(tr("Archivio privato degli accessi non disponibile.")); return false; }
    if (registration().isEmpty()) { fail(tr("Accesso SSO non configurato in questa installazione.")); return false; }
    m_busy = true; m_error.clear(); m_progress = 0;
    emit changed();
    return true;
}

void CloudAccounts::clearAuthorization()
{
    m_authDeadline.stop(); m_poll.stop(); m_callback.close();
    for (QTcpSocket *socket : m_callback.findChildren<QTcpSocket *>()) { socket->abort(); socket->deleteLater(); }
    m_authPending = false; m_authorizationUrl = QUrl{}; m_userCode.clear(); m_callbackPort = 0;
    m_verifier.clear(); m_state.clear(); m_deviceCode.clear(); m_authExpires = 0;
    if (m_storeLock && m_storeLock->isLocked()) {
        QFile::remove(QDir(m_store.directory()).filePath(QStringLiteral("oauth-browser-request.json")));
        QFile::remove(QDir(m_store.directory()).filePath(QStringLiteral("oauth-browser-response.json")));
    }
}

void CloudAccounts::cancel()
{
    const bool active = m_busy || m_authPending;
    ++m_generation;
    m_deadline.stop(); clearAuthorization();
    if (m_reply) { m_reply->abort(); m_reply.clear(); }
    m_sink.reset(); m_upload.reset(); m_uploadUrl = QUrl{};
    m_busy = false; m_progress = 0;
    if (active) { m_status = tr("Operazione annullata."); m_error.clear(); }
    emit changed();
}

void CloudAccounts::fail(const QString &message)
{
    cancel();
    m_status.clear(); m_error = message;
    emit changed();
}

void CloudAccounts::complete(const QString &message)
{
    m_busy = false; m_error.clear(); m_status = message; m_progress = 0;
    m_sink.reset(); m_upload.reset(); m_uploadUrl = QUrl{};
    emit changed();
}

void CloudAccounts::request(const QByteArray &method, const QUrl &url, const QByteArray &body,
                           Headers headers, qint64 maximum, Completion completed,
                           QIODevice *upload, std::shared_ptr<QSaveFile> sink)
{
    if (m_locked || !CloudProtocol::secureUrl(url) || m_reply) { fail(tr("Richiesta al servizio non valida.")); return; }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    for (auto it = headers.cbegin(); it != headers.cend(); ++it) request.setRawHeader(it.key(), it.value());
    QNetworkReply *reply = upload ? m_network->sendCustomRequest(request, method, upload)
                                 : m_network->sendCustomRequest(request, method, body);
    m_reply = reply;
    reply->setReadBufferSize(256 * 1024);
    m_deadline.start(120000);
    const quint64 generation = m_generation;
    const auto result = std::make_shared<Result>();
    const auto count = std::make_shared<qint64>(0);
    const auto drain = [this, generation, reply, result, count, maximum, sink] {
        if (generation != m_generation || result->exceeded || result->transportError) return;
        while (reply->bytesAvailable() > 0) {
            const QByteArray bytes = reply->read(64 * 1024);
            *count += bytes.size();
            if (*count > maximum) { result->exceeded = true; reply->abort(); return; }
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (sink && status >= 200 && status < 300) {
                if (sink->write(bytes) != bytes.size()) { result->transportError = true; reply->abort(); return; }
            } else {
                if (result->body.size() + bytes.size() > CloudProtocol::maximumJsonBytes) { result->exceeded = true; reply->abort(); return; }
                result->body.append(bytes);
            }
        }
    };
    connect(reply, &QNetworkReply::readyRead, this, drain);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, generation](qint64 received, qint64 total) {
        if (generation != m_generation || total <= 0) return;
        const double value = std::clamp(double(received) / double(total), 0., 1.);
        if (qAbs(m_progress - value) >= .02) { m_progress = value; emit changed(); }
    });
    connect(reply, &QNetworkReply::finished, this, [this, generation, reply, result, drain, completed = std::move(completed)] {
        if (generation != m_generation) { reply->deleteLater(); return; }
        drain();
        result->status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result->transportError = result->transportError || reply->error() != QNetworkReply::NoError;
        for (const auto &header : reply->rawHeaderPairs()) result->headers.insert(header.first.toLower(), header.second);
        m_reply.clear(); m_deadline.stop(); reply->deleteLater();
        completed(*result);
    });
}

QString CloudAccounts::httpError(const Result &result) const
{
    if (result.exceeded) return tr("Il file o la risposta supera il limite consentito.");
    if (result.status == 401) return tr("Accesso scaduto o revocato. Accedi di nuovo.");
    if (result.status == 403) return tr("Il servizio non ha autorizzato questa operazione. Controlla i permessi dell’account.");
    if (result.status == 404) return tr("Il file non è più disponibile.");
    if (result.status == 409 || result.status == 412) return tr("Esiste già un file con questo nome. Scegli un altro nome; nessun file è stato sostituito.");
    if (result.status == 429) return tr("Troppe richieste al servizio. Attendi e riprova.");
    if (!result.status) return tr("Collegamento HTTPS non riuscito. Controlla la rete e riprova.");
    return tr("Il servizio ha rifiutato la richiesta (HTTP %1).").arg(result.status);
}

QUrl CloudAccounts::tokenUrl() const
{
    if (m_provider == QStringLiteral("dropbox")) return QUrl(QStringLiteral("https://api.dropboxapi.com/oauth2/token"));
    if (m_provider == QStringLiteral("googleDrive")) return QUrl(QStringLiteral("https://oauth2.googleapis.com/token"));
    return QUrl(QStringLiteral("https://login.microsoftonline.com/%1/oauth2/v2.0/token")
        .arg(registration().value(QStringLiteral("tenant")).toString(QStringLiteral("common"))));
}

CloudAccounts::Headers CloudAccounts::bearerHeaders() const
{
    return {{"Authorization", "Bearer " + credential().value(QStringLiteral("accessToken")).toString().toUtf8()}};
}

bool CloudAccounts::acceptTokens(const QJsonObject &body, bool refresh)
{
    const QString access = body.value(QStringLiteral("access_token")).toString();
    const QString refreshToken = body.value(QStringLiteral("refresh_token")).toString(
        refresh ? credential().value(QStringLiteral("refreshToken")).toString() : QString{});
    const double expires = body.value(QStringLiteral("expires_in")).toDouble();
    if (!secret(access) || (!refreshToken.isEmpty() && !secret(refreshToken)) || expires < 1 || expires > 31536000
        || body.value(QStringLiteral("token_type")).toString().compare(QStringLiteral("Bearer"), Qt::CaseInsensitive) != 0) {
        fail(tr("Il servizio ha restituito un accesso non valido.")); return false;
    }
    QJsonObject updated = m_credentials;
    updated.insert(m_provider, QJsonObject{{QStringLiteral("accessToken"), access}, {QStringLiteral("refreshToken"), refreshToken},
        {QStringLiteral("expiresAt"), double(now() + qint64(expires))},
        {QStringLiteral("clientId"), registration().value(QStringLiteral("clientId"))}});
    if (!m_store.save(updated)) { fail(tr("Impossibile salvare l’accesso nell’archivio privato.")); return false; }
    m_credentials = updated;
    emit changed();
    return true;
}

void CloudAccounts::ensureToken(std::function<void()> action, bool force)
{
    const auto saved = credential();
    if (!force && secret(saved.value(QStringLiteral("accessToken")).toString())
        && saved.value(QStringLiteral("expiresAt")).toDouble() > now() + 60) { action(); return; }
    const QString refresh = saved.value(QStringLiteral("refreshToken")).toString();
    if (!secret(refresh)) { fail(tr("Accedi al servizio prima di continuare.")); return; }
    QList<QPair<QString, QString>> values{{QStringLiteral("grant_type"), QStringLiteral("refresh_token")},
        {QStringLiteral("refresh_token"), refresh}, {QStringLiteral("client_id"), registration().value(QStringLiteral("clientId")).toString()}};
    const QString clientSecret = registration().value(QStringLiteral("clientSecret")).toString();
    if (m_provider == QStringLiteral("googleDrive") && !clientSecret.isEmpty()) values.append({QStringLiteral("client_secret"), clientSecret});
    request("POST", tokenUrl(), CloudProtocol::form(values), {{"Content-Type", "application/x-www-form-urlencoded"}},
        CloudProtocol::maximumJsonBytes, [this, action = std::move(action)](Result result) {
            if (result.status != 200 || result.transportError) { fail(httpError(result)); return; }
            if (acceptTokens(object(result.body), true)) action();
        });
}

void CloudAccounts::publishAuthorization()
{
    if (!CloudProtocol::browserUrl(m_provider, m_authorizationUrl)) { fail(tr("Pagina di autorizzazione non valida.")); return; }
    const QJsonObject handoff{{QStringLiteral("provider"), m_provider},
        {QStringLiteral("requestId"), m_state},
        {QStringLiteral("authorizationUrl"), m_authorizationUrl.toString(QUrl::FullyEncoded)},
        {QStringLiteral("callbackPort"), m_callbackPort}, {QStringLiteral("expiresAt"), double(m_authExpires)}};
    if (!CloudStore::privateWrite(QDir(m_store.directory()).filePath(QStringLiteral("oauth-browser-request.json")), json(handoff))) {
        fail(tr("Impossibile preparare l’autorizzazione privata.")); return;
    }
    emit changed();
    if (!m_tabletMode && !m_testNetwork) openAuthorizationBrowser();
}

void CloudAccounts::openAuthorizationBrowser()
{
    if (m_locked || !m_authPending || !CloudProtocol::browserUrl(m_provider, m_authorizationUrl)) return;
    if (m_tabletMode) { m_status = tr("Completa l’accesso nel browser del Mac con l’assistente SSH."); emit changed(); return; }
    if (!QDesktopServices::openUrl(m_authorizationUrl)) { m_status = tr("Impossibile aprire il browser. Usa il collegamento di autorizzazione."); emit changed(); }
}

void CloudAccounts::beginAuthorization()
{
    if (m_locked || m_busy) return;
    reloadRegistrations();
    if (!beginOperation()) return;
    m_authPending = true; m_authExpires = now() + 900; m_authDeadline.start(900000);
    m_state = CloudProtocol::randomToken();
    m_status = tr("Completa l’accesso nella pagina ufficiale del servizio.");
    const QString client = registration().value(QStringLiteral("clientId")).toString();
    if (m_provider == QStringLiteral("oneDrive")) {
        const QUrl url(QStringLiteral("https://login.microsoftonline.com/%1/oauth2/v2.0/devicecode")
            .arg(registration().value(QStringLiteral("tenant")).toString(QStringLiteral("common"))));
        request("POST", url, CloudProtocol::form({{QStringLiteral("client_id"), client}, {QStringLiteral("scope"), microsoftScope}}),
            {{"Content-Type", "application/x-www-form-urlencoded"}}, CloudProtocol::maximumJsonBytes, [this](Result result) {
                const auto body = object(result.body);
                const int lifetime = body.value(QStringLiteral("expires_in")).toInt();
                m_pollSeconds = body.value(QStringLiteral("interval")).toInt(5);
                m_deviceCode = body.value(QStringLiteral("device_code")).toString();
                m_userCode = body.value(QStringLiteral("user_code")).toString();
                m_authorizationUrl = QUrl(body.value(QStringLiteral("verification_uri")).toString());
                if (result.status != 200 || result.transportError || !secret(m_deviceCode) || m_userCode.isEmpty()
                    || m_userCode.size() > 64 || lifetime < 1 || lifetime > 1800 || m_pollSeconds < 1 || m_pollSeconds > 300) {
                    fail(httpError(result)); return;
                }
                m_authExpires = now() + lifetime; m_authDeadline.start(lifetime * 1000);
                publishAuthorization();
                if (m_authPending) m_poll.start(m_pollSeconds * 1000);
            });
        return;
    }
    m_verifier = CloudProtocol::randomToken();
    QUrl url(m_provider == QStringLiteral("googleDrive") ? QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")
                                                        : QStringLiteral("https://www.dropbox.com/oauth2/authorize"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("client_id"), client);
    query.addQueryItem(QStringLiteral("code_challenge"), CloudProtocol::challenge(m_verifier));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("state"), m_state);
    if (m_provider == QStringLiteral("googleDrive")) {
        if (!m_callback.listen(QHostAddress::LocalHost, 0)) { fail(tr("Impossibile aprire il ritorno locale per l’accesso Google.")); return; }
        m_callbackPort = m_callback.serverPort();
        query.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://127.0.0.1:%1/oauth/callback").arg(m_callbackPort));
        query.addQueryItem(QStringLiteral("scope"), googleScope);
        query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
        query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    } else {
        query.addQueryItem(QStringLiteral("token_access_type"), QStringLiteral("offline"));
        query.addQueryItem(QStringLiteral("scope"), QStringLiteral("files.metadata.read files.content.read files.content.write"));
    }
    url.setQuery(query); m_authorizationUrl = url;
    publishAuthorization();
    if (m_authPending && m_provider == QStringLiteral("dropbox")) m_poll.start(1000);
}

void CloudAccounts::submitAuthorizationCode(const QString &code)
{
    if (m_locked || !m_authPending || m_provider != QStringLiteral("dropbox") || m_reply) return;
    const QString value = code.trimmed();
    if (!secret(value) || value.size() > 4096 || value.contains(u' ')) { m_error = tr("Inserisci il codice fornito da Dropbox."); emit changed(); return; }
    exchangeCode(value);
}

void CloudAccounts::exchangeCode(const QString &code)
{
    QList<QPair<QString, QString>> values{{QStringLiteral("grant_type"), QStringLiteral("authorization_code")},
        {QStringLiteral("code"), code}, {QStringLiteral("client_id"), registration().value(QStringLiteral("clientId")).toString()},
        {QStringLiteral("code_verifier"), m_verifier}};
    if (m_provider == QStringLiteral("googleDrive")) {
        values.append({QStringLiteral("redirect_uri"), QStringLiteral("http://127.0.0.1:%1/oauth/callback").arg(m_callbackPort)});
        const QString clientSecret = registration().value(QStringLiteral("clientSecret")).toString();
        if (!clientSecret.isEmpty()) values.append({QStringLiteral("client_secret"), clientSecret});
    }
    request("POST", tokenUrl(), CloudProtocol::form(values), {{"Content-Type", "application/x-www-form-urlencoded"}},
        CloudProtocol::maximumJsonBytes, [this](Result result) {
            if (result.status != 200 || result.transportError) { fail(httpError(result)); return; }
            if (!acceptTokens(object(result.body), false)) return;
            clearAuthorization(); m_busy = false;
            refreshFiles();
        });
}

void CloudAccounts::pollDeviceCode()
{
    if (m_locked || !m_authPending || m_reply || now() >= m_authExpires) return;
    if (m_provider == QStringLiteral("dropbox")) {
        const QString path = QDir(m_store.directory()).filePath(QStringLiteral("oauth-browser-response.json"));
        if (QFileInfo::exists(path)) {
            bool ok = false;
            const auto response = object(CloudStore::privateRead(path, 8192, &ok));
            QFile::remove(path);
            if (ok && response.value(QStringLiteral("requestId")).toString() == m_state) {
                submitAuthorizationCode(response.value(QStringLiteral("code")).toString());
                if (m_reply) return;
            }
        }
        m_poll.start(1000);
        return;
    }
    request("POST", tokenUrl(), CloudProtocol::form({{QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code")},
        {QStringLiteral("device_code"), m_deviceCode}, {QStringLiteral("client_id"), registration().value(QStringLiteral("clientId")).toString()}}),
        {{"Content-Type", "application/x-www-form-urlencoded"}}, CloudProtocol::maximumJsonBytes, [this](Result result) {
            const auto body = object(result.body);
            if (result.status == 200 && !result.transportError) {
                if (!acceptTokens(body, false)) return;
                clearAuthorization(); m_busy = false; refreshFiles(); return;
            }
            const QString error = body.value(QStringLiteral("error")).toString();
            if (error == QStringLiteral("authorization_pending") || error == QStringLiteral("slow_down")) {
                if (error == QStringLiteral("slow_down")) m_pollSeconds += 5;
                if (m_pollSeconds <= 300 && now() < m_authExpires) { m_poll.start(m_pollSeconds * 1000); return; }
            }
            fail(error == QStringLiteral("authorization_declined") || error == QStringLiteral("access_denied")
                ? tr("Autorizzazione annullata nella pagina Microsoft.") : httpError(result));
        });
}

void CloudAccounts::acceptCallback()
{
    while (m_callback.hasPendingConnections()) {
        QTcpSocket *socket = m_callback.nextPendingConnection();
        if (!socket) continue;
        if (m_locked || !m_authPending || m_reply || m_callback.findChildren<QTcpSocket *>().size() > 4) { socket->abort(); socket->deleteLater(); continue; }
        auto bytes = std::make_shared<QByteArray>();
        const quint64 generation = m_generation;
        QTimer::singleShot(5000, socket, [socket] { socket->abort(); socket->deleteLater(); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, bytes, generation] {
            if (generation != m_generation || m_locked || !m_authPending) { socket->abort(); return; }
            bytes->append(socket->read(8193 - bytes->size()));
            if (bytes->size() > 8192) { socket->abort(); return; }
            if (!bytes->contains("\r\n\r\n")) return;
            const auto line = bytes->split('\n').value(0).trimmed().split(' ');
            const QUrl target = QUrl::fromEncoded(line.value(1));
            const QUrlQuery query(target);
            int codes = 0, states = 0;
            for (const auto &item : query.queryItems()) { if (item.first == QStringLiteral("code")) ++codes; if (item.first == QStringLiteral("state")) ++states; }
            const QString code = query.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
            const bool accepted = line.size() == 3 && line[0] == "GET" && target.path() == QStringLiteral("/oauth/callback")
                && target.host().isEmpty() && codes == 1 && states == 1 && !m_state.isEmpty()
                && query.queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded) == m_state && secret(code) && code.size() <= 4096;
            const QByteArray text = accepted ? QByteArray("Authorization received. You can return to Quaderno.") : QByteArray("Invalid authorization callback.");
            socket->write(QByteArray(accepted ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n")
                + "Content-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: "
                + QByteArray::number(text.size()) + "\r\n\r\n" + text);
            socket->disconnectFromHost();
            if (accepted) { m_state.clear(); m_callback.close(); exchangeCode(code); }
        });
    }
}

void CloudAccounts::refreshFiles()
{
    if (!beginOperation()) return;
    m_status = tr("Collegamento al servizio…");
    ensureToken([this] { listPage(false); });
}

void CloudAccounts::loadMore()
{
    if (m_cursor.isEmpty() || !beginOperation()) return;
    ensureToken([this] { listPage(true); });
}

QVariantMap CloudAccounts::entry(const QString &id) const
{
    for (const QVariant &item : m_entries) {
        const QVariantMap value = item.toMap();
        if (value.value(QStringLiteral("id")).toString() == id) return value;
    }
    return {};
}

void CloudAccounts::enterFolder(const QString &id)
{
    if (m_locked || m_busy || m_folders.size() >= 64) return;
    const auto selected = entry(id);
    if (!selected.value(QStringLiteral("folder")).toBool()) return;
    m_folders.append({id, selected.value(QStringLiteral("name")).toString()});
    m_entries.clear(); m_cursor.clear();
    refreshFiles();
}

void CloudAccounts::goUp()
{
    if (m_locked || m_busy || m_folders.isEmpty()) return;
    m_folders.removeLast(); m_entries.clear(); m_cursor.clear();
    refreshFiles();
}

void CloudAccounts::listPage(bool append)
{
    Headers headers = bearerHeaders();
    QUrl url;
    QByteArray method("GET"), body;
    if (m_provider == QStringLiteral("dropbox")) {
        method = "POST"; headers.insert("Content-Type", "application/json");
        url = QUrl(append ? QStringLiteral("https://api.dropboxapi.com/2/files/list_folder/continue")
                         : QStringLiteral("https://api.dropboxapi.com/2/files/list_folder"));
        body = append ? json({{QStringLiteral("cursor"), m_cursor}})
            : json({{QStringLiteral("path"), currentFolder()}, {QStringLiteral("recursive"), false},
                {QStringLiteral("include_deleted"), false}, {QStringLiteral("limit"), 200}});
    } else if (m_provider == QStringLiteral("googleDrive")) {
        url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
        QUrlQuery query;
        const QString parent = currentFolder().isEmpty() ? QStringLiteral("root") : currentFolder();
        if (!CloudProtocol::safeId(parent, true)) { fail(tr("Cartella cloud non valida.")); return; }
        query.addQueryItem(QStringLiteral("q"), QStringLiteral("trashed=false and '%1' in parents").arg(parent));
        query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("100"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("nextPageToken,files(id,name,mimeType,size)"));
        if (append) query.addQueryItem(QStringLiteral("pageToken"), m_cursor);
        url.setQuery(query);
    } else {
        if (append) {
            url = QUrl(m_cursor);
            if (!CloudProtocol::secureUrl(url) || url.host() != QStringLiteral("graph.microsoft.com")
                || !url.path().startsWith(QStringLiteral("/v1.0/"))) { fail(tr("Risposta del servizio non valida.")); return; }
        } else {
            const QString folder = currentFolder().isEmpty() ? QStringLiteral("root") : QStringLiteral("items/") + segment(currentFolder());
            url = QUrl(QStringLiteral("https://graph.microsoft.com/v1.0/me/drive/%1/children").arg(folder));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("$top"), QStringLiteral("100"));
            query.addQueryItem(QStringLiteral("$select"), QStringLiteral("id,name,size,file,folder"));
            url.setQuery(query);
        }
    }
    request(method, url, body, headers, CloudProtocol::maximumJsonBytes, [this, append](Result result) {
        if (result.status != 200 || result.transportError) { fail(httpError(result)); return; }
        if (!appendEntries(object(result.body), append)) { fail(tr("Risposta dei file cloud non valida o troppo grande.")); return; }
        complete(tr("%1 elementi disponibili. I trasferimenti creano nuove copie.").arg(m_entries.size()));
    });
}

bool CloudAccounts::appendEntries(const QJsonObject &body, bool append)
{
    const QString key = m_provider == QStringLiteral("dropbox") ? QStringLiteral("entries")
        : m_provider == QStringLiteral("googleDrive") ? QStringLiteral("files") : QStringLiteral("value");
    if (!body.value(key).isArray() || body.value(key).toArray().size() > 500) return false;
    QVariantList entries = append ? m_entries : QVariantList{};
    QSet<QString> existing;
    for (const QVariant &item : entries) existing.insert(item.toMap().value(QStringLiteral("id")).toString());
    for (const QJsonValue &item : body.value(key).toArray()) {
        const QJsonObject value = item.toObject();
        const QString name = value.value(QStringLiteral("name")).toString();
        const QString id = value.value(m_provider == QStringLiteral("dropbox") ? QStringLiteral("path_lower") : QStringLiteral("id")).toString();
        const QString mime = value.value(QStringLiteral("mimeType")).toString();
        const bool folder = m_provider == QStringLiteral("dropbox") ? value.value(QStringLiteral(".tag")).toString() == QStringLiteral("folder")
            : m_provider == QStringLiteral("googleDrive") ? mime == QStringLiteral("application/vnd.google-apps.folder")
            : value.value(QStringLiteral("folder")).isObject();
        if (!CloudProtocol::safeName(name) || !CloudProtocol::safeId(id, m_provider == QStringLiteral("googleDrive"))
            || (!folder && !CloudProtocol::supportedName(name)) || existing.contains(id)) continue;
        bool sizeKnown = false;
        const qint64 size = value.value(QStringLiteral("size")).toVariant().toLongLong(&sizeKnown);
        if (sizeKnown && size < 0) return false;
        const qint64 maximum = QFileInfo(name).suffix().compare(QStringLiteral("quaderno"), Qt::CaseInsensitive) == 0
            ? CloudProtocol::maximumNotebookBytes : CloudProtocol::maximumFileBytes;
        entries.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("folder"), folder},
            {QStringLiteral("size"), sizeKnown ? size : -1}, {QStringLiteral("mime"), mime},
            {QStringLiteral("canDownload"), !folder && (!sizeKnown || size <= maximum)}});
        existing.insert(id);
        if (entries.size() > 2000) return false;
    }
    QString cursor;
    if (m_provider == QStringLiteral("dropbox")) {
        if (body.value(QStringLiteral("has_more")).toBool()) {
            cursor = body.value(QStringLiteral("cursor")).toString();
            if (cursor.isEmpty()) return false;
        }
    } else if (m_provider == QStringLiteral("googleDrive")) cursor = body.value(QStringLiteral("nextPageToken")).toString();
    else cursor = body.value(QStringLiteral("@odata.nextLink")).toString();
    if (cursor.size() > 16384 || cursor.contains(QChar::Null)) return false;
    m_entries = entries; m_cursor = cursor;
    return true;
}

void CloudAccounts::downloadFile(const QString &id)
{
    const auto selected = entry(id);
    if (selected.isEmpty() || selected.value(QStringLiteral("folder")).toBool()
        || !selected.value(QStringLiteral("canDownload")).toBool() || !beginOperation()) return;
    const QString name = selected.value(QStringLiteral("name")).toString();
    const QString directory = QDir(m_store.directory()).filePath(QStringLiteral("downloads/")) + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!CloudStore::privateDirectory(QDir(m_store.directory()).filePath(QStringLiteral("downloads")))
        || !CloudStore::privateDirectory(directory)) { fail(tr("Impossibile preparare il download privato.")); return; }
    const QString path = QDir(directory).filePath(name);
    m_sink = std::make_shared<QSaveFile>(path);
    m_sink->setDirectWriteFallback(false);
    if (!m_sink->open(QIODevice::WriteOnly) || !m_sink->setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
        fail(tr("Impossibile salvare il file scaricato.")); return;
    }
    m_status = tr("Download di %1…").arg(name);
    ensureToken([this, selected, path, id] {
        QUrl url;
        Headers headers = bearerHeaders();
        if (m_provider == QStringLiteral("dropbox")) {
            url = QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/download"));
            headers.insert("Dropbox-API-Arg", CloudProtocol::asciiJson({{QStringLiteral("path"), id}}));
        } else if (m_provider == QStringLiteral("googleDrive"))
            url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(segment(id)));
        else url = QUrl(QStringLiteral("https://graph.microsoft.com/v1.0/me/drive/items/%1/content").arg(segment(id)));
        downloadRequest(url, headers, 0, selected, path);
    });
}

void CloudAccounts::downloadRequest(const QUrl &url, Headers headers, int redirects,
                                   const QVariantMap &selected, const QString &path)
{
    if (!CloudProtocol::downloadUrl(m_provider, url) || redirects > 4) { fail(tr("Destinazione del download non autorizzata.")); return; }
    const qint64 maximum = QFileInfo(path).suffix().compare(QStringLiteral("quaderno"), Qt::CaseInsensitive) == 0
        ? CloudProtocol::maximumNotebookBytes : CloudProtocol::maximumFileBytes;
    const QByteArray method = url.host() == QStringLiteral("content.dropboxapi.com") ? QByteArray("POST") : QByteArray("GET");
    request(method, url, {}, headers, maximum, [this, url, redirects, selected, path](Result result) {
        if (result.status >= 300 && result.status < 400 && result.headers.contains("location")) {
            const QUrl next = url.resolved(QUrl::fromEncoded(result.headers.value("location")));
            // A preauthenticated CDN URL never receives a provider bearer token,
            // even when the redirect returns to an API host later in the chain.
            downloadRequest(next, {}, redirects + 1, selected, path); return;
        }
        if (result.status != 200 || result.transportError || result.exceeded) { fail(httpError(result)); return; }
        const qint64 expected = selected.value(QStringLiteral("size")).toLongLong();
        if (m_sink && expected >= 0 && m_sink->size() != expected) { fail(tr("Il file remoto è cambiato. Aggiorna l’elenco e scaricalo di nuovo.")); return; }
        if (!m_sink || !m_sink->commit()) { fail(tr("Impossibile completare il salvataggio del download.")); return; }
        m_downloads.insert(path, QFileInfo(path).canonicalFilePath());
        const QString name = selected.value(QStringLiteral("name")).toString();
        complete(tr("Scaricato: %1. Conferma l’importazione in Quaderno.").arg(name));
        emit downloaded(QUrl::fromLocalFile(path), name);
    }, nullptr, m_sink);
}

bool CloudAccounts::discardDownload(const QUrl &url)
{
    if (!url.isLocalFile()) return false;
    const QString path = url.toLocalFile();
    const auto owned = m_downloads.constFind(path);
    if (owned == m_downloads.cend()) return false;
    const QFileInfo file(path);
    const QFileInfo parent(file.absolutePath());
    const QString expectedParent = QFileInfo(owned.value()).absolutePath();
    // Only this instance's completed staging file can be removed. In
    // particular, replacing any directory with a link cannot redirect cleanup.
    if (file.isSymLink() || parent.isSymLink() || parent.canonicalFilePath() != expectedParent) return false;
    if (file.exists() && (!file.isFile() || file.canonicalFilePath() != owned.value() || !QFile::remove(path))) return false;
    m_downloads.remove(path);
    QDir().rmdir(parent.absoluteFilePath()); // Removes only an empty UUID folder.
    return true;
}

void CloudAccounts::uploadFile(const QUrl &url, const QString &name)
{
    if (m_locked || m_busy || !url.isLocalFile()) return;
    const QFileInfo info(url.toLocalFile());
    const QString filename = name.isEmpty() ? info.fileName() : name;
    const qint64 maximum = QFileInfo(filename).suffix().compare(QStringLiteral("quaderno"), Qt::CaseInsensitive) == 0
        ? CloudProtocol::maximumNotebookBytes : CloudProtocol::maximumFileBytes;
    if (!CloudProtocol::supportedName(filename) || !info.isFile() || info.isSymLink() || info.size() < 0 || info.size() > maximum) {
        fail(tr("Scegli un file supportato: massimo 100 MiB, oppure 64 MiB per un quaderno.")); return;
    }
    if (!beginOperation()) return;
    m_upload = std::make_unique<QFile>(info.absoluteFilePath());
    if (!m_upload->open(QIODevice::ReadOnly)) { fail(tr("Impossibile leggere il file da caricare.")); return; }
    m_uploadName = filename; m_uploadSize = m_upload->size(); m_uploadOffset = 0;
    if (m_uploadSize != info.size()) { fail(tr("Il file è cambiato. Selezionalo di nuovo.")); return; }
    m_status = tr("Caricamento di una nuova copia di %1…").arg(filename);
    ensureToken([this] { uploadRequest(); });
}

void CloudAccounts::uploadRequest()
{
    Headers headers = bearerHeaders();
    if (m_provider == QStringLiteral("dropbox")) {
        const QString path = currentFolder() + u'/' + m_uploadName;
        headers.insert("Content-Type", "application/octet-stream");
        headers.insert("Content-Length", QByteArray::number(m_uploadSize));
        headers.insert("Dropbox-API-Arg", CloudProtocol::asciiJson({{QStringLiteral("path"), path},
            {QStringLiteral("mode"), QStringLiteral("add")}, {QStringLiteral("autorename"), true},
            {QStringLiteral("strict_conflict"), true}, {QStringLiteral("mute"), false}}));
        request("POST", QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/upload")), {}, headers,
            CloudProtocol::maximumJsonBytes, [this](Result result) {
                if (result.status != 200 || result.transportError) { fail(httpError(result)); return; }
                complete(tr("Nuova copia caricata. Aggiorna l’elenco per visualizzarla."));
            }, m_upload.get());
        return;
    }
    headers.insert("Content-Type", "application/json; charset=utf-8");
    QUrl url;
    QJsonObject metadata;
    if (m_provider == QStringLiteral("googleDrive")) {
        url = QUrl(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable"));
        metadata = {{QStringLiteral("name"), m_uploadName}, {QStringLiteral("parents"), QJsonArray{
            currentFolder().isEmpty() ? QStringLiteral("root") : currentFolder()}}};
        headers.insert("X-Upload-Content-Type", CloudProtocol::mimeType(m_uploadName).toUtf8());
        headers.insert("X-Upload-Content-Length", QByteArray::number(m_uploadSize));
    } else {
        const QString folder = currentFolder().isEmpty() ? QStringLiteral("root") : QStringLiteral("items/") + segment(currentFolder());
        url = QUrl(QStringLiteral("https://graph.microsoft.com/v1.0/me/drive/%1:/%2:/createUploadSession")
            .arg(folder, segment(m_uploadName)));
        metadata = {{QStringLiteral("item"), QJsonObject{{QStringLiteral("@microsoft.graph.conflictBehavior"), QStringLiteral("fail")},
            {QStringLiteral("name"), m_uploadName}}}};
    }
    request("POST", url, json(metadata), headers, CloudProtocol::maximumJsonBytes, [this](Result result) {
        if ((result.status != 200 && result.status != 201) || result.transportError) { fail(httpError(result)); return; }
        m_uploadUrl = m_provider == QStringLiteral("googleDrive") ? QUrl::fromEncoded(result.headers.value("location"))
            : QUrl(object(result.body).value(QStringLiteral("uploadUrl")).toString());
        if (!CloudProtocol::uploadUrl(m_provider, m_uploadUrl)) { fail(tr("Sessione di caricamento non autorizzata.")); return; }
        uploadNextChunk();
    });
}

void CloudAccounts::uploadNextChunk()
{
    if (!m_upload || m_upload->size() != m_uploadSize || !m_upload->seek(m_uploadOffset)) {
        fail(tr("Il file è cambiato durante il caricamento.")); return;
    }
    const qint64 length = std::min(CloudProtocol::uploadChunkBytes, m_uploadSize - m_uploadOffset);
    const QByteArray body = m_upload->read(length);
    if (body.size() != length) { fail(tr("Impossibile leggere il file da caricare.")); return; }
    Headers headers = m_provider == QStringLiteral("googleDrive") ? bearerHeaders() : Headers{};
    headers.insert("Content-Type", CloudProtocol::mimeType(m_uploadName).toUtf8());
    headers.insert("Content-Length", QByteArray::number(length));
    headers.insert("Content-Range", m_uploadSize == 0 ? QByteArray("bytes */0")
        : "bytes " + QByteArray::number(m_uploadOffset) + '-' + QByteArray::number(m_uploadOffset + length - 1) + '/' + QByteArray::number(m_uploadSize));
    request("PUT", m_uploadUrl, body, headers, CloudProtocol::maximumJsonBytes, [this, length](Result result) {
        const qint64 next = m_uploadOffset + length;
        if ((result.status == 200 || result.status == 201) && !result.transportError && next == m_uploadSize) {
            complete(tr("Nuova copia caricata. Aggiorna l’elenco per visualizzarla.")); return;
        }
        bool accepted = false;
        if (m_provider == QStringLiteral("googleDrive") && result.status == 308) {
            accepted = result.headers.value("range") == "bytes=0-" + QByteArray::number(next - 1);
        } else if (m_provider == QStringLiteral("oneDrive") && result.status == 202) {
            const auto ranges = object(result.body).value(QStringLiteral("nextExpectedRanges")).toArray();
            accepted = !ranges.isEmpty() && ranges.first().toString().startsWith(QString::number(next) + u'-');
        }
        if (!accepted || result.transportError || next <= m_uploadOffset || next >= m_uploadSize) { fail(httpError(result)); return; }
        m_uploadOffset = next; m_progress = double(next) / double(m_uploadSize); emit changed();
        uploadNextChunk();
    });
}

void CloudAccounts::refreshConnection()
{
    if (!beginOperation()) return;
    ensureToken([this] { complete(tr("Accesso aggiornato. Puoi aggiornare l’elenco dei file.")); }, true);
}

void CloudAccounts::disconnectAccount(bool revoke)
{
    if (m_locked || m_busy || !CloudProtocol::provider(m_provider) || !m_storeReady) return;
    const auto removeLocal = [this] {
        QJsonObject updated = m_credentials; updated.remove(m_provider);
        if (!m_store.save(updated)) { fail(tr("Impossibile rimuovere l’accesso dall’archivio privato.")); return; }
        m_credentials = updated; m_entries.clear(); m_cursor.clear(); m_folders.clear();
        complete(tr("Accesso rimosso da questo dispositivo."));
    };
    if (!revoke) { removeLocal(); return; }
    if (m_provider == QStringLiteral("oneDrive")) { fail(tr("Per revocare Microsoft, gestisci le autorizzazioni nella pagina dell’account. Puoi rimuovere qui l’accesso locale.")); return; }
    if (!beginOperation()) return;
    ensureToken([this, removeLocal] {
        QUrl url;
        Headers headers;
        QByteArray body;
        if (m_provider == QStringLiteral("dropbox")) {
            url = QUrl(QStringLiteral("https://api.dropboxapi.com/2/auth/token/revoke")); headers = bearerHeaders();
        } else {
            url = QUrl(QStringLiteral("https://oauth2.googleapis.com/revoke"));
            const auto saved = credential();
            const QString refresh = saved.value(QStringLiteral("refreshToken")).toString();
            body = CloudProtocol::form({{QStringLiteral("token"), refresh.isEmpty() ? saved.value(QStringLiteral("accessToken")).toString() : refresh}});
            headers.insert("Content-Type", "application/x-www-form-urlencoded");
        }
        request("POST", url, body, headers, CloudProtocol::maximumJsonBytes, [this, removeLocal](Result result) {
            if (result.status != 200 || result.transportError) { fail(tr("Revoca non confermata dal servizio. Puoi rimuovere soltanto l’accesso locale.")); return; }
            removeLocal();
        });
    });
}
