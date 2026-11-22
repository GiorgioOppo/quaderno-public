#include "wifisynclink.h"

#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QDebug>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

namespace {
constexpr quint16 syncPort = 47987;
constexpr int connectTimeoutMs = 10000;
constexpr qint64 maximumBufferedBytes = 70 * 1024 * 1024;
constexpr qint64 readChunkBytes = 64 * 1024;

bool isLocalAddress(const QHostAddress &address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    const quint32 ipv4 = address.toIPv4Address();
    if ((ipv4 & 0xffffff00U) == 0x0a0b6300U)
        return false; // Paper Pro USB networking uses 10.11.99.0/24.
    return (ipv4 & 0xff000000U) == 0x0a000000U      // 10.0.0.0/8
        || (ipv4 & 0xfff00000U) == 0xac100000U     // 172.16.0.0/12
        || (ipv4 & 0xffff0000U) == 0xc0a80000U     // 192.168.0.0/16
        || (ipv4 >= 0xa9fe0100U && ipv4 <= 0xa9fefeffU); // 169.254.1.0–169.254.254.255
}

QHostAddress wifiAddress()
{
    QHostAddress fallback;
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        const bool primary = interface.name() == QStringLiteral("wlan0");
        const auto flags = interface.flags();
        if ((!primary && interface.type() != QNetworkInterface::Wifi)
            || !flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const auto &entry : interface.addressEntries()) {
            if (!isLocalAddress(entry.ip()))
                continue;
            if (primary)
                return entry.ip();
            if (fallback.isNull())
                fallback = entry.ip();
        }
    }
    return fallback;
}

QHostAddress parseAddress(const QString &input)
{
    const QString address = input.trimmed();
    QHostAddress parsed;
    if (!parsed.setAddress(address) || parsed.toString() != address || !isLocalAddress(parsed))
        return {};
    return parsed;
}
}

class WifiSyncLinkPrivate
{
public:
    explicit WifiSyncLinkPrivate(WifiSyncLink &owner) : m_owner(owner)
    {
        m_server.setProxy(QNetworkProxy::NoProxy);
        m_server.setMaxPendingConnections(1);
        m_server.setListenBacklogSize(1);
        m_connectDeadline.setSingleShot(true);
        m_interfaceWatch.setInterval(3000);
        QObject::connect(&m_server, &QTcpServer::pendingConnectionAvailable, &m_owner, [this] { accept(); });
        QObject::connect(&m_server, &QTcpServer::acceptError, &m_owner, [this] {
            fail(WifiSyncLink::tr("Impossibile accettare il collegamento Wi-Fi."));
        });
        QObject::connect(&m_connectDeadline, &QTimer::timeout, &m_owner, [this] {
            fail(WifiSyncLink::tr("Il Paper Pro non risponde. Controlla l’indirizzo e l’accesso del Mac alla rete locale."));
        });
        QObject::connect(&m_interfaceWatch, &QTimer::timeout, &m_owner, [this] {
            // A listener bound to a DHCP address may remain nominally active
            // after roaming or Wi-Fi loss. End that stale link so the normal
            // authenticated reconnect path can bind the current interface.
            if (!m_boundAddress.isNull() && wifiAddress() != m_boundAddress)
                fail(WifiSyncLink::tr("La rete Wi-Fi del Paper Pro è cambiata. Riconnessione in corso…"));
        });
    }

    ~WifiSyncLinkPrivate() { close(); }

    void listen(const QHostAddress &host)
    {
        close();
        if (host.isNull()) {
            fail(WifiSyncLink::tr("Connetti il Paper Pro a una rete Wi-Fi con un indirizzo IPv4 locale."));
            return;
        }
        if (!m_server.listen(host, syncPort)) {
            fail(WifiSyncLink::tr("Impossibile attivare la sincronizzazione sulla rete Wi-Fi del Paper Pro."));
            return;
        }
        m_address = host.toString();
        m_boundAddress = host;
        m_interfaceWatch.start();
        m_status = WifiSyncLink::tr("In attesa del Mac sulla rete Wi-Fi…");
        emit m_owner.changed();
    }

    void connectTo(const QHostAddress &host)
    {
        close();
        if (host.isNull()) {
            fail(WifiSyncLink::tr("Inserisci l’indirizzo IPv4 locale mostrato sul Paper Pro."));
            return;
        }

        auto *socket = new QTcpSocket(&m_owner);
        socket->setProxy(QNetworkProxy::NoProxy);
        adopt(socket);
        m_address = host.toString();
        m_status = WifiSyncLink::tr("Collegamento al Paper Pro sulla rete Wi-Fi…");
        m_connectDeadline.start(connectTimeoutMs);
        emit m_owner.changed();
        if (socket == m_socket)
            socket->connectToHost(host, syncPort);
    }

    void stop()
    {
        close();
        m_status.clear();
        emit m_owner.changed();
    }

    void send(const QByteArray &data)
    {
        if (!m_ready || data.isEmpty())
            return;
        auto *socket = m_socket;
        if (data.size() > maximumBufferedBytes - socket->bytesToWrite()) {
            fail(WifiSyncLink::tr("Il collegamento Wi-Fi ha superato il limite dei dati in attesa."));
            return;
        }
        if (socket->write(data) != data.size() && socket == m_socket)
            fail(WifiSyncLink::tr("Impossibile inviare dati sulla rete Wi-Fi."));
    }

