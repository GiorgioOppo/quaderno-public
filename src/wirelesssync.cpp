#include "wirelesssync.h"

#ifdef Q_OS_LINUX
#include "paperprobluetooth.h"
#endif

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <openssl/rand.h>

namespace {
constexpr qsizetype bluetoothLimit = 8 * 1024 * 1024;
constexpr qsizetype wifiLimit = 64 * 1024 * 1024;
constexpr qsizetype secretBytes = 16;
constexpr int rendezvousDiscoveryMs = 4000;
constexpr int rendezvousConnectionMs = 6000;
constexpr auto privatePermissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

QByteArray parseCode(const QString &value)
{
    QString normalized = value.trimmed().toLower();
    normalized.remove(QLatin1Char('-'));
    normalized.remove(QLatin1Char(' '));
    const auto text = normalized.toLatin1();
    const auto bytes = QByteArray::fromHex(text);
    return bytes.size() == secretBytes && bytes.toHex() == text ? bytes : QByteArray{};
}

QByteArray derive(const QByteArray &secret, const QByteArray &purpose)
{
    return QCryptographicHash::hash("Quaderno wireless v1/" + purpose + '/' + secret, QCryptographicHash::Sha256);
}

QSettings wirelessSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
}
}

WirelessSync::WirelessSync(Notebook *notebook, const QString &pairingPath, bool tablet, bool canActivateBluetooth,
                           QObject *parent)
    : QObject(parent), m_pairingPath(pairingPath), m_tablet(tablet), m_canActivateBluetooth(canActivateBluetooth),
      m_transfer(notebook), m_pipe(tablet)
{
    loadPairing();
    m_reconnect.setSingleShot(true);
    connect(&m_reconnect, &QTimer::timeout, this, [this] {
        m_reconnectDelayMs = 0;
        emit reconnectionChanged();
        // Recheck policy at execution time; a pending timer cannot bypass a lock,
        // network restriction or a switch from Bluetooth to Wi-Fi automation.
        if (!canReconnectAutomatically()) return;
        if (automaticLink() == Link::Bluetooth)
            connectBluetooth({}, false, true);
        else if (automaticLink() == Link::Wifi)
            connectWifi(m_preferredAddress, {}, true);
        else if (automaticLink() == Link::Waiting)
            startAutomaticListeners();
    });
    m_listenerRetry.setSingleShot(true);
    connect(&m_listenerRetry, &QTimer::timeout, this, [this] {
        m_reconnectDelayMs = 0;
        emit reconnectionChanged();
        retryMissingListeners();
    });
    connect(&m_transfer, &BleSync::changed, this, &WirelessSync::scheduleReconnect);
    m_activationDeadline.setSingleShot(true);
    connect(&m_activationDeadline, &QTimer::timeout, &m_activateBluetooth, &QProcess::kill);
    connect(&m_activateBluetooth, &QProcess::stateChanged, this, [this] { emit changed(); });
    connect(&m_activateBluetooth, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_link == Link::Bluetooth && error == QProcess::FailedToStart) {
            m_activationDeadline.stop();
            linkEnded(tr("Impossibile attivare il Bluetooth sul Paper Pro."));
        }
    });
    connect(&m_activateBluetooth, &QProcess::finished, this, [this](int result, QProcess::ExitStatus exitStatus) {
        m_activationDeadline.stop();
        const auto diagnostics = m_activateBluetooth.readAll().trimmed();
        if (m_link != Link::Bluetooth) {
            refreshBluetooth();
            return;
        }
#ifdef Q_OS_LINUX
        if (exitStatus != QProcess::NormalExit || result != 0) {
            qWarning().noquote() << "Paper Pro Bluetooth activation:" << diagnostics;
            linkEnded(tr("Impossibile attivare il Bluetooth sul Paper Pro."));
            return;
        }
        report(tr("Bluetooth attivo sul Paper Pro."));
        m_bluetooth.start();
#endif
    });
    m_rendezvousDeadline.setSingleShot(true);
    connect(&m_rendezvousDeadline, &QTimer::timeout, this, [this] { fallbackRendezvous(true); });
    connect(&m_transfer, &BleSync::syncWorkChanged, this, &WirelessSync::updateRendezvous, Qt::QueuedConnection);
    connect(&m_transfer, &BleSync::transportSelectionBusyChanged, this, &WirelessSync::updateRendezvous, Qt::QueuedConnection);
    connect(&m_transfer, &BleSync::sendMessage, this, [this](const QVariantMap &metadata, const QByteArray &payload) {
        auto &channel = m_rendezvous == Rendezvous::Wifi ? m_handoffWifi : m_channel;
        channel.sendMessage(metadata, payload);
    });
    connect(&m_channel, &SyncChannel::messageReceived, this, &WirelessSync::receivePrimaryMessage);
    connect(&m_channel, &SyncChannel::establishedChanged, this, [this] {
        if (m_channel.established())
            acceptPeer();
        else if (m_rendezvous != Rendezvous::Wifi)
            m_transfer.setConnected(false);
    });
    connect(&m_channel, &SyncChannel::encryptedReady, this, [this](const QByteArray &bytes) {
        if (m_channelLink == Link::Bluetooth)
            m_pipe.enqueue(bytes);
        else if (m_channelLink == Link::Wifi)
            m_wifi.send(bytes);
    });
    connect(&m_channel, &SyncChannel::failed, this, [this](const QString &message) {
        if (m_rendezvous == Rendezvous::Wifi)
            transportEnded(Link::Bluetooth, message);
        else
            linkEnded(message);
    });
    connect(&m_pipe, &BlePipe::received, this, [this](const QByteArray &bytes) {
        if (m_channelLink == Link::Bluetooth) m_channel.receive(bytes);
    });
    connect(&m_pipe, &BlePipe::failed, this, [this] {
        transportEnded(Link::Bluetooth, tr("Lo scambio Bluetooth si è interrotto. Ricollega il dispositivo."));
    }, Qt::QueuedConnection);
    connect(&m_wifi, &WifiSyncLink::ready, this, [this] { transportReady(Link::Wifi); });
    connect(&m_wifi, &WifiSyncLink::received, this, [this](const QByteArray &bytes) {
        if (m_rendezvous == Rendezvous::Connecting || m_rendezvous == Rendezvous::Wifi)
            m_handoffWifi.receive(bytes);
        else if (m_channelLink == Link::Wifi)
            m_channel.receive(bytes);
    });
    connect(&m_wifi, &WifiSyncLink::disconnected, this, [this](const QString &message) {
        transportEnded(Link::Wifi, message);
    });
    connect(&m_wifi, &WifiSyncLink::changed, this, [this] {
        if (m_link == Link::Wifi && m_rendezvous == Rendezvous::Inactive && !m_channel.established())
            report(m_wifi.status());
    });
    connect(&m_handoffWifi, &SyncChannel::encryptedReady, &m_wifi, &WifiSyncLink::send);
    connect(&m_handoffWifi, &SyncChannel::messageReceived, this, &WirelessSync::receiveHandoffMessage);
    connect(&m_handoffWifi, &SyncChannel::establishedChanged, this, [this] {
        if (!m_tablet && m_rendezvous == Rendezvous::Connecting && m_handoffWifi.established())
            sendRendezvous(SyncRendezvous::Kind::Ready);
    });
    connect(&m_handoffWifi, &SyncChannel::failed, this, [this] { fallbackRendezvous(true); });
