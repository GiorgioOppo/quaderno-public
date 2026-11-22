#pragma once

#include "blepipe.h"
#include "blesync.h"
#include "syncrendezvous.h"
#include "syncchannel.h"
#include "wifisynclink.h"

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <memory>

#ifdef Q_OS_MACOS
#include "blecentral.h"
#elif defined(Q_OS_LINUX)
#include "bleperipheral.h"
#endif

Q_MOC_INCLUDE("notebook.h")

class WirelessSync final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(BleSync *transfer READ transfer CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool tablet READ tablet CONSTANT)
    Q_PROPERTY(bool paired READ paired NOTIFY pairedChanged)
    Q_PROPERTY(bool active READ active NOTIFY linkChanged)
    Q_PROPERTY(bool bluetoothActive READ bluetoothActive NOTIFY linkChanged)
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)
    Q_PROPERTY(QString savedWifiAddress READ savedWifiAddress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY changed)
    Q_PROPERTY(bool activatingBluetooth READ activatingBluetooth NOTIFY changed)
    Q_PROPERTY(bool automaticAllowed READ automaticAllowed WRITE setAutomaticAllowed NOTIFY changed)
    Q_PROPERTY(bool automaticWifiOnly READ automaticWifiOnly WRITE setAutomaticWifiOnly NOTIFY automaticPolicyChanged)
    Q_PROPERTY(QString automaticTransport READ automaticTransport WRITE setAutomaticTransport NOTIFY automaticPolicyChanged)
    Q_PROPERTY(bool automaticWifiAllowed READ automaticWifiAllowed WRITE setAutomaticWifiAllowed NOTIFY automaticPolicyChanged)
    Q_PROPERTY(bool reconnectPending READ reconnectPending NOTIFY reconnectionChanged)
    Q_PROPERTY(int reconnectDelaySeconds READ reconnectDelaySeconds NOTIFY reconnectionChanged)
    Q_PROPERTY(bool connectionPaused READ connectionPaused NOTIFY reconnectionChanged)
    Q_PROPERTY(QString pdfTransport READ pdfTransport NOTIFY pdfChanged)
    Q_PROPERTY(QString pdfTransferSupported READ pdfTransferSupported NOTIFY pdfChanged)
    Q_PROPERTY(bool pdfSending READ pdfSending NOTIFY pdfChanged)
    Q_PROPERTY(qreal pdfProgress READ pdfProgress NOTIFY pdfChanged)
    Q_PROPERTY(QString pdfStatus READ pdfStatus NOTIFY pdfChanged)
    Q_PROPERTY(QUrl receivedPdf READ receivedPdf NOTIFY pdfChanged)

public:
    WirelessSync(Notebook *notebook, const QString &pairingPath, bool tablet, bool canActivateBluetooth = false,
                 QObject *parent = nullptr);
    ~WirelessSync() override;
    BleSync *transfer() { return &m_transfer; }
    bool available() const;
    bool tablet() const { return m_tablet; }
    bool paired() const { return !m_secret.isEmpty(); }
    bool active() const { return m_link != Link::None; }
    bool bluetoothActive() const { return m_link == Link::Bluetooth; }
    QString code() const;
    QString address() const;
    QString savedWifiAddress() const { return m_preferredAddress; }
    QString status() const { return m_status; }
    QString bluetoothStatus() const;
    bool activatingBluetooth() const { return m_activateBluetooth.state() != QProcess::NotRunning; }
    void setLocked(bool locked);
    bool automaticAllowed() const { return m_automaticAllowed; }
    bool automaticWifiOnly() const { return m_automaticTransport == QStringLiteral("wifi"); }
    QString automaticTransport() const { return m_automaticTransport; }
    bool automaticWifiAllowed() const { return m_automaticWifiAllowed; }
    bool reconnectPending() const { return m_reconnect.isActive() || m_listenerRetry.isActive(); }
    int reconnectDelaySeconds() const { return m_reconnectDelayMs / 1000; }
    bool connectionPaused() const { return m_paused; }
    QString pdfTransport() const;
    QString pdfTransferSupported() const;
    bool pdfSending() const;
    qreal pdfProgress() const;
    QString pdfStatus() const { return m_pdfStatus; }
    QUrl receivedPdf() const { return m_receivedPdf; }
    static constexpr qsizetype screenStreamPayloadLimit = 512 * 1024;
    bool screenStreamAvailable() const;
    bool sendScreenStreamMessage(const QVariantMap &metadata, const QByteArray &payload);
    void setAutomaticAllowed(bool allowed);
    void setAutomaticWifiOnly(bool wifiOnly);
    void setAutomaticTransport(const QString &transport);
    void setAutomaticWifiAllowed(bool allowed);

    Q_INVOKABLE void preparePairing();
    Q_INVOKABLE void refreshBluetooth();
    Q_INVOKABLE void startBluetooth(const QString &code);
    Q_INVOKABLE void startWifi(const QString &address, const QString &code);
    Q_INVOKABLE bool validWifiAddress(const QString &address) const { return WifiSyncLink::isSupportedAddress(address); }
    Q_INVOKABLE void stop();
    Q_INVOKABLE void forget();
    Q_INVOKABLE bool resumeAutomaticConnection();
    Q_INVOKABLE bool sendPdf(Notebook *notebook, const QVariantMap &option, const QString &transport);
    Q_INVOKABLE void cancelPdf();
    Q_INVOKABLE bool openReceivedPdf();
    Q_INVOKABLE void dismissReceivedPdf();

