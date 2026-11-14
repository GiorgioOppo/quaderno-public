#pragma once

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <memory>

class SyncChannel final : public QObject
{
    Q_OBJECT

public:
    explicit SyncChannel(QObject *parent = nullptr);
    ~SyncChannel() override;
    void start(bool server, const QByteArray &key, qsizetype payloadLimit);
    void stop();
    void receive(const QByteArray &encrypted);
    void sendMessage(const QVariantMap &metadata, const QByteArray &payload);
    bool established() const;

signals:
    void establishedChanged();
    void encryptedReady(const QByteArray &bytes);
    void messageReceived(const QVariantMap &metadata, const QByteArray &payload);
    void failed(const QString &message);

private:
    struct Tls;
    void flushEncrypted();
    void readPlaintext();
    void pump();
    void fail(const QString &message);

    std::unique_ptr<Tls> m_tls;
    QTimer m_handshakeDeadline;
    qsizetype m_payloadLimit = 0;
    QByteArray m_plaintext;
    bool m_established = false;
};