#ifdef Q_OS_MACOS
    m_pipe.setOutput([this](const QByteArray &chunk) { return m_bluetooth.writeChunk(chunk); },
                     [this] { return m_bluetooth.maximumChunkSize(); });
    connect(&m_bluetooth, &BleCentral::ready, this, [this] { transportReady(Link::Bluetooth); });
    connect(&m_bluetooth, &BleCentral::received, &m_pipe, &BlePipe::receive);
    connect(&m_bluetooth, &BleCentral::writable, &m_pipe, &BlePipe::writable);
    connect(&m_bluetooth, &BleCentral::statusChanged, this, [this](const QString &status) {
        if (m_link == Link::Bluetooth)
            report(status);
    });
    connect(&m_bluetooth, &BleCentral::disconnected, this, [this](const QString &message) {
        transportEnded(Link::Bluetooth, message);
    });
#elif defined(Q_OS_LINUX)
    m_pipe.setOutput([this](const QByteArray &chunk) { return m_bluetooth.send(chunk); },
                     [this] { return m_bluetooth.maximumChunkSize(); });
    connect(&m_bluetooth, &BlePeripheral::frameReceived, &m_pipe, &BlePipe::receive);
    connect(&m_bluetooth, &BlePeripheral::peerChanged, this, [this](bool connected) {
        if (m_link != Link::Bluetooth && m_link != Link::Waiting && m_rendezvous != Rendezvous::Wifi)
            return;
        if (connected)
            transportReady(Link::Bluetooth);
        else
            transportEnded(Link::Bluetooth, tr("Il Mac si è disconnesso dal Paper Pro."));
    });
    connect(&m_bluetooth, &BlePeripheral::changed, this, [this] {
        if (m_link == Link::Waiting) {
            m_bluetoothStatus = m_bluetooth.status();
            if (m_bluetoothCandidate && !m_bluetooth.running() && !m_bluetooth.busy())
                transportEnded(Link::Bluetooth, m_bluetooth.status());
            else
                emit changed();
            return;
        }
        if (m_link != Link::Bluetooth) {
            m_bluetoothStatus = m_bluetooth.status();
            emit changed();
            return;
        }
        if (m_channel.established() || activatingBluetooth())
            return;
        if (m_bluetooth.running() || m_bluetooth.busy()) {
            report(m_bluetooth.status());
        } else if (m_tryBluetoothActivation && m_bluetooth.activationNeeded()) {
            m_tryBluetoothActivation = false;
            activateBluetooth();
        } else {
            linkEnded(m_bluetooth.status());
        }
    });
#endif
}

WirelessSync::~WirelessSync()
{
    m_locked = true;
    if (activatingBluetooth()) {
        m_activateBluetooth.kill();
        m_activateBluetooth.waitForFinished(1000);
    }
    stop();
}

