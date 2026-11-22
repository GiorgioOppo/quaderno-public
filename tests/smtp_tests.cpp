#include <QtTest>
#include <QHostAddress>
#include <QSslConfiguration>
#include <QSslKey>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUuid>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <memory>

#include "smtpclient.h"

namespace {
struct ServerIdentity {
    QSslCertificate certificate{};
    QSslKey key{};
};

ServerIdentity localIdentity()
{
    const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1"), EVP_PKEY_free);
    const std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
    if (!key || !cert)
        return {};
    X509_NAME *name = X509_get_subject_name(cert.get());
    if (!name || !X509_set_version(cert.get(), 2) || !ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1)
        || !X509_gmtime_adj(X509_getm_notBefore(cert.get()), -120)
        || !X509_gmtime_adj(X509_getm_notAfter(cert.get()), 3600)
        || !X509_set_pubkey(cert.get(), key.get())
        || !X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char *>("127.0.0.1"), -1, -1, 0)
        || !X509_set_issuer_name(cert.get(), name)) {
        return {};
    }
    const std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> san(
        X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, "IP:127.0.0.1"), X509_EXTENSION_free);
    if (!san || !X509_add_ext(cert.get(), san.get(), -1) || !X509_sign(cert.get(), key.get(), EVP_sha256()))
        return {};
    const std::unique_ptr<BIO, decltype(&BIO_free)> certificatePem(BIO_new(BIO_s_mem()), BIO_free);
    const std::unique_ptr<BIO, decltype(&BIO_free)> keyPem(BIO_new(BIO_s_mem()), BIO_free);
    if (!certificatePem || !keyPem || !PEM_write_bio_X509(certificatePem.get(), cert.get())
        || !PEM_write_bio_PrivateKey(keyPem.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
        return {};
    }
    char *certData = nullptr;
    char *keyData = nullptr;
    const long certLength = BIO_get_mem_data(certificatePem.get(), &certData);
    const long keyLength = BIO_get_mem_data(keyPem.get(), &keyData);
    return {QSslCertificate(QByteArray(certData, certLength)), QSslKey(QByteArray(keyData, keyLength), QSsl::Ec)};
}

class ScopedTrust
{
public:
    explicit ScopedTrust(const QSslCertificate &cert) : m_original(QSslConfiguration::defaultConfiguration())
    {
        auto settings = m_original;
        auto authorities = settings.caCertificates();
        authorities.append(cert);
        settings.setCaCertificates(authorities);
        QSslConfiguration::setDefaultConfiguration(settings);
    }
    ~ScopedTrust() { QSslConfiguration::setDefaultConfiguration(m_original); }

private:
    const QSslConfiguration m_original;
};

class LocalRelay : public QTcpServer
{
public:
    enum Mode { StartTls, Implicit, NoTls };
    LocalRelay(Mode mode, const ServerIdentity &identity) : m_mode(mode), m_identity(identity) {}
    ~LocalRelay() override
    {
        if (m_socket) {
            disconnect(m_socket, nullptr, this, nullptr);
            m_socket->abort();
        }
    }
    QList<QByteArray> clearCommands;
    QList<QByteArray> encryptedCommands;
    QByteArray delivered;
    QString relayError;
    bool holdAcknowledgement = false;

protected:
    void incomingConnection(qintptr descriptor) override
    {
        m_socket = new QSslSocket(this);
        if (!m_socket->setSocketDescriptor(descriptor))
            return;
        m_socket->setLocalCertificate(m_identity.certificate);
        m_socket->setPrivateKey(m_identity.key);
        m_socket->setPeerVerifyMode(QSslSocket::VerifyNone);
        connect(m_socket, &QSslSocket::errorOccurred, this, [this] {
            relayError = m_socket->errorString();
        });
        const auto greet = [this] { m_socket->write("220-local relay\r\n220 welcome\r\n"); };
        connect(m_socket, &QSslSocket::readyRead, this, [this] { read(); });
        if (m_mode == Implicit) {
            connect(m_socket, &QSslSocket::encrypted, this, greet);
            m_socket->startServerEncryption();
        } else {
            greet();
        }
    }

private:
    void read()
    {
        m_buffer += m_socket->readAll();
        while (true) {
            if (m_data) {
                const qsizetype end = m_buffer.indexOf("\r\n.\r\n");
                if (end == -1)
                    return;
                delivered = m_buffer.left(end + 2);
                m_buffer.remove(0, end + 5);
                m_data = false;
                if (!holdAcknowledgement)
                    m_socket->write("250 queued\r\n");
            }
            const qsizetype end = m_buffer.indexOf("\r\n");
            if (end == -1)
                return;
            const QByteArray command = m_buffer.left(end);
            m_buffer.remove(0, end + 2);
            const bool encrypted = m_socket->isEncrypted();
            (encrypted ? encryptedCommands : clearCommands).append(command);
            if (command.startsWith("EHLO ")) {
                if (encrypted)
                    m_socket->write("250-local relay\r\n250-SIZE 25000000\r\n250 AUTH PLAIN\r\n");
                else if (m_mode == StartTls)
                    m_socket->write("250-local relay\r\n250 STARTTLS\r\n");
                else
                    m_socket->write("250-local relay\r\n250 AUTH PLAIN\r\n");
            } else if (command == "STARTTLS") {
                m_socket->write("220 begin TLS\r\n");
                m_socket->flush();
                m_socket->startServerEncryption();
            } else if (command.startsWith("AUTH PLAIN ")) {
                m_socket->write("235 authenticated\r\n");
            } else if (command.startsWith("MAIL FROM:") || command.startsWith("RCPT TO:")) {
                m_socket->write("250 accepted\r\n");
            } else if (command == "DATA") {
                m_data = true;
                m_socket->write("354 send content\r\n");
            } else if (command == "QUIT") {
                m_socket->write("221 bye\r\n");
            }
        }
    }