    bool active() const { return m_socket || m_server.isListening(); }
    QString address() const { return m_address; }
    QString status() const { return m_status; }

private:
    void close()
    {
        m_connectDeadline.stop();
        m_interfaceWatch.stop();
        m_boundAddress.clear();
        m_server.close();
        while (auto *pending = m_server.nextPendingConnection()) {
            pending->abort();
            pending->deleteLater();
        }
        m_ready = false;
        m_address.clear();
        if (auto *socket = m_socket) {
            m_socket = nullptr;
            QObject::disconnect(socket, nullptr, &m_owner, nullptr);
            socket->abort();
            socket->deleteLater();
        }
    }

    void fail(const QString &message)
    {
        close();
        m_status = message;
        emit m_owner.disconnected(message);
        emit m_owner.changed();
    }

    void accept()
    {
        auto *socket = m_server.nextPendingConnection();
        if (!socket)
            return;
        const QHostAddress peer = socket->peerAddress();
        const bool sameLoopback = m_server.serverAddress().isLoopback() && peer.isLoopback();
        if (m_socket || (!isLocalAddress(peer) && !sameLoopback)) {
            socket->abort();
            socket->deleteLater();
            return;
        }
        m_server.close();
        adopt(socket);
        connected(socket);
    }

    void adopt(QTcpSocket *socket)
    {
        socket->setParent(&m_owner);
        socket->setReadBufferSize(readChunkBytes);
        m_socket = socket;
        QObject::connect(socket, &QTcpSocket::connected, &m_owner, [this, socket] { connected(socket); });
        QObject::connect(socket, &QTcpSocket::readyRead, &m_owner, [this, socket] { read(socket); });
        QObject::connect(socket, &QTcpSocket::disconnected, &m_owner, [this, socket] {
            read(socket);
            if (socket == m_socket)
                fail(WifiSyncLink::tr("Collegamento Wi-Fi interrotto."));
        });
        QObject::connect(socket, &QTcpSocket::errorOccurred, &m_owner, [this, socket](QAbstractSocket::SocketError error) {
            if (socket != m_socket || error == QAbstractSocket::RemoteHostClosedError)
                return;
            if (m_ready)
                fail(WifiSyncLink::tr("Errore durante lo scambio di dati sulla rete Wi-Fi."));
            else
                fail(WifiSyncLink::tr("Impossibile collegarsi al Paper Pro. Controlla l’indirizzo e l’accesso del Mac alla rete locale."));
        });
    }

    void connected(QTcpSocket *socket)
    {
        if (socket != m_socket || m_ready)
            return;
        m_connectDeadline.stop();
        WifiSyncLink::configureConnectedSocket(*socket);
        m_ready = true;
        m_status = WifiSyncLink::tr("Collegamento Wi-Fi attivo.");
        emit m_owner.ready();
        if (socket == m_socket) {
            emit m_owner.changed();
            read(socket);
        }
    }

    void read(QTcpSocket *socket)
    {
        while (m_ready && socket == m_socket && socket->bytesAvailable() > 0) {
            const QByteArray data = socket->read(readChunkBytes);
            if (data.isEmpty())
                return;
            emit m_owner.received(data);
        }
    }

    WifiSyncLink &m_owner;
    QTcpServer m_server;
    QTimer m_connectDeadline;
    QTimer m_interfaceWatch;
    QHostAddress m_boundAddress;
    QTcpSocket *m_socket = nullptr;
    QString m_address;
    QString m_status;
    bool m_ready = false;
};

WifiSyncLink::WifiSyncLink(QObject *parent)
    : QObject(parent), m_private(std::make_unique<WifiSyncLinkPrivate>(*this))
{
}

WifiSyncLink::~WifiSyncLink() = default;

void WifiSyncLink::configureConnectedSocket(QTcpSocket &socket)
{
    socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket.setSocketOption(QAbstractSocket::KeepAliveOption, 1);
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // Default TCP keepalive periods are too long to recover promptly after
    // sleep or a lost access point. Acknowledged idle sessions stay connected;
    // only an unresponsive peer reaches the existing disconnect/retry path.
    const int descriptor = int(socket.socketDescriptor());
    const auto setOption = [descriptor](int option, int value) {
        return ::setsockopt(descriptor, IPPROTO_TCP, option, &value, sizeof(value)) == 0;
    };
#ifdef Q_OS_MACOS
    const bool idle = setOption(TCP_KEEPALIVE, 30);
#else
    const bool idle = setOption(TCP_KEEPIDLE, 30);
#endif
    const bool interval = setOption(TCP_KEEPINTVL, 10);
    const bool probes = setOption(TCP_KEEPCNT, 3);
    if (!idle || !interval || !probes)
        qWarning("Wi-Fi TCP keepalive tuning unavailable; retaining supported socket defaults.");
#endif
}

void WifiSyncLink::listen()
{
    m_private->listen(wifiAddress());
}

void WifiSyncLink::connectTo(const QString &ipv4)
{
    m_private->connectTo(parseAddress(ipv4));
}

bool WifiSyncLink::isSupportedAddress(const QString &ipv4)
{
    return !parseAddress(ipv4).isNull();
}

void WifiSyncLink::stop()
{
    m_private->stop();
}

void WifiSyncLink::send(const QByteArray &data)
{
    m_private->send(data);
}

bool WifiSyncLink::active() const
{
    return m_private->active();
}

QString WifiSyncLink::address() const
{
    return m_private->address();
}

QString WifiSyncLink::status() const
{
    return m_private->status();
}