bool WirelessSync::available() const
{
#ifdef Q_OS_MACOS
    return !m_tablet;
#elif defined(Q_OS_LINUX)
    return m_tablet;
#else
    return false;
#endif
}

QString WirelessSync::code() const
{
    if (!m_tablet || m_pendingSecret.isEmpty())
        return {};
    const auto hex = m_pendingSecret.toHex().toUpper();
    return QString::fromLatin1(hex.first(8) + '-' + hex.sliced(8, 8) + '-' + hex.sliced(16, 8) + '-' + hex.last(8));
}

QString WirelessSync::address() const
{
    return m_link == Link::Wifi || m_link == Link::Waiting ? m_wifi.address() : QString{};
}

QString WirelessSync::bluetoothStatus() const
{
    if (!m_bluetoothStatus.isEmpty() || !available())
        return m_bluetoothStatus;
    return m_tablet ? QString{} : tr("Collega Quaderno al Paper Pro per sincronizzare i quaderni.");
}

void WirelessSync::refreshBluetooth()
{
#ifdef Q_OS_LINUX
    if (!m_locked && available() && m_link != Link::Bluetooth && !activatingBluetooth())
        m_bluetooth.refresh();
#endif
}

void WirelessSync::preparePairing()
{
    if (!m_tablet || !available() || m_locked || paired() || active())
        return;
    QByteArray secret(secretBytes, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(secret.data()), int(secret.size())) != 1) {
        report(tr("Impossibile generare un codice di abbinamento."));
        return;
    }
    m_pendingSecret = secret;
    report(tr("Inserisci questo codice nell’app Quaderno sul Mac."));
}

bool WirelessSync::prepareConnection(Link link, const QString &code, bool automatic)
{
    if (!available() || active() || m_locked || activatingBluetooth())
        return false;
    if (automatic && !canReconnectAutomatically())
        return false;
    if (!paired()) {
        if (!m_tablet)
            m_pendingSecret = parseCode(code);
        if (m_pendingSecret.isEmpty()) {
            report(m_tablet ? tr("Genera un codice di abbinamento sul Paper Pro.")
                            : tr("Inserisci le 32 cifre esadecimali mostrate sul Paper Pro."));
            return false;
        }
    }
    stopReconnectTimers();
    m_paused = false;
    m_automaticConnection = automatic;
    if (!automatic) {
        m_reconnectFailures = 0;
        m_directWifiNext = false;
    }
    m_link = link;
    updateAutomaticTransferGate();
    emit linkChanged();
    emit reconnectionChanged();
    emit changed();
    return true;
}

void WirelessSync::startBluetooth(const QString &code)
{
    connectBluetooth(code, true);
}

void WirelessSync::connectBluetooth(const QString &code, bool allowActivation, bool automatic)
{
    if (m_tablet && m_pendingSecret.isEmpty())
        preparePairing();
    if (!prepareConnection(Link::Bluetooth, code, automatic))
        return;
    m_tryBluetoothActivation = allowActivation && m_canActivateBluetooth;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    m_bluetooth.start();
#endif
}

void WirelessSync::startWifi(const QString &address, const QString &code)
{
    connectWifi(address, code, false);
}

void WirelessSync::connectWifi(const QString &address, const QString &code, bool automatic)
{
    if (!m_tablet && !validWifiAddress(address)) {
        report(tr("Inserisci l’indirizzo IPv4 locale del Paper Pro, ad esempio 192.168.1.20. L’indirizzo USB non è utilizzabile qui."));
        return;
    }
    if (!prepareConnection(Link::Wifi, code, automatic))
        return;
    if (m_tablet)
        m_wifi.listen();
    else
        m_wifi.connectTo(address.trimmed());
}

void WirelessSync::startAutomaticListeners()
{
    if (!m_tablet || !prepareConnection(Link::Waiting, {}, true)) return;
    retryMissingListeners();
}

void WirelessSync::retryMissingListeners()
{
    if (m_link != Link::Waiting || !m_automaticConnection || m_locked || m_paused
        || !m_automaticAllowed || !m_transfer.autoSync() || m_automaticTransport != QStringLiteral("auto"))
        return;
    // Advertise only through a radio that is already enabled. The automatic
    // path must never execute the Paper Pro firmware activation helper.
    const bool startBluetooth = !m_bluetoothCandidate;
    m_bluetoothCandidate = true;
    if (m_automaticWifiAllowed && !m_wifiCandidate) {
        m_wifiCandidate = true;
        m_wifi.listen();
    }
#if defined(Q_OS_LINUX)
    if (startBluetooth && m_link == Link::Waiting)
        m_bluetooth.start();
#else
    Q_UNUSED(startBluetooth)
    m_bluetoothCandidate = false;
#endif
    if (m_link == Link::Waiting) {
        report(m_wifiCandidate && m_bluetoothCandidate
            ? tr("In attesa del Mac via Wi-Fi o Bluetooth…")
            : m_wifiCandidate ? tr("In attesa del Mac via Wi-Fi; Bluetooth non disponibile.")
            : m_bluetoothCandidate ? tr("In attesa del Mac via Bluetooth; Wi-Fi non disponibile.")
            : tr("Reti non disponibili. Quaderno riproverà il collegamento automaticamente."));
        scheduleListenerRetry();
    }
}

