#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>

class WifiSyncLinkPrivate;
class QTcpSocket;

class WifiSyncLink : public QObject
{
    Q_OBJECT

public:
    explicit WifiSyncLink(QObject *parent = nullptr);
    ~WifiSyncLink() override;

    void listen();
    void connectTo(const QString &ipv4);
    bool connectToHandoff(const QString &ipv4);
    static bool isSupportedAddress(const QString &ipv4);
    void stop();
    void send(const QByteArray &data);
    bool active() const;
    QString address() const;
    QString status() const;

signals:
    void ready();
    void received(const QByteArray &data);
    void disconnected(const QString &message);
    void changed();

private:
    friend class WifiSyncLinkPrivate;
    friend class WirelessSyncTests;
    static void configureConnectedSocket(QTcpSocket &socket);

    // The two test endpoints share a kernel-assigned port. Keep the actual
    // loopback address out of the authenticated rendezvous messages.
    struct Loopback {
        QString advertisedAddress;
        quint16 port = 0;
    };
    std::shared_ptr<Loopback> m_loopback;
    std::unique_ptr<WifiSyncLinkPrivate> m_private;
};