    const Mode m_mode;
    const ServerIdentity m_identity;
    QSslSocket *m_socket = nullptr;
    QByteArray m_buffer;
    bool m_data = false;
};

SmtpClient::Configuration configuration(quint16 port, bool implicit = false)
{
    return {QStringLiteral("127.0.0.1"), port, implicit ? QStringLiteral("tls") : QStringLiteral("starttls"),
            QStringLiteral("test-user"), QStringLiteral("test-password"), QStringLiteral("writer@example.test")};
}

SmtpClient::Message letter(const QString &path)
{
    return {QStringLiteral("reader+pdf@example.test"), QStringLiteral("Caffè e PDF"), QStringLiteral("Testo\né"),
            path, QStringLiteral("Caffè.pdf")};
}

QByteArray examplePdf()
{
    QByteArray pdf = "%PDF-1.7\n";
    QByteArray block;
    for (int i = 0; i < 256; ++i)
        block.append(char(i));
    for (int i = 0; i < 600; ++i)
        pdf += block;
    return pdf;
}

QString buildAttachment(const QTemporaryDir &folder)
{
    const QString path = folder.filePath(QStringLiteral("test.pdf"));
    const QByteArray contents = examplePdf();
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() ? path : QString{};
}
} // namespace

class SmtpTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The test is both an SMTP client and a TLS server. The OpenSSL
        // backend lets the fixture use an ephemeral identity entirely in RAM.
        QVERIFY(QSslSocket::setActiveBackend(QStringLiteral("openssl")));
        QVERIFY(QSslSocket::supportsSsl());
    }

    void validSubmission_data()
    {
        QTest::addColumn<bool>("implicit");
        QTest::newRow("starttls") << false;
        QTest::newRow("implicit-tls") << true;
    }

    void validSubmission()
    {
        QFETCH(bool, implicit);
        const auto identity = localIdentity();
        QVERIFY(!identity.certificate.isNull());
        QVERIFY(!identity.key.isNull());
        const ScopedTrust trusted(identity.certificate);
        LocalRelay server(implicit ? LocalRelay::Implicit : LocalRelay::StartTls, identity);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QTemporaryDir folder;
        const auto path = buildAttachment(folder);
        QVERIFY(!path.isEmpty());
        SmtpClient client;
        const QSignalSpy done(&client, &SmtpClient::finished);
        client.start(configuration(server.serverPort(), implicit), letter(path));
        QTRY_VERIFY2_WITH_TIMEOUT(done.size() == 1, qPrintable(server.relayError), 8000);
        QCOMPARE(done.first().first().toBool(), true);
        if (implicit) {
            QVERIFY(server.clearCommands.isEmpty());
        } else {
            QCOMPARE(server.clearCommands, (QList<QByteArray>{"EHLO localhost", "STARTTLS"}));
        }
        QVERIFY(server.encryptedCommands.size() >= 5);
        QCOMPARE(server.encryptedCommands.at(0), "EHLO localhost");
        QCOMPARE(server.encryptedCommands.at(1), "AUTH PLAIN " + QByteArray("\0test-user\0test-password", 24).toBase64());
        QCOMPARE(server.encryptedCommands.at(2), "MAIL FROM:<writer@example.test> SIZE=" + QByteArray::number(server.delivered.size()));
        QCOMPARE(server.encryptedCommands.at(3), "RCPT TO:<reader+pdf@example.test>");
        QCOMPARE(server.encryptedCommands.at(4), "DATA");
        QVERIFY(server.delivered.contains("Subject: =?UTF-8?B?" + QStringLiteral("Caffè e PDF").toUtf8().toBase64() + "?="));
        QVERIFY(server.delivered.contains("filename*=UTF-8''Caff%C3%A8.pdf"));
        const qsizetype attachment = server.delivered.indexOf("Content-Type: application/pdf\r\n");
        QVERIFY(attachment >= 0);
        const qsizetype start = server.delivered.indexOf("\r\n\r\n", attachment) + 4;
        const qsizetype end = server.delivered.indexOf("\r\n--quaderno-", start);
        QVERIFY(start > attachment && end > start);
        const auto attachmentBytes = server.delivered.mid(start, end - start);
        for (const auto &line : attachmentBytes.split('\n'))
            QVERIFY(line.trimmed().size() <= 76);
        QCOMPARE(QByteArray::fromBase64(attachmentBytes), examplePdf());
        QVERIFY(!client.active());
    }

    void refusesUntrustedCertificate()
    {
        const auto identity = localIdentity();
        QVERIFY(!identity.certificate.isNull());
        LocalRelay server(LocalRelay::Implicit, identity);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QTemporaryDir folder;
        SmtpClient client;
        const QSignalSpy done(&client, &SmtpClient::finished);
        client.start(configuration(server.serverPort(), true), letter(buildAttachment(folder)));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 8000);
        QCOMPARE(done.first().first().toBool(), false);
        QVERIFY(server.encryptedCommands.isEmpty());
        QVERIFY(!client.active());
    }

    void refusesDowngrade()
    {
        LocalRelay server(LocalRelay::NoTls, {});
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QTemporaryDir folder;
        SmtpClient client;
        const QSignalSpy done(&client, &SmtpClient::finished);
        client.start(configuration(server.serverPort()), letter(buildAttachment(folder)));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 3000);
        QCOMPARE(done.first().first().toBool(), false);
        QCOMPARE(server.clearCommands, (QList<QByteArray>{"EHLO localhost"}));
        QVERIFY(server.encryptedCommands.isEmpty());
    }

    void cancellationAfterDataReportsUnknownOutcome()
    {
        const auto identity = localIdentity();
        QVERIFY(!identity.certificate.isNull());
        const ScopedTrust trusted(identity.certificate);
        LocalRelay server(LocalRelay::StartTls, identity);
        server.holdAcknowledgement = true;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QTemporaryDir folder;
        SmtpClient client;
        const QSignalSpy done(&client, &SmtpClient::finished);
        client.start(configuration(server.serverPort()), letter(buildAttachment(folder)));
        QTRY_VERIFY_WITH_TIMEOUT(!server.delivered.isEmpty(), 8000);
        client.cancel();
        QCOMPARE(done.size(), 1);
        QCOMPARE(done.first().first().toBool(), false);
        QVERIFY(done.first().at(1).toString().contains(QStringLiteral("confermato")));
        QVERIFY(!client.active());
    }

    void refusesInjectedRecipientsAndOversizeAttachments()
    {
        QVERIFY(!SmtpClient::validMailbox(QStringLiteral("user@example.test\r\nRCPT TO:<other@example.test>")));
        QVERIFY(!SmtpClient::validMailbox(QStringLiteral("user@example.test,other@example.test")));
        QVERIFY(!SmtpClient::validMailbox(QStringLiteral("user@\u212a")));
        const QTemporaryDir folder;
        const auto path = buildAttachment(folder);
        QVERIFY(!path.isEmpty());
        QVERIFY(QFile(path).resize(SmtpClient::maximumAttachmentSize + 1));
        SmtpClient client;
        const QSignalSpy done(&client, &SmtpClient::finished);
        client.start(configuration(465, true), letter(path));
        QCOMPARE(done.size(), 1);
        QCOMPARE(done.first().first().toBool(), false);
        QVERIFY(done.first().at(1).toString().contains(QStringLiteral("15 MiB")));
        QVERIFY(!client.active());
    }
};

QTEST_GUILESS_MAIN(SmtpTests)
#include "smtp_tests.moc"