void WirelessSync::scheduleListenerRetry()
{
    if (m_link != Link::Waiting || !m_automaticConnection || m_locked || m_paused
        || !m_automaticAllowed || !m_transfer.autoSync()) return;
    if (m_bluetoothCandidate && (!m_automaticWifiAllowed || m_wifiCandidate)) {
        m_listenerRetry.stop();
        m_reconnectDelayMs = 0;
        emit reconnectionChanged();
        return;
    }
    if (!m_listenerRetry.isActive()) {
        m_reconnectFailures = qMin(m_reconnectFailures + 1, 6);
        m_reconnectDelayMs = reconnectDelayMs(true, m_reconnectFailures);
        m_listenerRetry.start(m_reconnectDelayMs);
        emit reconnectionChanged();
    }
}

void WirelessSync::transportReady(Link link)
{
    if (link == Link::Wifi && m_rendezvous == Rendezvous::Connecting) {
        // Bind the second TLS connection to this authenticated Bluetooth
        // exchange. A direct Wi-Fi sync or a stale offer cannot claim it.
        m_handoffWifi.start(m_tablet, derive(m_secret, QByteArrayLiteral("Wi-Fi handoff/")
            + m_rendezvousSession.toLatin1()), wifiLimit);
        return;
    }
    if (m_link == Link::Waiting) {
        if ((link == Link::Wifi && (!m_wifiCandidate || !m_automaticWifiAllowed))
            || (link == Link::Bluetooth && !m_bluetoothCandidate)) return;
        // Only one transport owns TLS. A second callback cannot replace a
        // handshake or interrupt a transfer already selected by the first.
        m_link = link;
        m_wifiCandidate = false;
        m_bluetoothCandidate = false;
        stopReconnectTimers();
        if (link == Link::Bluetooth)
            m_wifi.stop();
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
        else
            m_bluetooth.stop();
#endif
        updateAutomaticTransferGate();
        emit linkChanged();
    }
    if (m_link == link && !m_channel.established()) startChannel();
}

void WirelessSync::transportEnded(Link link, const QString &message)
{
    if (link == Link::Wifi && (m_rendezvous == Rendezvous::Connecting || m_rendezvous == Rendezvous::Wifi)) {
        fallbackRendezvous(true);
        return;
    }
    if (link == Link::Bluetooth && m_rendezvous == Rendezvous::Wifi) {
        m_channelLink = Link::None;
        m_channel.stop();
        m_pipe.reset();
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
        m_bluetooth.stop();
#endif
        return;
    }
    if (m_link == Link::Waiting) {
        if (link == Link::Wifi) m_wifiCandidate = false;
        if (link == Link::Bluetooth) {
            m_bluetoothCandidate = false;
            m_bluetoothStatus = message;
        }
        scheduleListenerRetry();
        report(m_wifiCandidate ? tr("In attesa del Mac via Wi-Fi; Bluetooth non disponibile.")
            : m_bluetoothCandidate ? tr("In attesa del Mac via Bluetooth; Wi-Fi non disponibile.")
            : tr("Reti non disponibili. Quaderno riproverà il collegamento automaticamente."));
        return;
    }
    if (m_link == link) linkEnded(message);
}

void WirelessSync::activateBluetooth()
{
#ifdef Q_OS_LINUX
    m_activationDeadline.start(75000);
    report(tr("Attivazione del Bluetooth sul Paper Pro…"));
    startPaperProBluetooth(m_activateBluetooth);
#endif
}

void WirelessSync::startChannel()
{
    if (!active() || m_link == Link::Waiting || m_channelLink != Link::None)
        return;
    // Record ownership once per TLS stream. Selecting Wi-Fi for file data
    // does not move the primary Bluetooth stream or change its record order.
    m_channelLink = m_link;
    const auto &secret = paired() ? m_secret : m_pendingSecret;
    const auto limit = m_link == Link::Bluetooth ? bluetoothLimit : wifiLimit;
    m_pipe.reset();
    m_transfer.setPayloadLimit(limit);
    report(tr("Autenticazione del collegamento…"));
    m_channel.start(m_tablet, derive(secret, QByteArrayLiteral("TLS PSK")), limit);
}

void WirelessSync::acceptPeer()
{
    if (!paired()) {
        if (!storePairing(m_pendingSecret)) {
            linkEnded(tr("Impossibile salvare l’abbinamento su questo dispositivo."));
            return;
        }
        m_secret = m_pendingSecret;
        m_pendingSecret.clear();
        updatePeer();
        emit pairedChanged();
    }
    m_preferredLink = m_link;
    m_reconnectFailures = 0;
    m_directWifiNext = false;
    stopReconnectTimers();
    if (m_link == Link::Wifi && !m_tablet)
        m_preferredAddress = m_wifi.address();
    saveConnection();
    updateAutomaticTransferGate();
    if (m_link == Link::Bluetooth)
        beginRendezvous();
    m_transfer.setConnected(true);
    report(m_link == Link::Bluetooth ? tr("Collegamento Bluetooth protetto attivo.")
                                     : tr("Collegamento Wi-Fi locale protetto attivo."));
}

