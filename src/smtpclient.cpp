#include "smtpclient.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHostAddress>
#include <QLocale>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QUrl>
#include <QUuid>
#include <utility>

namespace {
constexpr int stepTimeoutMs = 30000;
constexpr int deliveryTimeoutMs = 300000;
constexpr qsizetype maximumReplyBytes = 65536;
constexpr qsizetype maximumReplyLineSize = 1024;
constexpr auto crlf = "\r\n";
constexpr qsizetype bytesPerMimeLine = 57;

bool plainText(const QString &text, qsizetype limit)
{
    if (text.size() > limit || !text.isValidUtf16())
        return false;
    for (const QChar c : text)
        if (c.category() == QChar::Other_Control || c == QChar::LineSeparator || c == QChar::ParagraphSeparator)
            return false;
    return true;
}

QByteArray encodeMimeLines(const QByteArray &bytes)
{
    const auto encoded = bytes.toBase64();
    QByteArray output;
    constexpr qsizetype columns = 76;
    output.reserve(encoded.size() + 2 * ((encoded.size() + columns - 1) / columns));
    for (qsizetype i = 0; i < encoded.size(); i += columns) {
        output.append(encoded.constData() + i, qMin(columns, encoded.size() - i));
        output += crlf;
    }
    return output;
}

QByteArray encodedSubject(const QString &subject)
{
    const QByteArray utf8 = subject.toUtf8();
    QByteArray header;
    for (qsizetype start = 0; start < utf8.size();) {
        qsizetype end = qMin(start + 42, utf8.size());
        while (end < utf8.size() && (quint8(utf8.at(end)) & 0xc0) == 0x80)
            --end; // Don't split an encoded word in the middle of a UTF-8 character.
        if (start != 0)
            header += "\r\n ";
        header += "=?UTF-8?B?" + utf8.mid(start, end - start).toBase64() + "?=";
        start = end;
    }
    return header;
}

QByteArray mimeHeaders(const SmtpClient::Configuration &settings, const SmtpClient::Message &message,
                       const QByteArray &boundary)
{
    const auto sender = settings.sender.toLatin1();
    const auto senderDomain = sender.mid(sender.indexOf('@') + 1);
    const auto id = QUuid::createUuid().toByteArray(QUuid::Id128);
    const auto date = QLocale::c().toString(QDateTime::currentDateTimeUtc(),
        QStringLiteral("ddd, dd MMM yyyy HH:mm:ss")).toLatin1();
    QByteArray body = message.text.toUtf8();
    body.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n");
    static const QRegularExpression asciiFilename(QStringLiteral("\\A[A-Za-z0-9 ._()-]+\\z"));
    QByteArray filename = "filename=\"Quaderno.pdf\"";
    if (asciiFilename.match(message.attachmentName).hasMatch()) {
        filename = "filename=\"" + message.attachmentName.toLatin1() + '"';
    } else {
        filename += ";\r\n filename*=UTF-8''" + QUrl::toPercentEncoding(message.attachmentName);
    }

    return "Date: " + date + " +0000\r\n"
        "From: <" + sender + ">\r\n"
        "To: <" + message.recipient.toLatin1() + ">\r\n"
        "Message-ID: <" + id + '@' + senderDomain + ">\r\n"
        "Subject: " + encodedSubject(message.subject) + "\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n--" + boundary + "\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n" + encodeMimeLines(body) + "\r\n--" + boundary + "\r\n"
        "Content-Type: application/pdf\r\n"
        "Content-Disposition: attachment; " + filename + "\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n";
}

QStringList capabilityArguments(const QStringList &lines, const QString &name)
{
    // The first EHLO line is the greeting, not an extension.
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QString line = lines.at(i).simplified().toUpper();
        if (line == name)
            return {QString{}};
        if (line.startsWith(name + u' ') || line.startsWith(name + u'='))
            return line.mid(name.size() + 1).split(u' ', Qt::SkipEmptyParts);
    }
    return {};
}
} // namespace

SmtpClient::SmtpClient(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_deadline.setSingleShot(true);
    m_socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket.setReadBufferSize(maximumReplyBytes);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        connectionLost(tr("Il server SMTP non ha risposto in tempo."));
    });
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        connectionLost(tr("L’invio SMTP ha superato il tempo massimo."));
    });
    connect(&m_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        connectionLost(tr("Connessione al server SMTP interrotta."));
    });
    connect(&m_socket, &QSslSocket::disconnected, this, [this] {
        connectionLost(tr("Il server SMTP ha chiuso la connessione."));
    });
    connect(&m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &) {
        connectionLost(tr("Impossibile verificare il certificato del server SMTP."));
    });
    connect(&m_socket, &QSslSocket::encrypted, this, [this] {
        if (m_state == State::TlsHandshake)
            command("EHLO localhost\r\n", State::SecureEhlo);
    });
    connect(&m_socket, &QSslSocket::readyRead, this, &SmtpClient::receive);
    connect(&m_socket, &QSslSocket::encryptedBytesWritten, this, [this](qint64 written) {
        if (m_state == State::Upload && written > 0) {
            m_timeout.start(stepTimeoutMs);
            upload();
        }
    });
}

