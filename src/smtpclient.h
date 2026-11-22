#pragma once

#include <QFile>
#include <QObject>
#include <QSslSocket>
#include <QStringList>
#include <QTimer>

class SmtpClient : public QObject
{
    Q_OBJECT

public:
    struct Configuration {
        QString host;
        int port = 587;
        QString security = QStringLiteral("starttls");
        QString username;
        QString password;
        QString sender;
    };

    struct Message {
        QString recipient;
        QString subject;
        QString text;
        QString attachmentPath;
        QString attachmentName;
    };

    static constexpr qint64 maximumAttachmentSize = 15 * 1024 * 1024;

    explicit SmtpClient(QObject *parent = nullptr);
    ~SmtpClient() override;
    static QString normalizedHost(const QString &input);
    static bool validMailbox(const QString &address);
    static bool validConfiguration(const Configuration &configuration);
    static bool validMessage(const Message &message);
    bool active() const;

    void start(const Configuration &configuration, const Message &message);
    void cancel();

signals:
    void finished(bool success, const QString &status);

private:
    enum class State {
        Idle, Greeting, Ehlo, StartTls, TlsHandshake, SecureEhlo, PlainChallenge, PlainAuth,
        LoginUser, LoginPassword, LoginAuth, Sender, Recipient, Data, Upload, Accepted
    };

    void receive();
    void processReply(int code, const QStringList &lines);
    void command(const QByteArray &bytes, State next);
    void authenticate(const QStringList &capabilities);
    QByteArray plainCredentials() const;
    void sendEnvelope();
    void upload();
    void connectionLost(const QString &error);
    void complete(bool success, const QString &status);

    QSslSocket m_socket;
    QTimer m_timeout;
    QTimer m_deadline;
    QFile m_attachment;
    Configuration m_configuration;
    QByteArray m_recipient;
    QByteArray m_input;
    QStringList m_replyLines;
    int m_replyCode = 0;
    QByteArray m_prefix;
    QByteArray m_suffix;
    qint64 m_remaining = 0;
    qint64 m_messageBytes = 0;
    bool m_advertisesSize = false;
    State m_state = State::Idle;
};