bool WirelessSync::canRendezvous() const
{
    return m_automaticConnection && m_automaticAllowed
        && m_automaticTransport == QStringLiteral("auto") && m_transfer.autoSync() && !m_locked && !m_paused;
}

bool WirelessSync::canOfferWifi() const
{
    return canRendezvous() && m_automaticWifiAllowed;
}

void WirelessSync::beginRendezvous()
{
    if (!canRendezvous() || !m_transfer.setTransportSelectionPaused(true))
        return;
    m_rendezvous = Rendezvous::Discovering;
    m_rendezvousDeadline.start(rendezvousDiscoveryMs);
    if (!m_tablet) {
        m_rendezvousSession = SyncRendezvous::newSessionId();
        sendRendezvous(SyncRendezvous::Kind::Hello);
    }
}

void WirelessSync::sendRendezvous(SyncRendezvous::Kind kind, const QString &address)
{
    using Kind = SyncRendezvous::Kind;
    if (kind == Kind::Hello || kind == Kind::Work) {
        m_localWork = m_transfer.hasSyncWork() || m_transfer.transportSelectionBusy();
        m_localWifi = canOfferWifi();
    }
    auto &channel = kind == Kind::Ready || kind == Kind::Commit ? m_handoffWifi : m_channel;
    channel.sendMessage(SyncRendezvous::encode(
        {kind, m_rendezvousSession, m_localWork, m_localWifi, address}), {});
}

void WirelessSync::updateRendezvous()
{
    using Kind = SyncRendezvous::Kind;
    if (m_rendezvous == Rendezvous::Bluetooth) {
        const bool work = m_transfer.hasSyncWork() || m_transfer.transportSelectionBusy();
        if (m_localWork != work || m_localWifi != canOfferWifi())
            sendRendezvous(Kind::Work);
        // Let the entire current batch settle before another Wi-Fi attempt.
        if (!m_tablet && !m_localWork && !m_peerWork && m_localWifi && m_peerWifi)
            beginRendezvous();
        return;
    }
    if (m_rendezvous != Rendezvous::Idle)
        return;
    if (!canOfferWifi() || !m_peerWifi || m_transfer.transportSelectionBusy()) {
        fallbackRendezvous(true);
        return;
    }
    if (m_localWork != m_transfer.hasSyncWork())
        sendRendezvous(Kind::Work);
    if (m_rendezvous != Rendezvous::Idle || !m_tablet || (!m_localWork && !m_peerWork))
        return;
    m_rendezvous = Rendezvous::Connecting;
    m_rendezvousDeadline.start(rendezvousConnectionMs);
    report(tr("Cerco un collegamento Wi-Fi per sincronizzare…"));
    m_wifi.listen();
    if (m_rendezvous == Rendezvous::Connecting)
        sendRendezvous(Kind::Offer, m_wifi.address());
}

void WirelessSync::receivePrimaryMessage(const QVariantMap &metadata, const QByteArray &payload)
{
    using Kind = SyncRendezvous::Kind;
    if (!SyncRendezvous::isControlMessage(metadata)) {
        if (m_rendezvous == Rendezvous::Wifi)
            return;
        const auto type = metadata.value(QStringLiteral("type")).toString();
        // Legacy peers can start a transfer while the capability probe is
        // pending. Finish on Bluetooth before delivering the first payload.
        if (m_rendezvous != Rendezvous::Inactive && m_rendezvous != Rendezvous::Bluetooth
            && type != QStringLiteral("syncHello")
            && type != QStringLiteral("syncHelloReply"))
            fallbackRendezvous(true);
        m_transfer.receiveMessage(metadata, payload);
        return;
    }
    if (m_rendezvous == Rendezvous::Inactive)
        return;
    const auto message = SyncRendezvous::decode(metadata, payload);
    if (!message)
        return;
    const bool discovering = m_rendezvous == Rendezvous::Discovering
        && (m_tablet || m_rendezvousSession == message->session);
    const bool rearming = m_rendezvous == Rendezvous::Bluetooth && m_tablet
        && message->session != m_rendezvousSession;
    if (message->kind == Kind::Hello && (discovering || rearming)) {
        if (rearming && (!canOfferWifi() || !m_transfer.setTransportSelectionPaused(true))) {
            m_channel.sendMessage(SyncRendezvous::encode({Kind::Abort, message->session, false, false, {}}), {});
            m_rendezvous = Rendezvous::Inactive;
            return;
        }
        m_rendezvousSession = message->session;
        m_peerWork = message->work;
        m_peerWifi = message->wifi;
        m_rendezvous = Rendezvous::Idle;
        m_rendezvousDeadline.stop();
        if (m_tablet)
            sendRendezvous(Kind::Hello);
        updateRendezvous();
    } else if (message->session == m_rendezvousSession) {
        if (message->kind == Kind::Abort && m_rendezvous != Rendezvous::Bluetooth) {
            fallbackRendezvous(false);
        } else if (message->kind == Kind::Work
                   && (m_rendezvous == Rendezvous::Idle || m_rendezvous == Rendezvous::Bluetooth)) {
            m_peerWork = message->work;
            m_peerWifi = message->wifi;
            updateRendezvous();
        } else if (message->kind == Kind::Offer && !m_tablet && m_rendezvous == Rendezvous::Idle) {
            if (!canOfferWifi() || !m_peerWifi || (!m_transfer.hasSyncWork() && !m_peerWork)
                || m_transfer.transportSelectionBusy()) {
                fallbackRendezvous(true);
                return;
            }
            m_rendezvous = Rendezvous::Connecting;
            m_rendezvousDeadline.start(rendezvousConnectionMs);
            report(tr("Cerco un collegamento Wi-Fi per sincronizzare…"));
            if (!m_wifi.connectToHandoff(message->address))
                fallbackRendezvous(true);
        }
    }
}