SmtpClient::~SmtpClient()
{
    disconnect(&m_socket, nullptr, this, nullptr);
    m_socket.abort();
}

QString SmtpClient::normalizedHost(const QString &input)
{
    const QString host = input.trimmed();
    if (host.isEmpty() || !plainText(host, 253))
        return {};
    QHostAddress address;
    if (address.setAddress(host) && address.scopeId().isEmpty())
        return address.toString();
    const auto ascii = QUrl::toAce(host);
    static const QRegularExpression dnsName(QStringLiteral(
        "\\A[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?"
        "(?:\\.[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)*\\z"));
    const QString name = QString::fromLatin1(ascii).toLower();
    return name.size() <= 253 && dnsName.match(name).hasMatch() ? name : QString{};
}

bool SmtpClient::validMailbox(const QString &address)
{
    if (address.size() > 254 || !plainText(address, 254))
        return false;
    const qsizetype at = address.indexOf(u'@');
    if (at < 1 || at > 64 || at != address.lastIndexOf(u'@'))
        return false;
    static const QRegularExpression dotAtom(QStringLiteral(
        "\\A[A-Za-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[A-Za-z0-9!#$%&'*+/=?^_`{|}~-]+)*\\z"));
    const QString domain = address.mid(at + 1);
    return dotAtom.match(address.left(at)).hasMatch() && !domain.isEmpty()
        && QString::fromLatin1(domain.toLatin1()) == domain
        && normalizedHost(domain) == domain.toLower() && !domain.contains(u':');
}

bool SmtpClient::validConfiguration(const Configuration &settings)
{
    return !settings.host.isEmpty() && normalizedHost(settings.host) == settings.host
        && settings.port >= 1 && settings.port <= 65535
        && (settings.security == QStringLiteral("starttls") || settings.security == QStringLiteral("tls"))
        && validMailbox(settings.sender)
        && plainText(settings.username, 254)
        && plainText(settings.password, 512)
        && settings.username.isEmpty() == settings.password.isEmpty();
}

bool SmtpClient::validMessage(const Message &message)
{
    return validMailbox(message.recipient)
        && plainText(message.subject, 200)
        && message.text.size() <= 8192 && message.text.isValidUtf16() && !message.text.contains(QChar::Null)
        && plainText(message.attachmentName, 120) && message.attachmentName.toUtf8().size() <= 256
        && QFileInfo(message.attachmentName).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0
        && QFileInfo(message.attachmentName).fileName() == message.attachmentName
        && !message.attachmentName.contains(u'\\');
}

bool SmtpClient::active() const { return m_state != State::Idle; }

void SmtpClient::start(const Configuration &configuration, const Message &message)
{
    if (active())
        return;
    if (!validConfiguration(configuration) || !validMessage(message)) {
        emit finished(false, tr("Controlla i dati SMTP e i campi dell’email."));
        return;
    }
    if (!QSslSocket::supportsSsl()) {
        emit finished(false, tr("TLS non è disponibile su questo dispositivo."));
        return;
    }
    const QFileInfo source(message.attachmentPath);
    if (!source.isFile() || source.isSymLink() || source.size() <= 0
        || source.size() > maximumAttachmentSize) {
        emit finished(false, tr("Il PDF non è disponibile o supera il limite di 15 MiB."));
        return;
    }
    m_attachment.setFileName(message.attachmentPath);
    if (!m_attachment.open(QIODevice::ReadOnly) || m_attachment.size() <= 0
        || m_attachment.size() > maximumAttachmentSize || m_attachment.peek(5) != "%PDF-") {
        m_attachment.close();
        emit finished(false, tr("Impossibile leggere il PDF da allegare."));
        return;
    }

    m_socket.abort();
    m_configuration = configuration;
    m_recipient = message.recipient.toLatin1();
    m_remaining = m_attachment.size();
    const auto boundary = QByteArray("quaderno-") + QUuid::createUuid().toByteArray(QUuid::Id128);
    m_prefix = mimeHeaders(configuration, message, boundary);
    m_suffix = "--" + boundary + "--\r\n";
    m_messageBytes = m_prefix.size() + m_suffix.size() + 4 * ((m_remaining + 2) / 3)
        + 2 * ((m_remaining + bytesPerMimeLine - 1) / bytesPerMimeLine);
    m_advertisesSize = false;
    m_input.clear();
    m_replyLines.clear();
    m_replyCode = 0;
    auto tls = QSslConfiguration::defaultConfiguration();
    tls.setProtocol(QSsl::TlsV1_2OrLater);
    m_socket.setSslConfiguration(tls);
    m_socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket.setPeerVerifyName(configuration.host);
    m_state = State::Greeting;
    m_timeout.start(stepTimeoutMs);
    m_deadline.start(deliveryTimeoutMs);
    if (configuration.security == QStringLiteral("tls"))
        m_socket.connectToHostEncrypted(configuration.host, configuration.port);
    else
        m_socket.connectToHost(configuration.host, configuration.port);
}