signals:
    void changed();
    void pairedChanged();
    void linkChanged();
    void automaticPolicyChanged();
    void reconnectionChanged();
    void pdfChanged();
    void screenStreamLinkChanged();
    void screenStreamMessageReceived(const QVariantMap &metadata, const QByteArray &payload);

private:
    friend class WirelessSyncTests;
    enum class Link { None, Bluetooth, Wifi, Waiting };
    enum class Rendezvous { Inactive, Discovering, Idle, Connecting, Wifi, Bluetooth };
    enum class PdfSupport { Unknown, Supported, Unsupported };
    struct PdfTransfer;
    bool prepareConnection(Link link, const QString &code, bool automatic = false);
    void connectBluetooth(const QString &code, bool allowActivation, bool automatic = false);
    void connectWifi(const QString &address, const QString &code, bool automatic);
    void startAutomaticListeners();
    void retryMissingListeners();
    void scheduleListenerRetry();
    void transportReady(Link link);
    void transportEnded(Link link, const QString &message);
    void activateBluetooth();
    void startChannel();
    void acceptPeer();
    bool canRendezvous() const;
    bool canOfferWifi() const;
    void beginRendezvous();
    void updateRendezvous();
    void receivePrimaryMessage(const QVariantMap &metadata, const QByteArray &payload);
    void receiveHandoffMessage(const QVariantMap &metadata, const QByteArray &payload);
    void sendRendezvous(SyncRendezvous::Kind kind, const QString &address = {});
    void useHandoffWifi();
    void fallbackRendezvous(bool notifyPeer);
    void linkEnded(const QString &message);
    void report(const QString &message);
    bool storePairing(const QByteArray &secret);
    void loadPairing();
    void updatePeer();
    void resetLink();
    void scheduleReconnect();
    Link automaticLink() const;
    bool canReconnectAutomatically() const;
    void updateAutomaticTransferGate();
    void enforceAutomaticPolicy();
    void stopReconnectTimers();
    static int reconnectDelayMs(bool tablet, int failures);
    void saveConnection();
    void handlePeerCapabilities(const QVariantMap &metadata);
    const SyncChannel *screenStreamChannel() const;
    void handleScreenStreamCapability(const SyncChannel &channel, const QVariantMap &metadata, const QByteArray &payload);
    bool receiveScreenStreamMessage(const SyncChannel &channel, const QVariantMap &metadata, const QByteArray &payload);
    void resetScreenStreamTransport();
    bool receivePdfMessage(const QVariantMap &metadata, const QByteArray &payload);
    void beginPdfOffer();
    void sendPdfChunk();
    void sendPdfMessage(const QString &type, const QString &request, QVariantMap metadata = {},
                        const QByteArray &payload = {});
    bool startPdfReceive(const QVariantMap &metadata, const QString &request);
    bool writePdfChunk(const QVariantMap &metadata, const QByteArray &payload);
    QUrl storeReceivedPdf();
    void finishPdf(const QString &status, bool completed = false);
    void setPdfStatus(const QString &status);
    void resetPdfTransport();
    bool transportSelectionBusy() const;
    static qsizetype pdfMaximumSize(Link link);

    const QString m_pairingPath;
    const bool m_tablet;
    const bool m_canActivateBluetooth;
    BleSync m_transfer;
    SyncChannel m_channel;
    SyncChannel m_handoffWifi;
    BlePipe m_pipe;
    WifiSyncLink m_wifi;
#ifdef Q_OS_MACOS
    BleCentral m_bluetooth;
#elif defined(Q_OS_LINUX)
    BlePeripheral m_bluetooth;
#endif
    Link m_link = Link::None;
    Link m_channelLink = Link::None;
    Link m_preferredLink = Link::None;
    QString m_preferredAddress;
    QTimer m_reconnect;
    QTimer m_listenerRetry;
    QTimer m_rendezvousDeadline;
    Rendezvous m_rendezvous = Rendezvous::Inactive;
    QString m_rendezvousSession;
    bool m_localWork = false;
    bool m_localWifi = false;
    bool m_peerWork = false;
    bool m_peerWifi = false;
    QProcess m_activateBluetooth;
    QTimer m_activationDeadline;
    bool m_paused = false;
    bool m_automaticAllowed = true;
    QString m_automaticTransport = QStringLiteral("auto");
    bool m_automaticWifiAllowed = true;
    bool m_automaticConnection = false;
    bool m_wifiCandidate = false;
    bool m_bluetoothCandidate = false;
    bool m_directWifiNext = false;
    bool m_resettingLink = false;
    int m_reconnectFailures = 0;
    int m_reconnectDelayMs = 0;
    bool m_locked = true;
    bool m_tryBluetoothActivation = false;
    QByteArray m_secret;
    QByteArray m_pendingSecret;
    QString m_status;
    QString m_bluetoothStatus;
    std::unique_ptr<PdfTransfer> m_pdf;
    QTimer m_pdfDeadline;
    PdfSupport m_pdfSupport = PdfSupport::Unknown;
    QString m_pdfStatus;
    QUrl m_receivedPdf;
    QString m_pdfDownloadDirectory; // Isolated destination in protocol tests.
    bool m_pdfCompleted = false;
    bool m_screenStreamPeerSupports = false;
};