void WirelessSync::receiveHandoffMessage(const QVariantMap &metadata, const QByteArray &payload)
{
    using Kind = SyncRendezvous::Kind;
    if (m_rendezvous == Rendezvous::Wifi) {
        if (!SyncRendezvous::isControlMessage(metadata))
            m_transfer.receiveMessage(metadata, payload);
        return;
    }
    const auto message = SyncRendezvous::decode(metadata, payload);
    if (m_rendezvous != Rendezvous::Connecting || !message || message->session != m_rendezvousSession
        || message->kind != (m_tablet ? Kind::Ready : Kind::Commit)
        || !canOfferWifi() || m_transfer.transportSelectionBusy()) {
        fallbackRendezvous(true);
        return;
    }
    if (m_tablet)
        sendRendezvous(Kind::Commit);
    if (m_rendezvous == Rendezvous::Connecting)
        useHandoffWifi();
}

void WirelessSync::useHandoffWifi()
{
    m_rendezvousDeadline.stop();
    m_rendezvous = Rendezvous::Wifi;
    m_transfer.setConnected(false);
    m_link = Link::Wifi;
    m_transfer.setPayloadLimit(wifiLimit);
    m_preferredLink = Link::Wifi;
    if (!m_tablet)
        m_preferredAddress = m_wifi.address();
    saveConnection();
    updateAutomaticTransferGate();
    emit linkChanged();
    m_transfer.setConnected(true);
    m_transfer.setTransportSelectionPaused(false);
    report(tr("Collegamento Wi-Fi locale protetto attivo."));
}

void WirelessSync::fallbackRendezvous(bool notifyPeer)
{
    if (m_rendezvous == Rendezvous::Inactive || m_rendezvous == Rendezvous::Bluetooth)
        return;
    const bool wasWifi = m_rendezvous == Rendezvous::Wifi;
    const bool wifiAttempted = wasWifi || m_rendezvous == Rendezvous::Connecting;
    const bool peerSupportsHandoff = m_rendezvous != Rendezvous::Discovering;
    m_rendezvousDeadline.stop();
    if (wasWifi && !m_channel.established()) {
        linkEnded(tr("Collegamento Wi-Fi interrotto. Riconnessione in corso…"));
        return;
    }
    m_rendezvous = peerSupportsHandoff ? Rendezvous::Bluetooth : Rendezvous::Inactive;
    // Route the abort over the original Bluetooth TLS stream, before opening
    // it to fresh transfer messages.
    if (notifyPeer && !m_rendezvousSession.isEmpty())
        sendRendezvous(SyncRendezvous::Kind::Abort);
    if (!peerSupportsHandoff)
        m_rendezvousSession.clear();
    m_handoffWifi.stop();
    m_wifi.stop();
    if (wasWifi) {
        // A fresh preview can resume on Bluetooth. A lost apply confirmation
        // is still handled by the engine as an uncertain result.
        m_transfer.setConnected(false);
        m_link = Link::Bluetooth;
        m_transfer.setPayloadLimit(bluetoothLimit);
        updateAutomaticTransferGate();
        emit linkChanged();
        m_transfer.setConnected(true);
    }
    m_transfer.setTransportSelectionPaused(false);
    if (peerSupportsHandoff)
        sendRendezvous(SyncRendezvous::Kind::Work);
    if (wifiAttempted)
        report(tr("Wi-Fi non disponibile. La sincronizzazione continua via Bluetooth."));
}

void WirelessSync::stop()
{
    m_paused = true;
    stopReconnectTimers();
    const bool bluetooth = m_link == Link::Bluetooth;
    resetLink();
    m_transfer.cancelDeferredTransfer();
    if (bluetooth) {
        m_bluetoothStatus.clear();
        refreshBluetooth();
    }
    report({});
    emit reconnectionChanged();
}

void WirelessSync::resetLink()
{
    m_resettingLink = true;
    // Let the engine classify the interrupted preview/apply while its link
    // policy is still intact. Closing that gate first would look like a user
    // pause and erase the retry checkpoint for the disconnected transfer.
    m_transfer.setConnected(false);
    m_rendezvousDeadline.stop();
    m_rendezvous = Rendezvous::Inactive;
    m_rendezvousSession.clear();
    m_localWork = false;
    m_localWifi = false;
    m_peerWork = false;
    m_peerWifi = false;
    m_transfer.setTransportSelectionPaused(false);
    m_link = Link::None;
    m_automaticConnection = false;
    m_channelLink = Link::None;
    m_wifiCandidate = false;
    m_bluetoothCandidate = false;
    m_listenerRetry.stop();
    updateAutomaticTransferGate();
    m_tryBluetoothActivation = false;
    if (activatingBluetooth())
        m_activateBluetooth.kill();
    m_channel.stop();
    m_handoffWifi.stop();
    m_pipe.reset();
    m_wifi.stop();
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    m_bluetooth.stop();
#endif
    m_pendingSecret.clear();
    m_resettingLink = false;
    emit linkChanged();
}