void SmtpClient::cancel()
{
    if (active())
        connectionLost(tr("Invio email annullato."));
}

void SmtpClient::receive()
{
    if (!active())
        return;
    m_input += m_socket.readAll();
    const auto invalid = [this] { connectionLost(tr("Risposta SMTP non valida.")); };
    if (m_input.size() > maximumReplyBytes) {
        invalid();
        return;
    }
    while (!m_input.isEmpty()) {
        const qsizetype end = m_input.indexOf('\n');
        if (end < 0) {
            if (m_input.size() > maximumReplyLineSize)
                invalid();
            return;
        }
        if (end < 4 || end > maximumReplyLineSize || m_input.at(end - 1) != '\r') {
            invalid();
            return;
        }
        const QByteArray line = m_input.left(end - 1);
        m_input.remove(0, end + 1);
        bool digits = false;
        const int code = line.left(3).toInt(&digits);
        const char separator = line.size() == 3 ? ' ' : line.at(3);
        if (!digits || code < 200 || code > 599 || (separator != ' ' && separator != '-')
            || (m_replyCode != 0 && m_replyCode != code) || m_replyLines.size() >= 64) {
            invalid();
            return;
        }
        m_replyCode = code;
        m_replyLines.append(QString::fromLatin1(line.mid(4)));
        if (separator == '-')
            continue;
        // This client never pipelines commands. Extra bytes after a reply
        // are therefore unsolicited, especially before a STARTTLS upgrade.
        if (!m_input.isEmpty()) {
            invalid();
            return;
        }
        m_replyCode = 0;
        const auto reply = std::exchange(m_replyLines, {});
        processReply(code, reply);
    }
}

void SmtpClient::processReply(int code, const QStringList &lines)
{
    switch (m_state) {
    case State::Greeting:
        if (code == 220 && (m_configuration.security == QStringLiteral("starttls") || m_socket.isEncrypted())) {
            command("EHLO localhost\r\n", m_socket.isEncrypted() ? State::SecureEhlo : State::Ehlo);
            return;
        }
        break;
    case State::Ehlo:
        if (code == 250) {
            if (capabilityArguments(lines, QStringLiteral("STARTTLS")).isEmpty()) {
                complete(false, tr("Il server SMTP non offre STARTTLS. Controlla modalità e porta."));
                return;
            }
            command("STARTTLS\r\n", State::StartTls);
            return;
        }
        break;
    case State::StartTls:
        if (code == 220) {
            m_state = State::TlsHandshake;
            m_timeout.start(stepTimeoutMs);
            m_socket.startClientEncryption();
            return;
        }
        break;
    case State::SecureEhlo:
        if (code == 250 && m_socket.isEncrypted()) {
            authenticate(lines);
            return;
        }
        break;
    case State::PlainChallenge:
        if (code == 334) {
            command(plainCredentials() + crlf, State::PlainAuth);
            return;
        }
        break;
    case State::LoginUser:
        if (code == 334) {
            command(m_configuration.username.toUtf8().toBase64() + crlf, State::LoginPassword);
            return;
        }
        break;
    case State::LoginPassword:
        if (code == 334) {
            command(m_configuration.password.toUtf8().toBase64() + crlf, State::LoginAuth);
            return;
        }
        break;
    case State::PlainAuth:
    case State::LoginAuth:
        if (code == 235) {
            sendEnvelope();
            return;
        }
        complete(false, tr("Autenticazione SMTP rifiutata. Controlla utente e password."));
        return;
    case State::Sender:
        if (code == 250) {
            command("RCPT TO:<" + m_recipient + ">\r\n", State::Recipient);
            return;
        }
        complete(false, tr("Il server SMTP ha rifiutato il mittente (%1).").arg(code));
        return;
    case State::Recipient:
        if (code == 250 || code == 251) {
            command("DATA\r\n", State::Data);
            return;
        }
        complete(false, tr("Il server SMTP ha rifiutato il destinatario (%1).").arg(code));
        return;
    case State::Data:
        if (code == 354) {
            command(std::exchange(m_prefix, {}), State::Upload);
            upload();
            return;
        }
        break;
    case State::Accepted:
        if (code == 250) {
            m_socket.write("QUIT\r\n");
            complete(true, tr("Email accettata dal server SMTP."));
            return;
        }
        break;
    case State::Idle:
        return;
    case State::TlsHandshake:
    case State::Upload:
        break;
    }
    complete(false, tr("Il server SMTP ha rifiutato l’operazione (%1).").arg(code));
}

