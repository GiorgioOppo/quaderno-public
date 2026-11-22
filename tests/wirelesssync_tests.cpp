#include "wirelesssync.h"
#include "notebook.h"
#include "screenstream.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPdfWriter>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QImage>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <algorithm>
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

    void preparePaired(WirelessSync &wireless, const QString &peerIdentity = {})
    {
        // Use an isolated pairing and settings in every fixture.
        wireless.m_secret = QByteArray(16, 't');
        wireless.updatePeer();
        if (!peerIdentity.isEmpty())
            wireless.transfer()->setPeerIdentity(peerIdentity);
        wireless.m_preferredAddress = QStringLiteral("192.168.50.20");
        wireless.m_transfer.setAutoSync(true);
    }

    static void forwardBluetoothPackets(BlePipe &sender, BlePipe &receiver)
    {
        sender.setOutput([&receiver](const QByteArray &packet) {
            return QMetaObject::invokeMethod(&receiver, [packet, &receiver] { receiver.receive(packet); },
                                             Qt::QueuedConnection);
        }, [] { return 244; });
    }

    void prepareBluetoothPair(WirelessSync &mac, WirelessSync &tablet, bool tabletSupportsHandoff)
    {
        preparePaired(mac, QString(32, QLatin1Char('a')));
        preparePaired(tablet, QString(32, QLatin1Char('b')));
        for (auto *peer : {&mac, &tablet}) {
            peer->m_locked = false;
            peer->m_link = WirelessSync::Link::Bluetooth;
            peer->m_automaticConnection = !peer->m_tablet || tabletSupportsHandoff;
            peer->transfer()->setTriggeredAutomaticSync(true);
            peer->updateAutomaticTransferGate();
        }
        forwardBluetoothPackets(mac.m_pipe, tablet.m_pipe);
        forwardBluetoothPackets(tablet.m_pipe, mac.m_pipe);
        tablet.transportReady(WirelessSync::Link::Bluetooth);
        mac.transportReady(WirelessSync::Link::Bluetooth);
    }

    static QVariantMap pdfOption(const Notebook &notebook)
    {
        for (const auto &option : notebook.shareOptions()) {
            const auto choice = option.toMap();
            if (choice.value(QStringLiteral("format")) == QStringLiteral("pdf-page"))
                return choice;
        }
        return {};
    }

    static bool startDirectWifiPair(WirelessSync &tablet, WirelessSync &mac)
    {
        auto network = std::make_shared<WifiSyncLink::Loopback>();
        network->advertisedAddress = QStringLiteral("192.168.50.20");
        tablet.m_wifi.m_loopback = network;
        mac.m_wifi.m_loopback = network;
        tablet.m_wifi.listen();
        return network->port != 0 && mac.m_wifi.connectToHandoff(network->advertisedAddress);
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
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Bluetooth);
        mac.m_directWifiNext = true;
        QCOMPARE(mac.automaticLink(), WirelessSync::Link::Wifi);
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
        QCOMPARE(wireless.m_channelLink, WirelessSync::Link::Wifi);
        QCOMPARE(linkChanges.size(), 1);
        wireless.transportReady(WirelessSync::Link::Bluetooth);
        wireless.transportEnded(WirelessSync::Link::Bluetooth, QStringLiteral("late old callback"));
        QCOMPARE(wireless.m_link, WirelessSync::Link::Wifi);
        QCOMPARE(wireless.m_channelLink, WirelessSync::Link::Wifi);
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

        WifiSyncLink link;
        QVERIFY(!link.connectToHandoff(QStringLiteral("10.11.99.1")));
        QVERIFY(!link.connectToHandoff(QStringLiteral("8.8.8.8")));
        QVERIFY(!link.active());
    }

    void automaticHandoffTransfersThroughOneSelectedLink_data()
    {
        QTest::addColumn<bool>("tabletOrigin");
        QTest::addColumn<QString>("networkState");
        QTest::newRow("mac-work-over-wifi") << false << QStringLiteral("reachable");
        QTest::newRow("tablet-work-over-wifi") << true << QStringLiteral("reachable");
        QTest::newRow("wifi-drops-during-authentication") << false << QStringLiteral("closed");
        QTest::newRow("wifi-authentication-times-out") << false << QStringLiteral("silent");
        QTest::newRow("wifi-policy-blocks-handoff") << false << QStringLiteral("blocked");
        QTest::newRow("legacy-peer-stays-on-bluetooth") << false << QStringLiteral("legacy");
        QTest::newRow("wifi-loss-after-promotion") << false << QStringLiteral("lost-after-promotion");
    }

    void automaticHandoffTransfersThroughOneSelectedLink()
    {
        QFETCH(bool, tabletOrigin);
        QFETCH(QString, networkState);
        const bool networkFault = networkState == QStringLiteral("closed") || networkState == QStringLiteral("silent");
        const bool legacyPeer = networkState == QStringLiteral("legacy");
        const bool wifiBlocked = networkState == QStringLiteral("blocked");
        const bool losePromotedWifi = networkState == QStringLiteral("lost-after-promotion");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Notebook source(directory.filePath("source.json"));
        Notebook destination(directory.filePath("destination.json"));
        QVERIFY(source.createBook(QStringLiteral("Rendezvous regression")));
        const QString book = source.bookId();
        const auto revision = source.bookTransferRevision(book);
        const auto originalBookCount = destination.books().size();

        WirelessSync mac(tabletOrigin ? &destination : &source, directory.filePath("mac-pairing"), false);
        WirelessSync tablet(tabletOrigin ? &source : &destination, directory.filePath("tablet-pairing"), true);
        const auto network = std::make_shared<WifiSyncLink::Loopback>();
        network->advertisedAddress = QStringLiteral("192.168.50.20");
        tablet.m_wifi.m_loopback = network;
        mac.m_wifi.m_loopback = network;

        QTcpServer interruptedNetwork;
        QSignalSpy attemptedConnection(&interruptedNetwork, &QTcpServer::pendingConnectionAvailable);
        if (networkFault) {
            QVERIFY(interruptedNetwork.listen(QHostAddress::LocalHost, 0));
            mac.m_wifi.m_loopback = std::make_shared<WifiSyncLink::Loopback>(
                WifiSyncLink::Loopback{network->advertisedAddress, interruptedNetwork.serverPort()});
            if (networkState == QStringLiteral("closed")) {
                connect(&interruptedNetwork, &QTcpServer::pendingConnectionAvailable, &interruptedNetwork, [&] {
                    if (auto *socket = interruptedNetwork.nextPendingConnection()) {
                        socket->abort();
                        socket->deleteLater();
                    }
                });
            }
        }

        if (wifiBlocked)
            tablet.setAutomaticWifiAllowed(false);
        prepareBluetoothPair(mac, tablet, !legacyPeer);
        QTRY_VERIFY(mac.m_channel.established());
        QTRY_VERIFY(tablet.m_channel.established());
        if (legacyPeer) {
            QCOMPARE(mac.m_rendezvous, WirelessSync::Rendezvous::Discovering);
            QCOMPARE(tablet.m_rendezvous, WirelessSync::Rendezvous::Inactive);
        } else if (wifiBlocked) {
            QTRY_COMPARE(mac.m_rendezvous, WirelessSync::Rendezvous::Bluetooth);
            QTRY_COMPARE(tablet.m_rendezvous, WirelessSync::Rendezvous::Bluetooth);
        } else {
            QTRY_COMPARE(mac.m_rendezvous, WirelessSync::Rendezvous::Idle);
            QTRY_COMPARE(tablet.m_rendezvous, WirelessSync::Rendezvous::Idle);
        }
        QVERIFY(!mac.m_wifi.active());
        QVERIFY(!tablet.m_wifi.active());
        QCOMPARE(network->port, quint16(0));
        QVERIFY(!destination.hasBookId(book));

        auto *sender = (tabletOrigin ? tablet : mac).transfer();
        QSignalSpy transfers(sender, &BleSync::sendMessage);
        sender->setPinned(book, true);
        QVERIFY(sender->requestAutomaticSync());
        QTRY_COMPARE_WITH_TIMEOUT(sender->previewInfo().value(QStringLiteral("action")).toString(),
                                  QStringLiteral("create"), 10000);
        if (networkFault || wifiBlocked || legacyPeer) {
            QCOMPARE(attemptedConnection.size(), networkFault ? 1 : 0);
            QVERIFY(mac.bluetoothActive());
            QVERIFY(tablet.bluetoothActive());
            QVERIFY(!mac.m_wifi.active());
            QVERIFY(!tablet.m_wifi.active());
            if (legacyPeer)
                QCOMPARE(mac.m_rendezvous, WirelessSync::Rendezvous::Inactive);
        } else {
            QCOMPARE(mac.m_rendezvous, WirelessSync::Rendezvous::Wifi);
            QCOMPARE(tablet.m_rendezvous, WirelessSync::Rendezvous::Wifi);
            QVERIFY(mac.m_handoffWifi.established());
            QVERIFY(tablet.m_handoffWifi.established());
            QCOMPARE(mac.savedWifiAddress(), network->advertisedAddress);
            QTRY_VERIFY(mac.screenStreamAvailable());
            QTRY_VERIFY(tablet.screenStreamAvailable());
            QSignalSpy screenMessages(&mac, &WirelessSync::screenStreamMessageReceived);
            const QVariantMap frame{{QStringLiteral("type"), QStringLiteral("screen-frame")}};
            QVERIFY(tablet.sendScreenStreamMessage(frame, "via handoff"));
            QTRY_COMPARE(screenMessages.size(), 1);
            QCOMPARE(screenMessages.first().at(1).toByteArray(), QByteArrayLiteral("via handoff"));
        }
        // The transport change does not skip the review for a first transfer.
        QVERIFY(!destination.hasBookId(book));
        sender->apply();
        QTRY_VERIFY(sender->completed());
        QCOMPARE(destination.bookTransferRevision(book), revision);
        QCOMPARE(destination.books().size(), originalBookCount + 1);
        int applyRequests = 0;
        for (const auto &args : transfers) {
            if (args.first().toMap().value(QStringLiteral("type")) == QStringLiteral("apply"))
                ++applyRequests;
        }
        QCOMPARE(applyRequests, 1);

        if (losePromotedWifi)
            tablet.m_wifi.stop(); // A real socket close, while the BLE control link stays up.
        if (wifiBlocked)
            tablet.setAutomaticWifiAllowed(true);
        if (networkFault || wifiBlocked || losePromotedWifi) {
            QTRY_COMPARE(mac.m_rendezvous, WirelessSync::Rendezvous::Idle);
            QTRY_COMPARE(tablet.m_rendezvous, WirelessSync::Rendezvous::Idle);
            QVERIFY(mac.bluetoothActive());
            QVERIFY(tablet.bluetoothActive());
            QVERIFY(mac.m_channel.established());
            QVERIFY(tablet.m_channel.established());
            mac.m_wifi.m_loopback = network;
            QVERIFY(source.renameBook(QStringLiteral("Network available again")));
            const auto editedRevision = source.bookTransferRevision(book);
            QVERIFY(editedRevision != revision);
            QVERIFY(sender->requestAutomaticSync());
            QTRY_COMPARE(destination.bookTransferRevision(book), editedRevision);
            QCOMPARE(mac.m_rendezvous, WirelessSync::Rendezvous::Wifi);
            QCOMPARE(tablet.m_rendezvous, WirelessSync::Rendezvous::Wifi);
            QCOMPARE(destination.books().size(), originalBookCount + 1);
        }
        mac.setLocked(true);
        tablet.setLocked(true);
    }

    void directWifiPreservesEditableConflictBranchesAfterReconnect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto tabletPath = directory.filePath("tablet.json");
        const auto macPath = directory.filePath("mac.json");
        Notebook tabletLibrary(tabletPath);
        Notebook macLibrary(macPath);

        // Keep an actual PDF source attached without requiring a PDF renderer.
        QByteArray pdf;
        QBuffer output(&pdf);
        QVERIFY(output.open(QIODevice::WriteOnly));
        {
            QPdfWriter writer(&output);
            writer.setResolution(72);
            QPainter painter(&writer);
            QVERIFY(painter.isActive());
            painter.drawText(40, 80, QStringLiteral("Battery tests for the field report"));
        }
        output.close();
        const QString asset = QString::fromLatin1(QCryptographicHash::hash(pdf, QCryptographicHash::Sha256).toHex());
        const QString bookId = QStringLiteral("wireless-field-report");
        const QJsonObject pdfPage{{"template", "blank"}, {"strokes", QJsonArray{}}, {"items", QJsonArray{}},
            {"source", QJsonObject{{"kind", "pdf"}, {"asset", asset}, {"page", 0}}}};
        const QJsonObject book{{"id", bookId}, {"title", "Field report"}, {"pages", QJsonArray{pdfPage}},
            {"sourceDocuments", QJsonObject{{asset,
                QJsonObject{{"kind", "pdf"}, {"data", QString::fromLatin1(pdf.toBase64())}}}}}};
        QVERIFY2(tabletLibrary.importFromData(QJsonDocument(QJsonObject{
            {"version", 9}, {"books", QJsonArray{book}}, {"folders", QJsonArray{}}, {"activeBook", bookId}
        }).toJson()), qPrintable(tabletLibrary.error()));
        QVERIFY(tabletLibrary.openBook(bookId));
        QVERIFY(tabletLibrary.hasPageBackground());
        const QString notePage = tabletLibrary.pageId();
        const QVariantList points{QVariantMap{{"x", .2}, {"y", .2}, {"p", .6}},
                                  QVariantMap{{"x", .6}, {"y", .21}, {"p", .8}}};
        QVERIFY(tabletLibrary.appendStroke({{"tool", "highlighter"}, {"color", "#f2d248"},
                                            {"width", 8.0}, {"points", points}}));
        const QVariantList highlights = tabletLibrary.strokes();
        const QVariantMap note{{"type", "note"}, {"x", .63}, {"y", .42}, {"width", .05}, {"height", .0375},
            {"text", "Check the observed voltage"}, {"noteColor", "blue"},
            {"noteInk", QVariantList{QVariantMap{{"tool", "pen"}, {"color", "#202833"},
                                                {"width", 3.0}, {"points", points}}}}};
        QVERIFY(tabletLibrary.savePageNote(note, bookId, notePage));
        const auto originalNote = tabletLibrary.pageItems().value(0).toMap();
        const QString originalFormula = QStringLiteral("=SUM(A2:A2)");
        QVERIFY(tabletLibrary.savePageItemOnNewPage({{"type", "table"}, {"rows", 2}, {"cols", 2},
            {"cells", QVariantList{"Reading", "Total", "4", originalFormula}}}));
        const QString tablePage = tabletLibrary.pageId();
        const auto initialRevision = tabletLibrary.bookTransferRevision(bookId);

        WirelessSync tablet(&tabletLibrary, directory.filePath("tablet-pairing"), true);
        WirelessSync mac(&macLibrary, directory.filePath("mac-pairing"), false);
        preparePaired(tablet, QString(32, QLatin1Char('b')));
        preparePaired(mac, QString(32, QLatin1Char('a')));
        for (auto *peer : {&tablet, &mac}) {
            peer->transfer()->setAutoSync(false);
            peer->m_locked = false;
        }
        const auto connectWifi = [&] {
            // Both device roles share the host process in this fixture.
            for (auto *peer : {&tablet, &mac}) {
                peer->m_paused = false;
                peer->m_link = WirelessSync::Link::Wifi;
            }
            return startDirectWifiPair(tablet, mac);
        };
        auto *tabletTransfer = tablet.transfer();
        auto *macTransfer = mac.transfer();
        QVERIFY(connectWifi());
        QTRY_VERIFY(tablet.m_channel.established() && mac.m_channel.established());
        QTRY_VERIFY(tabletTransfer->connected() && macTransfer->connected());
        const int initialMacCount = macLibrary.books().size();
        tabletTransfer->preview(bookId);
        QTRY_COMPARE(tabletTransfer->previewInfo().value("action").toString(), QStringLiteral("create"));
        QVERIFY(!macLibrary.hasBookId(bookId));
        tabletTransfer->apply();
        QTRY_VERIFY(tabletTransfer->completed());
        QCOMPARE(macLibrary.bookTransferRevision(bookId), initialRevision);
        QCOMPARE(macLibrary.books().size(), initialMacCount + 1);

        QVERIFY(tabletLibrary.openPage(notePage));
        auto tabletNote = originalNote;
        tabletNote.insert("text", "Paper Pro: replaced the battery");
        QVERIFY(tabletLibrary.savePageNote(tabletNote, bookId, notePage, originalNote));
        QVERIFY(macLibrary.openBook(bookId));
        QVERIFY(macLibrary.openPage(tablePage));
        auto macTable = macLibrary.pageItems().value(0).toMap();
        const QString macFormula = QStringLiteral("=SUM(A2;A2;A2)");
        macTable.insert("cells", QVariantList{"Reading", "Total", "7", macFormula});
        QVERIFY(macLibrary.savePageItem(macTable));
        QCOMPARE(macLibrary.tableCellValues(macTable).value(3).toString(), QStringLiteral("21"));
        const auto macRevision = macLibrary.bookTransferRevision(bookId);
        const auto tabletRevision = tabletLibrary.bookTransferRevision(bookId);
        QVERIFY(macRevision != initialRevision && tabletRevision != initialRevision && macRevision != tabletRevision);

        tabletTransfer->preview(bookId);
        QTRY_COMPARE(tabletTransfer->previewInfo().value("action").toString(), QStringLiteral("conflict"));
        const auto macBeforeConflict = macLibrary.exportLibraryForTransfer();
        tablet.m_wifi.stop();
        QTRY_VERIFY(!macTransfer->connected());
        tablet.stop();
        QVERIFY(tabletTransfer->previewInfo().isEmpty());
        tabletTransfer->apply(); // The approval attached to the closed link cannot change the Mac library.
        QCOMPARE(macLibrary.exportLibraryForTransfer(), macBeforeConflict);

        QVERIFY(connectWifi());
        QTRY_VERIFY(tablet.m_channel.established() && mac.m_channel.established());
        QTRY_VERIFY(tabletTransfer->connected() && macTransfer->connected());
        QCOMPARE(macLibrary.exportLibraryForTransfer(), macBeforeConflict);
        tabletTransfer->preview(bookId);
        QTRY_COMPARE(tabletTransfer->previewInfo().value("action").toString(), QStringLiteral("conflict"));
        tabletTransfer->apply();
        QTRY_VERIFY(tabletTransfer->completed());
        QCOMPARE(macLibrary.books().size(), initialMacCount + 2);
        QCOMPARE(macLibrary.bookTransferRevision(bookId), macRevision);
        QCOMPARE(tabletLibrary.bookTransferRevision(bookId), tabletRevision);

        const auto conflictCopies = [&bookId](const Notebook &library) {
            QStringList copies;
            for (const auto &entry : library.books()) {
                const auto candidate = entry.toMap();
                const auto id = candidate.value("id").toString();
                if (id != bookId && candidate.value("title").toString().startsWith(QStringLiteral("Field report")))
                    copies.append(id);
            }
            return copies;
        };
        const auto macCopies = conflictCopies(macLibrary);
        QCOMPARE(macCopies.size(), 1);
        const auto copyId = macCopies.first();

        macTransfer->preview(bookId);
        QTRY_COMPARE(macTransfer->previewInfo().value("action").toString(), QStringLiteral("conflict"));
        macTransfer->apply();
        QTRY_VERIFY(macTransfer->completed());
        QCOMPARE(tabletLibrary.bookTransferRevision(bookId), tabletRevision);
        const auto tabletCopies = conflictCopies(tabletLibrary);
        QCOMPARE(tabletCopies.size(), 1);
        Notebook reopenedTablet(tabletPath);
        QVERIFY(reopenedTablet.openBook(tabletCopies.first()));
        QVERIFY(reopenedTablet.goToPage(1));
        const auto receivedMacTable = reopenedTablet.pageItems().value(0).toMap();
        QCOMPARE(receivedMacTable.value("cells").toList().value(3).toString(), macFormula);
        QCOMPARE(reopenedTablet.tableCellValues(receivedMacTable).value(3).toString(), QStringLiteral("21"));

        Notebook reopened(macPath);
        QVERIFY2(reopened.error().isEmpty(), qPrintable(reopened.error()));
        QVERIFY(reopened.openBook(copyId));
        QVERIFY(reopened.goToPage(0));
        const auto source = reopened.pageRenderContent().value("source").toMap();
        QCOMPARE(source.value("kind").toString(), QStringLiteral("pdf"));
        QCOMPARE(QByteArray::fromBase64(source.value("data").toString().toLatin1()), pdf);
        QCOMPARE(reopened.strokes(), highlights);
        QVERIFY(reopened.pageSummaries().value(0).toMap().value("annotated").toBool());
        const auto receivedNote = reopened.pageItems().value(0).toMap();
        QCOMPARE(receivedNote.value("text"), tabletNote.value("text"));
        QCOMPARE(receivedNote.value("noteInk"), tabletNote.value("noteInk"));
        QCOMPARE(receivedNote.value("x"), originalNote.value("x"));
        auto revisedNote = receivedNote;
        revisedNote.insert("text", "Mac: verified the replacement");
        QVERIFY(reopened.savePageNote(revisedNote, copyId, reopened.pageId(), receivedNote));
        QCOMPARE(reopened.pageItems().value(0).toMap().value("text"), revisedNote.value("text"));

        QVERIFY(reopened.goToPage(1));
        auto copyTable = reopened.pageItems().value(0).toMap();
        QCOMPARE(copyTable.value("cells").toList().value(3).toString(), originalFormula);
        QCOMPARE(reopened.tableCellValues(copyTable).value(3).toString(), QStringLiteral("4"));
        auto cells = copyTable.value("cells").toList();
        cells[2] = QStringLiteral("9");
        copyTable.insert("cells", cells);
        QVERIFY(reopened.savePageItem(copyTable));
        const auto editedCopy = reopened.pageItems().value(0).toMap();
        QCOMPARE(editedCopy.value("cells").toList().value(3).toString(), originalFormula);
        QCOMPARE(reopened.tableCellValues(editedCopy).value(3).toString(), QStringLiteral("9"));

        QVERIFY(reopened.openBook(bookId));
        QVERIFY(reopened.openPage(notePage));
        QCOMPARE(reopened.pageItems().value(0).toMap(), originalNote);
        QVERIFY(reopened.openPage(tablePage));
        const auto persistedTable = reopened.pageItems().value(0).toMap();
        QCOMPARE(persistedTable.value("cells").toList().value(3).toString(), macFormula);
        QCOMPARE(reopened.tableCellValues(persistedTable).value(3).toString(), QStringLiteral("21"));
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

    void pairedPdfDelivery_data()
    {
        QTest::addColumn<bool>("wifi");
        QTest::newRow("local-wifi") << true;
        QTest::newRow("bluetooth") << false;
    }

    void pairedPdfDelivery()
    {
        QFETCH(bool, wifi);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Notebook source(directory.filePath("source.json"));
        Notebook destination(directory.filePath("destination.json"));
        QVERIFY(source.createBook(QStringLiteral("Diagramma comunità")));
        const auto before = destination.exportLibraryForTransfer();
        const auto option = pdfOption(source);
        QVERIFY(!option.isEmpty());

        WirelessSync tablet(&source, directory.filePath("tablet-pairing"), true);
        WirelessSync mac(&destination, directory.filePath("mac-pairing"), false);
        const auto link = wifi ? WirelessSync::Link::Wifi : WirelessSync::Link::Bluetooth;
        for (auto *peer : {&tablet, &mac}) {
            preparePaired(*peer);
            peer->transfer()->setAutoSync(false);
            peer->m_locked = false;
            peer->m_link = link;
        }
        mac.m_pdfDownloadDirectory = directory.filePath("downloads");
        auto network = std::make_shared<WifiSyncLink::Loopback>();
        network->advertisedAddress = QStringLiteral("192.168.50.20");
        if (wifi) {
            tablet.m_wifi.m_loopback = network;
            mac.m_wifi.m_loopback = network;
            tablet.m_wifi.listen();
            QVERIFY(network->port != 0);
        } else {
            forwardBluetoothPackets(tablet.m_pipe, mac.m_pipe);
            forwardBluetoothPackets(mac.m_pipe, tablet.m_pipe);
        }

        const auto downloads = QDir(mac.m_pdfDownloadDirectory).filePath(QStringLiteral("Quaderno"));
        QVERIFY(QDir().mkpath(downloads));
        const auto originalName = option.value(QStringLiteral("name")).toString();
        QFile preexisting(QDir(downloads).filePath(originalName));
        QVERIFY(preexisting.open(QIODevice::WriteOnly));
        QCOMPARE(preexisting.write("existing file"), qint64(13));
        preexisting.close();

        QByteArray receivedBytes;
        auto capture = connect(&mac.m_channel, &SyncChannel::messageReceived, this,
            [&receivedBytes](const QVariantMap &message, const QByteArray &payload) {
                if (message.value(QStringLiteral("type")) == QStringLiteral("pdfShareChunk"))
                    receivedBytes.append(payload);
            });
        const auto transport = wifi ? QStringLiteral("wifi") : QStringLiteral("bluetooth");
        const auto alternate = wifi ? QStringLiteral("bluetooth") : QStringLiteral("wifi");
        QVERIFY(!tablet.sendPdf(&source, option, alternate));
        QVERIFY(tablet.pdfTransport().isEmpty());
        QCOMPARE(tablet.pdfTransferSupported(), QStringLiteral("unknown"));
        QVERIFY(tablet.sendPdf(&source, option, transport)); // Wait for the selected authenticated link.
        QVERIFY(tablet.pdfSending());
        QVERIFY(!tablet.sendPdf(&source, option, transport)); // Keep the pending export intact.
        if (wifi) {
            QVERIFY(mac.m_wifi.connectToHandoff(network->advertisedAddress));
        } else {
            tablet.transportReady(link);
            mac.transportReady(link);
        }
        QTRY_VERIFY(!mac.receivedPdf().isEmpty());
        QTRY_VERIFY(!tablet.pdfSending());
        QCOMPARE(tablet.pdfProgress(), 1.);
        QCOMPARE(tablet.pdfTransferSupported(), QStringLiteral("supported"));
        QCOMPARE(tablet.pdfTransport(), transport);
        QCOMPARE(destination.exportLibraryForTransfer(), before);
        QVERIFY(receivedBytes.startsWith("%PDF-"));
        QCOMPARE(QFileInfo(mac.receivedPdf().toLocalFile()).fileName(), originalName.chopped(4) + QStringLiteral(" (2).pdf"));
        QFile delivered(mac.receivedPdf().toLocalFile());
        QVERIFY(delivered.open(QIODevice::ReadOnly));
        QCOMPARE(delivered.readAll(), receivedBytes);
        delivered.close();
        QVERIFY(preexisting.open(QIODevice::ReadOnly));
        QCOMPARE(preexisting.readAll(), QByteArrayLiteral("existing file"));
        preexisting.close();
        mac.dismissReceivedPdf();
        QVERIFY(mac.receivedPdf().isEmpty());
        QVERIFY(delivered.exists());
        disconnect(capture);

        // A peer running an older Quaderno still exchanges notebook greetings.
        // Its absent PDF capability blocks the export without dropping the link.
        const auto request = QString(32, u'd');
        mac.m_channel.sendMessage({{QStringLiteral("type"), QStringLiteral("syncHello")},
            {QStringLiteral("request"), request}, {QStringLiteral("deltaVersion"), 1}}, {});
        QTRY_COMPARE(tablet.pdfTransferSupported(), QStringLiteral("unsupported"));
        QVERIFY(!tablet.sendPdf(&source, option, transport));
        QVERIFY(!tablet.pdfStatus().isEmpty());
        QCOMPARE(tablet.pdfProgress(), 0.);
        QVERIFY(tablet.transfer()->connected());

        // An invalid name and a corrupt transfer cannot create part files or
        // replace a previous delivery, even over the correct paired channel.
        QSignalSpy messages(&tablet.m_channel, &SyncChannel::messageReceived);
        QVariantMap offer{{QStringLiteral("name"), QStringLiteral("../Other.pdf")}, {QStringLiteral("size"), 10},
                          {QStringLiteral("sha256"), QString(64, u'0')}};
        tablet.sendPdfMessage(QStringLiteral("pdfShareOffer"), request, offer);
        QTRY_VERIFY(std::any_of(messages.cbegin(), messages.cend(), [](const auto &event) {
            return event.first().toMap().value(QStringLiteral("type")) == QStringLiteral("pdfShareAbort");
        }));
        messages.clear();
        offer.insert(QStringLiteral("name"), QStringLiteral("Corrupted.pdf"));
        tablet.sendPdfMessage(QStringLiteral("pdfShareOffer"), request, offer);
        QTRY_VERIFY(std::any_of(messages.cbegin(), messages.cend(), [](const auto &event) {
            return event.first().toMap().value(QStringLiteral("type")) == QStringLiteral("pdfShareAck");
        }));
        messages.clear();
        tablet.sendPdfMessage(QStringLiteral("pdfShareChunk"), request, {{QStringLiteral("offset"), 0}}, "%PDF-12345");
        QTRY_VERIFY(std::any_of(messages.cbegin(), messages.cend(), [](const auto &event) {
            return event.first().toMap().value(QStringLiteral("type")) == QStringLiteral("pdfShareAbort");
        }));
        QVERIFY(mac.receivedPdf().isEmpty());
        QCOMPARE(QDir(downloads).entryList(QDir::Files | QDir::Hidden).size(), 2);
        QCOMPARE(destination.exportLibraryForTransfer(), before);

        // Locking the receiver discards a pending partial PDF without deleting
        // already received files or reopening the connection.
        messages.clear();
        tablet.sendPdfMessage(QStringLiteral("pdfShareOffer"), request, offer);
        QTRY_VERIFY(std::any_of(messages.cbegin(), messages.cend(), [](const auto &event) {
            return event.first().toMap().value(QStringLiteral("type")) == QStringLiteral("pdfShareAck");
        }));
        mac.setLocked(true);
        QCOMPARE(QDir(downloads).entryList(QDir::Files | QDir::Hidden).size(), 2);
        tablet.setLocked(true);
    }

    void screenStreamRequiresPairedWifi_data()
    {
        QTest::addColumn<bool>("wifi");
        QTest::newRow("wifi") << true;
        QTest::newRow("bluetooth") << false;
    }

    void screenStreamRequiresPairedWifi()
    {
        QFETCH(bool, wifi);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Notebook source(directory.filePath("tablet.json"));
        Notebook destination(directory.filePath("mac.json"));
        WirelessSync tablet(&source, directory.filePath("tablet-pairing"), true);
        WirelessSync mac(&destination, directory.filePath("mac-pairing"), false);
        const QVariantMap frame{{QStringLiteral("type"), QStringLiteral("screen-frame")},
            {QStringLiteral("sequence"), 7}};
        const auto link = wifi ? WirelessSync::Link::Wifi : WirelessSync::Link::Bluetooth;
        for (auto *peer : {&tablet, &mac}) {
            QVERIFY(!peer->screenStreamAvailable());
            QVERIFY(!peer->sendScreenStreamMessage(frame, "before pairing"));
            preparePaired(*peer);
            peer->transfer()->setAutoSync(false);
            peer->m_locked = false;
            peer->m_link = link;
        }

        QSignalSpy linkEvents(&mac, &WirelessSync::screenStreamLinkChanged);
        QSignalSpy incoming(&mac, &WirelessSync::screenStreamMessageReceived);
        QSignalSpy traffic(&mac.m_channel, &SyncChannel::messageReceived);
        if (wifi) {
            QVERIFY(startDirectWifiPair(tablet, mac));
        } else {
            forwardBluetoothPackets(tablet.m_pipe, mac.m_pipe);
            forwardBluetoothPackets(mac.m_pipe, tablet.m_pipe);
            tablet.transportReady(link);
            mac.transportReady(link);
        }
        QTRY_VERIFY(tablet.transfer()->connected());
        QTRY_VERIFY(mac.transfer()->connected());
        if (wifi) {
            QTRY_VERIFY(tablet.screenStreamAvailable());
            QTRY_VERIFY(mac.screenStreamAvailable());
            QCOMPARE(linkEvents.size(), 1);

            QVariantMap largeMetadata = frame;
            largeMetadata.insert(QStringLiteral("large"), QString(4096, u'x'));
            const QByteArray maxPayload(WirelessSync::screenStreamPayloadLimit, 'x');
            const QByteArray oversizedPayload(WirelessSync::screenStreamPayloadLimit + 1, 'y');
            QVERIFY(!tablet.sendScreenStreamMessage({{QStringLiteral("type"), QStringLiteral("preview")}}, {}));
            QVERIFY(!tablet.sendScreenStreamMessage(largeMetadata, {}));
            QVERIFY(!tablet.sendScreenStreamMessage(frame, oversizedPayload));

            // Even a peer with valid pairing cannot bypass the feature limits.
            tablet.m_channel.sendMessage(frame, oversizedPayload);
            tablet.m_channel.sendMessage(largeMetadata, {});
            QVERIFY(tablet.sendScreenStreamMessage(frame, maxPayload));
            QTRY_COMPARE(incoming.size(), 1);
            QCOMPARE(incoming.first().at(0).toMap().value(QStringLiteral("sequence")).toInt(), 7);
            QCOMPARE(incoming.first().at(1).toByteArray(), maxPayload);

            // The next greeting models a peer which omits the optional capability.
            tablet.m_channel.sendMessage({{QStringLiteral("type"), QStringLiteral("syncHello")},
                {QStringLiteral("request"), QString(32, u'd')}, {QStringLiteral("deltaVersion"), 1}}, {});
            QTRY_VERIFY(!mac.screenStreamAvailable());
            QCOMPARE(linkEvents.size(), 2);
            QVERIFY(!mac.sendScreenStreamMessage(frame, "legacy"));
            QVERIFY(mac.transfer()->connected());
        } else {
            QVERIFY(!tablet.screenStreamAvailable());
            QVERIFY(!mac.screenStreamAvailable());
            QVERIFY(!tablet.sendScreenStreamMessage(frame, "over Bluetooth"));
            traffic.clear();
            tablet.m_channel.sendMessage(frame, "unexpected Bluetooth frame");
            QTRY_VERIFY(std::any_of(traffic.cbegin(), traffic.cend(), [](const auto &event) {
                return event.first().toMap().value(QStringLiteral("type")) == QStringLiteral("screen-frame");
            }));
            QVERIFY(incoming.isEmpty());
            QCOMPARE(linkEvents.size(), 0);
        }
        tablet.setLocked(true);
        QVERIFY(!tablet.screenStreamAvailable());
        QVERIFY(!tablet.sendScreenStreamMessage(frame, "after lock"));
        QTRY_VERIFY(!mac.screenStreamAvailable());
        mac.setLocked(true);
    }

    void screenStreamReceiverRequiresViewerAndClearsFrame()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Notebook source(directory.filePath("tablet.json"));
        Notebook destination(directory.filePath("mac.json"));
        WirelessSync tablet(&source, directory.filePath("tablet-pairing"), true);
        WirelessSync mac(&destination, directory.filePath("mac-pairing"), false);
        ScreenStream receiver(&mac);
        receiver.setBlocked(false);

        const auto reconnect = [&] {
            for (auto *peer : {&tablet, &mac}) {
                preparePaired(*peer);
                peer->transfer()->setAutoSync(false);
                peer->m_locked = false;
                peer->m_link = WirelessSync::Link::Wifi;
            }
            return startDirectWifiPair(tablet, mac);
        };
        QVERIFY(reconnect());
        QTRY_VERIFY(tablet.screenStreamAvailable());
        QTRY_VERIFY(receiver.available());
        QSignalSpy replies(&tablet, &WirelessSync::screenStreamMessageReceived);
        QSignalSpy traffic(&mac, &WirelessSync::screenStreamMessageReceived);

        QBuffer encoded;
        QVERIFY(encoded.open(QIODevice::WriteOnly));
        QImage image(16, 16, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(&encoded, "JPEG"));
        QVERIFY(!encoded.data().isEmpty());
        QVERIFY(encoded.size() <= WirelessSync::screenStreamPayloadLimit);
        const QString session(32, u'a');
        const QVariantMap start{{QStringLiteral("type"), QStringLiteral("screen-start")},
            {QStringLiteral("session"), session}};
        const QVariantMap stop{{QStringLiteral("type"), QStringLiteral("screen-stop")},
            {QStringLiteral("session"), session}};
        const QVariantMap frame{{QStringLiteral("type"), QStringLiteral("screen-frame")},
            {QStringLiteral("session"), session}, {QStringLiteral("sequence"), 1}};
        const auto answered = [&](const QString &type) {
            return std::any_of(replies.cbegin(), replies.cend(), [&](const auto &event) {
                const auto message = event.first().toMap();
                return message.value(QStringLiteral("type")) == type
                    && message.value(QStringLiteral("session")) == session;
            });
        };

        QVERIFY(tablet.sendScreenStreamMessage(start, {}));
        QTRY_VERIFY(answered(QStringLiteral("screen-stop")));
        QVERIFY(!receiver.active());
        QVERIFY(receiver.frameSource().isEmpty());

        receiver.setViewerOpen(true);
        replies.clear();
        QVERIFY(tablet.sendScreenStreamMessage(start, {}));
        QVERIFY(tablet.sendScreenStreamMessage(frame, encoded.data()));
        QTRY_VERIFY(receiver.active());
        QTRY_VERIFY(!receiver.frameSource().isEmpty());
        QCOMPARE(receiver.frameOrientation(), 0); // Legacy frames carry no orientation field.
        QTRY_VERIFY(answered(QStringLiteral("screen-ack")));
        QVERIFY(tablet.sendScreenStreamMessage(stop, {}));
        QTRY_VERIFY(!receiver.active());
        QVERIFY(receiver.frameSource().isEmpty());

        for (const auto &orientation : {QVariant(45), QVariant(90.5), QVariant(QStringLiteral("90")), QVariant(true)}) {
            auto malformed = frame;
            malformed.insert(QStringLiteral("orientation"), orientation);
            replies.clear();
            QVERIFY(tablet.sendScreenStreamMessage(start, {}));
            QTRY_VERIFY(receiver.active());
            QVERIFY(tablet.sendScreenStreamMessage(malformed, encoded.data()));
            QTRY_VERIFY(answered(QStringLiteral("screen-stop")));
            QVERIFY(!receiver.active());
            QVERIFY(receiver.frameSource().isEmpty());
            QCOMPARE(receiver.frameOrientation(), 0);
        }

        QVERIFY(tablet.sendScreenStreamMessage(start, {}));
        QVERIFY(tablet.sendScreenStreamMessage(frame, encoded.data()));
        QTRY_VERIFY(!receiver.frameSource().isEmpty());
        mac.setLocked(true);
        QVERIFY(!receiver.active());
        QVERIFY(receiver.frameSource().isEmpty());
        QTRY_VERIFY(!tablet.screenStreamAvailable());

        QVERIFY(reconnect());
        QTRY_VERIFY(tablet.screenStreamAvailable());
        QTRY_VERIFY(receiver.available());
        QVERIFY(!receiver.active());
        traffic.clear();
        QVERIFY(tablet.sendScreenStreamMessage(frame, encoded.data()));
        QTRY_COMPARE(traffic.size(), 1);
        QVERIFY(!receiver.active());
        QVERIFY(receiver.frameSource().isEmpty());
    }

    void screenStreamCapturesUpdatedWindowAfterExplicitStart()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Notebook source(directory.filePath("tablet.json"));
        Notebook destination(directory.filePath("mac.json"));
        WirelessSync tablet(&source, directory.filePath("tablet-pairing"), true);
        WirelessSync mac(&destination, directory.filePath("mac-pairing"), false);
        ScreenStream sender(&tablet);
        ScreenStream receiver(&mac);
        QQuickWindow window;
        window.setGeometry(0, 0, 320, 440);
        window.setColor(Qt::white);
        window.show();
        QTRY_VERIFY(window.isExposed());
        sender.attach(&window);
        sender.setBlocked(false);
        receiver.setBlocked(false);
        receiver.setViewerOpen(true);

        for (auto *peer : {&tablet, &mac}) {
            preparePaired(*peer);
            peer->transfer()->setAutoSync(false);
            peer->m_locked = false;
            peer->m_link = WirelessSync::Link::Wifi;
        }
        QVERIFY(startDirectWifiPair(tablet, mac));
        QTRY_VERIFY(sender.available());
        QVERIFY(!sender.active());
        QVERIFY(!receiver.active());
        QVERIFY(receiver.frameSource().isEmpty());

        sender.start();
        QTRY_VERIFY(!receiver.frameSource().isEmpty());
        QCOMPARE(receiver.frameOrientation(), 0);
        const QUrl firstSource = receiver.frameSource();
        std::unique_ptr<QQuickImageProvider> provider(receiver.createImageProvider());
        const auto originalFrame = provider->requestImage(firstSource.fileName(), nullptr, {});
        QVERIFY(!originalFrame.isNull());

        // The Paper Pro's window stays physically portrait; the logical UI
        // angle can change while the raw encoded image still has the same size.
        window.setProperty("appliedOrientation", 90);
        window.update();
        QTRY_COMPARE(receiver.frameOrientation(), 90);
        const auto rotatedSource = receiver.frameSource();
        QVERIFY(rotatedSource != firstSource);
        QCOMPARE(provider->requestImage(rotatedSource.fileName(), nullptr, {}), originalFrame);

        window.setProperty("appliedOrientation", 270);
        window.setColor(Qt::black);
        QTRY_COMPARE(receiver.frameOrientation(), 270);
        QVERIFY(receiver.frameSource() != rotatedSource);
        const QImage latest = provider->requestImage(receiver.frameSource().fileName(), nullptr, {});
        QVERIFY(!latest.isNull());
        QVERIFY(latest.pixelColor(80, 80).value() < 30);

        sender.setBlocked(true);
        QVERIFY(!sender.active());
        QTRY_VERIFY(!receiver.active());
        QVERIFY(receiver.frameSource().isEmpty());
        QCOMPARE(receiver.frameOrientation(), 0);
        QVERIFY(provider->requestImage(firstSource.fileName(), nullptr, {}).isNull());
    }

    void screenStreamRemoteNavigationIsScopedToAnOptedInSession()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Notebook source(directory.filePath("tablet.json"));
        Notebook destination(directory.filePath("mac.json"));
        WirelessSync tablet(&source, directory.filePath("tablet-pairing"), true);
        WirelessSync mac(&destination, directory.filePath("mac-pairing"), false);
        ScreenStream sender(&tablet);
        ScreenStream receiver(&mac);
        QQuickWindow window;
        window.setGeometry(0, 0, 320, 440);
        window.show();
        QTRY_VERIFY(window.isExposed());
        sender.attach(&window);
        sender.setBlocked(false);
        receiver.setBlocked(false);
        receiver.setViewerOpen(true);

        for (auto *peer : {&tablet, &mac}) {
            preparePaired(*peer);
            peer->transfer()->setAutoSync(false);
            peer->m_locked = false;
            peer->m_link = WirelessSync::Link::Wifi;
        }
        QVERIFY(startDirectWifiPair(tablet, mac));
        QTRY_VERIFY(sender.available());
        QSignalSpy requests(&sender, &ScreenStream::remoteTapRequested);
        QSignalSpy keys(&sender, &ScreenStream::remoteKeyRequested);
        QSignalSpy states(&mac, &WirelessSync::screenStreamMessageReceived);
        const auto latestMessage = [&](const QString &type) {
            for (auto event = states.crbegin(); event != states.crend(); ++event) {
                const auto message = event->first().toMap();
                if (message.value("type") == type)
                    return message;
            }
            return QVariantMap{};
        };
        const auto tap = [](const QString &session, const QVariant &sequence, const QString &section,
                            const QVariant &x, const QVariant &y) {
            return QVariantMap{{QStringLiteral("type"), QStringLiteral("screen-tap")},
                {QStringLiteral("session"), session}, {QStringLiteral("section"), section},
                {QStringLiteral("sequence"), sequence}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
        };
        const auto key = [](const QString &session, const QString &section, const QVariant &action, const QVariant &text) {
            return QVariantMap{{QStringLiteral("type"), QStringLiteral("screen-key")},
                {QStringLiteral("session"), session}, {QStringLiteral("section"), section},
                {QStringLiteral("action"), action}, {QStringLiteral("text"), text}};
        };
        const auto currentSession = [&] { return latestMessage(QStringLiteral("screen-start")).value("session").toString(); };
        const auto currentFrame = [&] { return latestMessage(QStringLiteral("screen-frame")).value("sequence").toInt(); };

        sender.start();
        QTRY_VERIFY(!receiver.frameSource().isEmpty());
        QVERIFY(!sender.remoteNavigationAllowed());
        QVERIFY(!receiver.remoteNavigationAllowed());
        const QString oldSession = currentSession();
        QVERIFY(!oldSession.isEmpty());
        receiver.requestTap(0.25, 0.75);
        receiver.requestKey(QStringLiteral("text"), QStringLiteral("view-only"));
        QVERIFY(mac.sendScreenStreamMessage(tap(oldSession, currentFrame(), QStringLiteral("books"), 0.25, 0.75), {}));
        QVERIFY(mac.sendScreenStreamMessage(key(oldSession, QStringLiteral("books"), QStringLiteral("text"),
            QStringLiteral("view-only")), {}));
        receiver.stop();
        QTRY_VERIFY(!sender.active());
        QVERIFY(requests.isEmpty());
        QVERIFY(keys.isEmpty());

        states.clear();
        receiver.setViewerOpen(true);
        QTRY_VERIFY(sender.available());
        sender.startWithRemoteNavigation();
        QVERIFY(sender.remoteNavigationAllowed());
        QTRY_VERIFY(receiver.remoteNavigationAllowed());
        QTRY_VERIFY(!receiver.frameSource().isEmpty());
        const QString session = currentSession();
        QVERIFY(!session.isEmpty() && session != oldSession);
        QVERIFY(receiver.remoteSection().isEmpty());
        receiver.requestTap(0.25, 0.75);
        sender.reportSection(QStringLiteral("diagrams"));
        QTRY_COMPARE(receiver.remoteSection(), QStringLiteral("diagrams"));
        const QString diagrams = QStringLiteral("diagrams");
        const int frame = currentFrame();
        QVERIFY(frame > 0);
        QVERIFY(mac.sendScreenStreamMessage(tap(oldSession, frame, diagrams, 0.25, 0.75), {}));
        QVERIFY(mac.sendScreenStreamMessage(tap(session, frame + 1, diagrams, 0.25, 0.75), {}));
        QVERIFY(mac.sendScreenStreamMessage(tap(session, frame, QStringLiteral("charts"), 0.25, 0.75), {}));
        QVERIFY(mac.sendScreenStreamMessage(tap(session, frame, diagrams, QStringLiteral("0.25"), 0.75), {}));
        QVERIFY(mac.sendScreenStreamMessage(tap(session, frame, diagrams, true, 0.75), {}));
        QVERIFY(mac.sendScreenStreamMessage(tap(session, frame, diagrams, -0.01, 0.75), {}));
        QVERIFY(mac.sendScreenStreamMessage(tap(session, frame, diagrams, 0.25, 1.0), {}));
        QVERIFY(mac.sendScreenStreamMessage(tap(session, frame, diagrams, 0.25, 0.75), "unexpected payload"));
        receiver.requestTap(0.25, 1.0);
        receiver.requestTap(0.25, 0.75);
        QTRY_COMPARE(requests.size(), 1);
        QCOMPARE(requests.last().at(0).toDouble(), 0.25);
        QCOMPARE(requests.last().at(1).toDouble(), 0.75);
        sender.reportSection({});
        QTRY_VERIFY(receiver.remoteSection().isEmpty());
        sender.reportSection(QStringLiteral("tables"));
        QTRY_COMPARE(receiver.remoteSection(), QStringLiteral("tables"));

        const QString note = QStringLiteral("book-note");
        const QString textAction = QStringLiteral("text");
        const QString unicode = QStringLiteral("Città ✍️");
        sender.reportSection(note);
        QTRY_COMPARE(receiver.remoteSection(), note);
        QVERIFY(mac.sendScreenStreamMessage(key(oldSession, note, textAction, unicode), {}));
        QVERIFY(mac.sendScreenStreamMessage(key(session, QStringLiteral("tables"), textAction, unicode), {}));
        QVERIFY(mac.sendScreenStreamMessage(key(session, note, textAction, 42), {}));
        QVERIFY(mac.sendScreenStreamMessage(key(session, note, textAction, QString(257, u'a')), {}));
        QVERIFY(mac.sendScreenStreamMessage(key(session, note, textAction, unicode), "unexpected payload"));
        receiver.requestKey(textAction, QStringLiteral("a\nb"));
        receiver.requestKey(QStringLiteral("escape"), {});
        receiver.requestKey(QStringLiteral("select-all"), QStringLiteral("unexpected text"));
        receiver.requestKey(textAction, unicode);
        receiver.requestKey(textAction, QString(256, u'b'));
        receiver.requestKey(QStringLiteral("new-line"), {});
        QTRY_COMPARE(keys.size(), 3);
        QCOMPARE(keys.at(0).at(0).toString(), textAction);
        QCOMPARE(keys.at(0).at(1).toString(), unicode);
        QCOMPARE(keys.at(1).at(1).toString(), QString(256, u'b'));
        QCOMPARE(keys.at(2).at(0).toString(), QStringLiteral("new-line"));
        QVERIFY(keys.at(2).at(1).toString().isEmpty());

        sender.setBlocked(true);
        QVERIFY(!sender.remoteNavigationAllowed());
        QTRY_VERIFY(!receiver.remoteNavigationAllowed());
        QVERIFY(receiver.remoteSection().isEmpty());
        sender.setBlocked(false);
        sender.start();
        QTRY_VERIFY(!receiver.frameSource().isEmpty());
        QVERIFY(!receiver.remoteNavigationAllowed());

        receiver.setRemoteNavigationAllowed(true);
        QVERIFY(!receiver.remoteNavigationAllowed());
        sender.setRemoteNavigationAllowed(true);
        sender.reportSection(QStringLiteral("tables"));
        QVERIFY(sender.remoteNavigationAllowed());
        QTRY_COMPARE(receiver.remoteSection(), QStringLiteral("tables"));
        QVERIFY(receiver.remoteNavigationAllowed());

        const qsizetype accepted = requests.size();
        const qsizetype acceptedKeys = keys.size();
        QSignalSpy permissionUpdates(&receiver, &ScreenStream::navigationChanged);
        receiver.requestTap(0.5, 0.5);
        receiver.requestKey(textAction, QStringLiteral("old grant"));
        sender.setRemoteNavigationAllowed(false);
        sender.setRemoteNavigationAllowed(true);
        sender.reportSection(QStringLiteral("tables"));
        QTRY_COMPARE(permissionUpdates.size(), 3);
        QCOMPARE(receiver.remoteSection(), QStringLiteral("tables"));
        receiver.requestTap(0.0, 0.0);
        receiver.requestKey(QStringLiteral("backspace"), {});
        QTRY_COMPARE(requests.size(), accepted + 1);
        QTRY_COMPARE(keys.size(), acceptedKeys + 1);
        QCOMPARE(requests.last().at(0).toDouble(), 0.0);
        QCOMPARE(requests.last().at(1).toDouble(), 0.0);
        QCOMPARE(keys.last().at(0).toString(), QStringLiteral("backspace"));

        sender.setRemoteNavigationAllowed(false);
        QTRY_VERIFY(!receiver.remoteNavigationAllowed());
        QVERIFY(receiver.remoteSection().isEmpty());
        QVERIFY(sender.active() && receiver.active());
        sender.setBlocked(true);
        sender.setRemoteNavigationAllowed(true);
        QVERIFY(!sender.remoteNavigationAllowed());
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