void WirelessSync::linkEnded(const QString &message)
{
    if (!active())
        return;
    // A failed attempt does not discard the unpaired tablet's displayed code.
    const QByteArray pending = m_tablet ? m_pendingSecret : QByteArray{};
    const bool bluetooth = m_link == Link::Bluetooth;
    const Link failedLink = m_link;
    m_reconnectFailures = qMin(m_reconnectFailures + 1, 6);
    if (m_automaticTransport == QStringLiteral("auto"))
        m_directWifiNext = failedLink == Link::Bluetooth;
    resetLink();
    m_pendingSecret = pending;
    if (bluetooth)
        m_bluetoothStatus = message;
    report(message);
    scheduleReconnect();
}

void WirelessSync::forget()
{
    stop();
    if (QFile::exists(m_pairingPath) && !QFile::remove(m_pairingPath)) {
        report(tr("Impossibile rimuovere l’abbinamento salvato."));
        return;
    }
    const bool wasPaired = paired();
    m_secret.clear();
    m_transfer.forgetPeer();
    if (wasPaired) emit pairedChanged();
    m_preferredLink = Link::None;
    m_preferredAddress.clear();
    m_reconnectFailures = 0;
    m_directWifiNext = false;
    wirelessSettings().remove(QStringLiteral("wirelessTransport"));
    report(tr("Abbinamento rimosso."));
}

void WirelessSync::report(const QString &message)
{
    m_status = message;
    if (m_link == Link::Bluetooth)
        m_bluetoothStatus = message;
    emit changed();
}

void WirelessSync::loadPairing()
{
    const bool wasPaired = paired();
    QFile file(m_pairingPath);
    const QFileInfo info(file);
    if (info.isSymLink() || !info.isFile() || !file.open(QIODevice::ReadOnly)
        || (file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
                                | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther)))
        return;
    const QByteArray stored = file.read(secretBytes + 1);
    if (stored.size() == secretBytes) {
        m_secret = stored;
        updatePeer();
        auto settings = wirelessSettings();
        settings.beginGroup(QStringLiteral("wirelessTransport"));
        const QString transport = settings.value(QStringLiteral("type")).toString();
        m_preferredAddress = settings.value(QStringLiteral("address")).toString();
        if (!validWifiAddress(m_preferredAddress))
            m_preferredAddress.clear();
        if (transport == QStringLiteral("bluetooth"))
            m_preferredLink = Link::Bluetooth;
        else if (transport == QStringLiteral("wifi") && (m_tablet || !m_preferredAddress.isEmpty()))
            m_preferredLink = Link::Wifi;
        if (wasPaired != paired()) emit pairedChanged();
    }
}

bool WirelessSync::storePairing(const QByteArray &secret)
{
    if (secret.size() != secretBytes || !QDir().mkpath(QFileInfo(m_pairingPath).absolutePath()))
        return false;
    QSaveFile file(m_pairingPath);
    return file.open(QIODevice::WriteOnly) && file.setPermissions(privatePermissions)
        && file.write(secret) == secret.size() && file.commit();
}

void WirelessSync::updatePeer()
{
    m_transfer.setPeerIdentity(paired() ? QString::fromLatin1(derive(m_secret, QByteArrayLiteral("Peer")).first(16).toHex())
                                        : QString{});
}

void WirelessSync::setLocked(bool locked)
{
    if (locked == m_locked)
        return;
    m_locked = locked;
    if (locked) {
        stopReconnectTimers();
        resetLink();
        m_transfer.cancelDeferredTransfer();
        m_bluetoothStatus.clear();
        report({});
    } else {
        scheduleReconnect();
    }
}

void WirelessSync::setAutomaticAllowed(bool allowed)
{
    if (m_automaticAllowed == allowed) return;
    m_automaticAllowed = allowed;
    updateAutomaticTransferGate();
    enforceAutomaticPolicy();
    scheduleReconnect();
    emit changed();
}

void WirelessSync::setAutomaticWifiOnly(bool wifiOnly)
{
    setAutomaticTransport(wifiOnly ? QStringLiteral("wifi") : QStringLiteral("auto"));
}

void WirelessSync::setAutomaticTransport(const QString &transport)
{
    if ((transport != QStringLiteral("auto") && transport != QStringLiteral("wifi")
         && transport != QStringLiteral("bluetooth")) || m_automaticTransport == transport) return;
    m_automaticTransport = transport;
    m_directWifiNext = false;
    m_reconnectFailures = 0;
    stopReconnectTimers();
    updateAutomaticTransferGate();
    enforceAutomaticPolicy();
    scheduleReconnect();
    emit automaticPolicyChanged();
    emit changed();
}