void SmtpClient::command(const QByteArray &bytes, State next)
{
    m_state = next;
    m_timeout.start(stepTimeoutMs);
    if (m_socket.write(bytes) != bytes.size())
        connectionLost(tr("Impossibile scrivere al server SMTP."));
}

void SmtpClient::authenticate(const QStringList &capabilities)
{
    const auto size = capabilityArguments(capabilities, QStringLiteral("SIZE"));
    m_advertisesSize = !size.isEmpty();
    if (m_advertisesSize) {
        bool numeric = false;
        const qint64 maximum = size.first().toLongLong(&numeric);
        if (numeric && maximum > 0 && m_messageBytes > maximum) {
            complete(false, tr("L’email con allegato supera il limite del server SMTP."));
            return;
        }
    }
    if (m_configuration.username.isEmpty()) {
        sendEnvelope();
        return;
    }
    const auto mechanisms = capabilityArguments(capabilities, QStringLiteral("AUTH"));
    if (mechanisms.contains(QStringLiteral("PLAIN"))) {
        const auto credentials = plainCredentials();
        if (credentials.size() <= 499)
            command("AUTH PLAIN " + credentials + crlf, State::PlainAuth);
        else
            command("AUTH PLAIN\r\n", State::PlainChallenge);
    } else if (mechanisms.contains(QStringLiteral("LOGIN"))) {
        command("AUTH LOGIN\r\n", State::LoginUser);
    } else {
        complete(false, tr("Il server SMTP non offre l’autenticazione con password su TLS."));
    }
}

QByteArray SmtpClient::plainCredentials() const
{
    QByteArray sasl(1, '\0');
    sasl += m_configuration.username.toUtf8();
    sasl += '\0';
    sasl += m_configuration.password.toUtf8();
    return sasl.toBase64();
}

void SmtpClient::sendEnvelope()
{
    m_configuration.password.clear();
    QByteArray sender = "MAIL FROM:<" + m_configuration.sender.toLatin1() + '>';
    if (m_advertisesSize)
        sender += " SIZE=" + QByteArray::number(m_messageBytes);
    command(sender + crlf, State::Sender);
}

void SmtpClient::upload()
{
    constexpr qint64 sendBufferLimit = 64 * 1024;
    while (m_state == State::Upload && m_socket.bytesToWrite() + m_socket.encryptedBytesToWrite() < sendBufferLimit) {
        if (m_remaining == 0) {
            command(m_suffix + ".\r\n", State::Accepted);
            return;
        }
        const auto expected = qMin(m_remaining, qint64(bytesPerMimeLine * 64));
        const auto data = m_attachment.read(expected);
        if (data.size() != expected) {
            complete(false, tr("Impossibile leggere il PDF durante l’invio."));
            return;
        }
        m_remaining -= data.size();
        const auto encoded = encodeMimeLines(data);
        if (m_socket.write(encoded) != encoded.size()) {
            connectionLost(tr("Impossibile inviare l’allegato al server SMTP."));
            return;
        }
    }
}

void SmtpClient::connectionLost(const QString &error)
{
    if (!active())
        return;
    // After sending the final dot the server might have accepted the message
    // before the connection was interrupted. Don't report a definite failure.
    complete(false, m_state == State::Accepted
        ? tr("Il server SMTP non ha confermato la consegna. Controlla prima di riprovare.") : error);
}

void SmtpClient::complete(bool success, const QString &status)
{
    m_state = State::Idle;
    m_timeout.stop();
    m_deadline.stop();
    m_attachment.close();
    m_configuration = {};
    m_input.clear();
    m_replyLines.clear();
    m_prefix.clear();
    m_suffix.clear();
    if (success)
        m_socket.disconnectFromHost();
    else
        m_socket.abort();
    emit finished(success, status);
}
