#include "wirelesssync.h"
#include "notebook.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

class WirelessSyncTests final : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;

    static constexpr bool nativeTablet()
    {
#ifdef Q_OS_LINUX
        return true;
#else
        return false;
#endif
    }

    void preparePaired(WirelessSync &wireless)
    {
        // No radio, socket or production pairing file is used by these tests.
        wireless.m_secret = QByteArray(16, 't');
        wireless.updatePeer();
        wireless.m_preferredAddress = QStringLiteral("192.168.50.20");
        wireless.m_transfer.setAutoSync(true);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }

    void init()
    {
        QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"),
                  QStringLiteral("Quaderno")).clear();
    }

    void automaticTransportSelectionAndFallback()
    {
        WirelessSync mac(nullptr, settings.filePath("absent-pairing"), false);
        preparePaired(mac);
        QCOMPARE(mac.automaticTransport(), QStringLiteral("auto"));
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Wifi);
        mac.m_bluetoothFallbackNext = true;
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Bluetooth);
        mac.setAutomaticTransport(QStringLiteral("wifi"));
        QVERIFY(mac.automaticWifiOnly());
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Wifi);
        mac.setAutomaticWifiAllowed(false);
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::None);
        mac.setAutomaticTransport(QStringLiteral("bluetooth"));
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Bluetooth);
        mac.setAutomaticTransport(QStringLiteral("invalid"));
        QCOMPARE(mac.automaticTransport(), QStringLiteral("bluetooth"));
        mac.setAutomaticWifiOnly(false);
        QCOMPARE(mac.automaticTransport(), QStringLiteral("auto"));
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Bluetooth);
        mac.setAutomaticWifiAllowed(true);
        mac.m_preferredAddress.clear();
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Bluetooth);

        WirelessSync tablet(nullptr, settings.filePath("absent-tablet-pairing"), true);
        preparePaired(tablet);
        QCOMPARE(tablet.automaticLink(), WirelessSync::Link::Waiting);
        tablet.setAutomaticWifiAllowed(false);
        QCOMPARE(tablet.automaticLink(), WirelessSync::Link::Waiting); // Bluetooth remains permitted.
        tablet.setAutomaticTransport(QStringLiteral("wifi"));
        QCOMPARE(tablet.automaticLink(), WirelessSync::Link::None);
    }

    void reconnectDelayIsBoundedAndProgressive()
    {
        QCOMPARE(WirelessSync::reconnectDelayMs(true, 0), 3000);
        QCOMPARE(WirelessSync::reconnectDelayMs(false, 0), 8000);
        QCOMPARE(WirelessSync::reconnectDelayMs(false, 1), 8000);
        QCOMPARE(WirelessSync::reconnectDelayMs(false, 2), 16000);
        QCOMPARE(WirelessSync::reconnectDelayMs(false, 3), 32000);
        QCOMPARE(WirelessSync::reconnectDelayMs(false, 20), 120000);
        QCOMPARE(WirelessSync::reconnectDelayMs(true, 20), 120000);
    }

    void lockedOrRevokedPolicyStopsPendingReconnectSynchronously()
    {
        WirelessSync wireless(nullptr, settings.filePath("absent-pairing"), nativeTablet());
        preparePaired(wireless);
        wireless.setAutomaticTransport(QStringLiteral("wifi"));
        QVERIFY(!wireless.canReconnectAutomatically());
        wireless.setLocked(false);
        QVERIFY(wireless.reconnectPending());
        QVERIFY(wireless.reconnectDelaySeconds() > 0);
        wireless.setAutomaticWifiAllowed(false);
        QVERIFY(!wireless.reconnectPending());
        QVERIFY(!wireless.canReconnectAutomatically());
        wireless.setAutomaticWifiAllowed(true);
        QVERIFY(wireless.reconnectPending());
        wireless.setAutomaticAllowed(false);
        QVERIFY(!wireless.reconnectPending());
        QVERIFY(!wireless.resumeAutomaticConnection());
        wireless.setAutomaticAllowed(true);
        QVERIFY(wireless.reconnectPending());
        wireless.setLocked(true);
        QVERIFY(!wireless.reconnectPending());
        QVERIFY(!wireless.transfer()->automaticAllowed());
        QCoreApplication::processEvents();
        QVERIFY(!wireless.active());
    }

    void explicitDisconnectStaysPausedUntilUserResumes()
    {
        WirelessSync wireless(nullptr, settings.filePath("absent-pairing"), nativeTablet());
        preparePaired(wireless);
        wireless.setLocked(false);
        wireless.stop();
        QVERIFY(wireless.connectionPaused());
        QVERIFY(!wireless.reconnectPending());
        wireless.setLocked(true);
        wireless.setLocked(false);
        QVERIFY(!wireless.reconnectPending());
        QVERIFY(wireless.resumeAutomaticConnection());
        QVERIFY(!wireless.connectionPaused());
        QVERIFY(wireless.reconnectPending());
        wireless.setLocked(true); // Cancel before processing events: no live transport.
        QVERIFY(!wireless.reconnectPending());
        QCoreApplication::processEvents();
        QVERIFY(!wireless.active());
    }

    void waitingTransportChoosesOneHandshakeAndIgnoresTheOther()
    {
        WirelessSync wireless(nullptr, settings.filePath("absent-pairing"), true);
        preparePaired(wireless);
        wireless.m_locked = false;
        wireless.m_link = WirelessSync::Link::Waiting;
        wireless.m_automaticConnection = true;
        wireless.m_wifiCandidate = true;
        wireless.m_bluetoothCandidate = true;
        QSignalSpy linkChanges(&wireless, &WirelessSync::linkChanged);
        wireless.transportReady(WirelessSync::Link::Wifi);
        QCOMPARE(wireless.m_link, WirelessSync::Link::Wifi);
        QVERIFY(wireless.m_channelStarted);
        QCOMPARE(linkChanges.size(), 1);
        wireless.transportReady(WirelessSync::Link::Bluetooth);
        wireless.transportEnded(WirelessSync::Link::Bluetooth, QStringLiteral("late old callback"));
        QCOMPARE(wireless.m_link, WirelessSync::Link::Wifi);
        QVERIFY(wireless.m_channelStarted);
        QCOMPARE(linkChanges.size(), 1);
        QVERIFY(!wireless.activatingBluetooth());
        wireless.stop();
    }

    void bluetoothCandidateSurvivesWifiFailureWithoutActivatingRadio()
    {
        WirelessSync wireless(nullptr, settings.filePath("absent-pairing"), true);
        preparePaired(wireless);
        wireless.m_locked = false;
        wireless.m_link = WirelessSync::Link::Waiting;
        wireless.m_automaticConnection = true;
        wireless.m_wifiCandidate = true;
        wireless.m_bluetoothCandidate = true;
        wireless.transportEnded(WirelessSync::Link::Wifi, QStringLiteral("network changed"));
        QCOMPARE(wireless.m_link, WirelessSync::Link::Waiting);
        QVERIFY(!wireless.m_wifiCandidate);
        QVERIFY(wireless.m_bluetoothCandidate);
        QVERIFY(wireless.reconnectPending());
        QVERIFY(!wireless.activatingBluetooth());
        wireless.setAutomaticWifiAllowed(false);
        QVERIFY(!wireless.reconnectPending()); // No reason to retry a prohibited Wi-Fi candidate.
        wireless.setLocked(true);
        QCOMPARE(wireless.m_link, WirelessSync::Link::None);
        QVERIFY(!wireless.m_bluetoothCandidate);
    }

    void policyChangesDoNotReplaceAuthenticatedConnection()
    {
        WirelessSync wireless(nullptr, settings.filePath("absent-pairing"), false);
        preparePaired(wireless);
        wireless.m_locked = false;
        wireless.m_automaticConnection = true;
        wireless.m_link = WirelessSync::Link::Wifi;
        wireless.updateAutomaticTransferGate();
        SyncChannel peer;
        connect(&wireless.m_channel, &SyncChannel::encryptedReady, &peer, &SyncChannel::receive, Qt::QueuedConnection);
        connect(&peer, &SyncChannel::encryptedReady, &wireless.m_channel, &SyncChannel::receive, Qt::QueuedConnection);
        const QByteArray key = QCryptographicHash::hash(
            QByteArrayLiteral("Quaderno wireless v1/TLS PSK/") + wireless.m_secret, QCryptographicHash::Sha256);
        peer.start(true, key, 64 * 1024 * 1024);
        wireless.startChannel();
        QTRY_VERIFY(wireless.m_channel.established());
        QVERIFY(wireless.transfer()->connected());
        QVERIFY(wireless.transfer()->automaticAllowed());
        QSignalSpy linkChanges(&wireless, &WirelessSync::linkChanged);
        wireless.setAutomaticTransport(QStringLiteral("bluetooth"));
        QVERIFY(wireless.m_channel.established());
        QVERIFY(wireless.transfer()->connected());
        QVERIFY(!wireless.transfer()->automaticAllowed());
        QCOMPARE(wireless.m_link, WirelessSync::Link::Wifi);
        QCOMPARE(linkChanges.size(), 0);
        wireless.setLocked(true);
        QVERIFY(!wireless.transfer()->connected());
        QVERIFY(!wireless.reconnectPending());
    }

    void wifiAddressValidationRejectsUsbAndInternet()
    {
        QVERIFY(WifiSyncLink::isSupportedAddress(QStringLiteral("192.168.50.20")));
        QVERIFY(WifiSyncLink::isSupportedAddress(QStringLiteral("172.16.1.2")));
        QVERIFY(!WifiSyncLink::isSupportedAddress(QStringLiteral("10.11.99.1")));
        QVERIFY(!WifiSyncLink::isSupportedAddress(QStringLiteral("8.8.8.8")));
        QVERIFY(!WifiSyncLink::isSupportedAddress(QStringLiteral("127.0.0.1")));
        QVERIFY(!WifiSyncLink::isSupportedAddress(QStringLiteral("192.168.50.20:47987")));
    }

    void realLinkResetPreservesAutomaticPreviewRetry()
    {
        QTemporaryDir directory;
        Notebook source(directory.filePath("source.json"));
        Notebook destination(directory.filePath("destination.json"));
        QVERIFY(source.createBook(QStringLiteral("Interrupted automatic preview")));
        WirelessSync wireless(&source, directory.filePath("absent-pairing"), nativeTablet());
        preparePaired(wireless);
        wireless.m_locked = false;
        wireless.m_link = WirelessSync::Link::Wifi;
        wireless.m_automaticConnection = true;
        wireless.updateAutomaticTransferGate();
        auto *sender = wireless.transfer();
        sender->setTriggeredAutomaticSync(true);
        sender->setPinned(source.bookId(), true);
        BleSync receiver(&destination, nullptr, directory.filePath("receiver"));
        receiver.setPeerIdentity(QString(32, QLatin1Char('b')));
        connect(sender, &BleSync::sendMessage, &receiver, &BleSync::receiveMessage, Qt::QueuedConnection);
        connect(&receiver, &BleSync::sendMessage, sender, &BleSync::receiveMessage, Qt::QueuedConnection);
        receiver.setConnected(true);
        sender->setConnected(true);
        const QByteArray before = destination.exportLibraryForTransfer();
        QVERIFY(sender->requestAutomaticSync());
        QTRY_COMPARE(sender->previewInfo().value(QStringLiteral("action")).toString(), QStringLiteral("create"));
        QVERIFY(sender->previewInfo().value(QStringLiteral("automatic")).toBool());
        wireless.linkEnded(QStringLiteral("Simulated radio disconnect"));
        QVERIFY(!sender->connected());
        QVERIFY(sender->previewInfo().isEmpty());
        QVERIFY(sender->automaticRetryPending());
        QCOMPARE(sender->pendingAutomaticCount(), 1);
        QVERIFY(!sender->automaticAllowed());
        QCOMPARE(destination.exportLibraryForTransfer(), before);
        wireless.setLocked(true); // Never execute the armed physical reconnect.
        QVERIFY(!wireless.reconnectPending());
    }

    void localTcpKeepaliveDetectsLostPeersPromptly()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        QTRY_VERIFY(server.hasPendingConnections());
        std::unique_ptr<QTcpSocket> accepted(server.nextPendingConnection());
        QVERIFY(accepted);
        WifiSyncLink::configureConnectedSocket(client);
        WifiSyncLink::configureConnectedSocket(*accepted);
        QVERIFY(client.socketOption(QAbstractSocket::KeepAliveOption).toInt() != 0);
        QVERIFY(accepted->socketOption(QAbstractSocket::KeepAliveOption).toInt() != 0);
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
        for (QTcpSocket *socket : {&client, accepted.get()}) {
            const auto getOption = [socket](int option) {
                int value = -1;
                socklen_t size = sizeof(value);
                return ::getsockopt(int(socket->socketDescriptor()), IPPROTO_TCP, option, &value, &size) == 0
                    ? value : -1;
            };
#ifdef Q_OS_MACOS
            QCOMPARE(getOption(TCP_KEEPALIVE), 30);
#else
            QCOMPARE(getOption(TCP_KEEPIDLE), 30);
#endif
            QCOMPARE(getOption(TCP_KEEPINTVL), 10);
            QCOMPARE(getOption(TCP_KEEPCNT), 3);
        }
#endif
        QCOMPARE(client.write("still connected"), qint64(15));
        QTRY_COMPARE(accepted->bytesAvailable(), qint64(15));
        QCOMPARE(accepted->readAll(), QByteArrayLiteral("still connected"));
    }
};

QTEST_MAIN(WirelessSyncTests)
#include "wirelesssync_tests.moc"