void WirelessSync::setAutomaticWifiAllowed(bool allowed)
{
    if (m_automaticWifiAllowed == allowed) return;
    m_automaticWifiAllowed = allowed;
    updateAutomaticTransferGate();
    enforceAutomaticPolicy();
    scheduleReconnect();
    emit automaticPolicyChanged();
    emit changed();
}

void WirelessSync::updateAutomaticTransferGate()
{
    const bool transportAllowed = (m_link == Link::Wifi && m_automaticWifiAllowed
        && m_automaticTransport != QStringLiteral("bluetooth"))
        || (m_link == Link::Bluetooth && m_automaticTransport != QStringLiteral("wifi"));
    m_transfer.setAutomaticAllowed(m_automaticAllowed && !m_locked && transportAllowed);
}

void WirelessSync::enforceAutomaticPolicy()
{
    if (m_resettingLink || !m_automaticConnection) return;
    if (m_rendezvous == Rendezvous::Bluetooth)
        QMetaObject::invokeMethod(this, &WirelessSync::updateRendezvous, Qt::QueuedConnection);
    else if ((m_rendezvous == Rendezvous::Discovering && !canRendezvous())
             || ((m_rendezvous == Rendezvous::Idle || m_rendezvous == Rendezvous::Connecting) && !canOfferWifi()))
        fallbackRendezvous(true);
    if (m_channel.established() || m_rendezvous == Rendezvous::Wifi) return;
    const bool allowed = m_automaticAllowed && m_transfer.autoSync() && !m_locked && !m_paused;
    if (!allowed || (m_link == Link::Wifi && (!m_automaticWifiAllowed
            || m_automaticTransport == QStringLiteral("bluetooth")))
        || (m_link == Link::Bluetooth && automaticWifiOnly())
        || (m_link == Link::Waiting && m_automaticTransport != QStringLiteral("auto"))) {
        resetLink();
        return;
    }
    if (m_link == Link::Waiting) {
        if (!m_automaticWifiAllowed && m_wifiCandidate) {
            m_wifiCandidate = false;
            m_wifi.stop();
        }
        // Starting candidates is deferred: a QML policy binding never starts a
        // radio or server while its remaining security gates are reevaluating.
        if (m_automaticWifiAllowed && !m_wifiCandidate && !m_listenerRetry.isActive()) {
            m_listenerRetry.start(0);
            emit reconnectionChanged();
        } else {
            scheduleListenerRetry();
        }
    }
}

WirelessSync::Link WirelessSync::automaticLink() const
{
    const bool wifi = m_automaticWifiAllowed && (m_tablet || validWifiAddress(m_preferredAddress));
    if (m_automaticTransport == QStringLiteral("bluetooth")) return Link::Bluetooth;
    if (m_automaticTransport == QStringLiteral("wifi")) return wifi ? Link::Wifi : Link::None;
    if (m_tablet) return Link::Waiting;
    return wifi && m_directWifiNext ? Link::Wifi : Link::Bluetooth;
}

bool WirelessSync::canReconnectAutomatically() const
{
    return available() && !m_resettingLink && !active() && !activatingBluetooth() && paired() && m_transfer.autoSync()
        && m_automaticAllowed && !m_locked && !m_paused && automaticLink() != Link::None;
}

void WirelessSync::scheduleReconnect()
{
    if (m_resettingLink) return;
    enforceAutomaticPolicy();
    if (m_link == Link::Waiting) return;
    if (!canReconnectAutomatically()) {
        if (m_reconnect.isActive()) {
            m_reconnect.stop();
            m_reconnectDelayMs = 0;
            emit reconnectionChanged();
        }
    } else if (!m_reconnect.isActive()) {
        m_reconnectDelayMs = reconnectDelayMs(m_tablet, m_reconnectFailures);
        m_reconnect.start(m_reconnectDelayMs);
        emit reconnectionChanged();
    }
}

int WirelessSync::reconnectDelayMs(bool tablet, int failures)
{
    const int shift = qBound(0, failures - 1, 6);
    return qMin(120000, (tablet ? 3000 : 8000) * (1 << shift));
}

void WirelessSync::stopReconnectTimers()
{
    m_reconnect.stop();
    m_listenerRetry.stop();
    m_reconnectDelayMs = 0;
    emit reconnectionChanged();
}

bool WirelessSync::resumeAutomaticConnection()
{
    if (!available() || active() || activatingBluetooth() || !paired() || !m_transfer.autoSync()
        || !m_automaticAllowed || m_locked || automaticLink() == Link::None) return false;
    m_paused = false;
    m_reconnectFailures = 0;
    m_directWifiNext = false;
    m_reconnectDelayMs = 0;
    m_reconnect.start(0);
    emit reconnectionChanged();
    return true;
}

void WirelessSync::saveConnection()
{
    auto settings = wirelessSettings();
    settings.beginGroup(QStringLiteral("wirelessTransport"));
    settings.setValue(QStringLiteral("type"), m_preferredLink == Link::Bluetooth ? QStringLiteral("bluetooth")
                                                                                 : QStringLiteral("wifi"));
    settings.setValue(QStringLiteral("address"), m_preferredAddress);
}
