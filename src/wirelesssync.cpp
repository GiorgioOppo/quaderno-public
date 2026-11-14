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
        if (m_preferredLink == Link::Bluetooth)
            connectBluetooth({}, false);
        else if (m_preferredLink == Link::Wifi)
            startWifi(m_preferredAddress, {});
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
    connect(&m_transfer, &BleSync::sendMessage, &m_channel, &SyncChannel::sendMessage);
    connect(&m_channel, &SyncChannel::messageReceived, &m_transfer, &BleSync::receiveMessage);
    connect(&m_channel, &SyncChannel::establishedChanged, this, [this] {
        if (m_channel.established())
            acceptPeer();
        else
            m_transfer.setConnected(false);
    });
    connect(&m_channel, &SyncChannel::encryptedReady, this, [this](const QByteArray &bytes) {
        if (m_link == Link::Bluetooth)
            m_pipe.enqueue(bytes);
        else if (m_link == Link::Wifi)
            m_wifi.send(bytes);
    });
    connect(&m_channel, &SyncChannel::failed, this, &WirelessSync::linkEnded);
    connect(&m_pipe, &BlePipe::received, &m_channel, &SyncChannel::receive);
    connect(&m_pipe, &BlePipe::failed, this, [this] {
        linkEnded(tr("Lo scambio Bluetooth si è interrotto. Ricollega il dispositivo."));
    }, Qt::QueuedConnection);
    connect(&m_wifi, &WifiSyncLink::ready, this, &WirelessSync::startChannel);
    connect(&m_wifi, &WifiSyncLink::received, &m_channel, &SyncChannel::receive);
    connect(&m_wifi, &WifiSyncLink::disconnected, this, &WirelessSync::linkEnded);
    connect(&m_wifi, &WifiSyncLink::changed, this, [this] {
        if (m_link == Link::Wifi && !m_channel.established())
            report(m_wifi.status());
    });
#ifdef Q_OS_MACOS
    m_pipe.setOutput([this](const QByteArray &chunk) { return m_bluetooth.writeChunk(chunk); },
                     [this] { return m_bluetooth.maximumChunkSize(); });
    connect(&m_bluetooth, &BleCentral::ready, this, &WirelessSync::startChannel);
    connect(&m_bluetooth, &BleCentral::received, &m_pipe, &BlePipe::receive);
    connect(&m_bluetooth, &BleCentral::writable, &m_pipe, &BlePipe::writable);
    connect(&m_bluetooth, &BleCentral::statusChanged, this, [this](const QString &status) {
        if (m_link == Link::Bluetooth)
            report(status);
    });
    connect(&m_bluetooth, &BleCentral::disconnected, this, &WirelessSync::linkEnded);
#elif defined(Q_OS_LINUX)
    m_pipe.setOutput([this](const QByteArray &chunk) { return m_bluetooth.send(chunk); },
                     [this] { return m_bluetooth.maximumChunkSize(); });
    connect(&m_bluetooth, &BlePeripheral::frameReceived, &m_pipe, &BlePipe::receive);
    connect(&m_bluetooth, &BlePeripheral::peerChanged, this, [this](bool connected) {
        if (m_link != Link::Bluetooth)
            return;
        if (connected)
            startChannel();
        else
            linkEnded(tr("Il Mac si è disconnesso dal Paper Pro."));
    });
    connect(&m_bluetooth, &BlePeripheral::changed, this, [this] {
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
    return m_link == Link::Wifi ? m_wifi.address() : QString{};
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

bool WirelessSync::prepareConnection(Link link, const QString &code)
{
    if (!available() || active() || m_locked || activatingBluetooth())
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
    m_reconnect.stop();
    m_paused = false;
    m_link = link;
    emit changed();
    return true;
}

void WirelessSync::startBluetooth(const QString &code)
{
    connectBluetooth(code, true);
}

void WirelessSync::connectBluetooth(const QString &code, bool allowActivation)
{
    if (m_tablet && m_pendingSecret.isEmpty())
        preparePairing();
    if (!prepareConnection(Link::Bluetooth, code))
        return;
    m_tryBluetoothActivation = allowActivation && m_canActivateBluetooth;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    m_bluetooth.start();
#endif
}

void WirelessSync::startWifi(const QString &address, const QString &code)
{
    if (!prepareConnection(Link::Wifi, code))
        return;
    if (m_tablet)
        m_wifi.listen();
    else
        m_wifi.connectTo(address.trimmed());
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
    if (!active())
        return;
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
    }
    m_preferredLink = m_link;
    m_preferredAddress = m_link == Link::Wifi && !m_tablet ? m_wifi.address() : QString{};
    saveConnection();
    m_transfer.setConnected(true);
    report(m_link == Link::Bluetooth ? tr("Collegamento Bluetooth protetto attivo.")
                                     : tr("Collegamento Wi-Fi locale protetto attivo."));
}

void WirelessSync::stop()
{
    m_paused = true;
    m_reconnect.stop();
    const bool bluetooth = m_link == Link::Bluetooth;
    resetLink();
    if (bluetooth) {
        m_bluetoothStatus.clear();
        refreshBluetooth();
    }
    report({});
}

void WirelessSync::resetLink()
{
    m_link = Link::None;
    m_tryBluetoothActivation = false;
    if (activatingBluetooth())
        m_activateBluetooth.kill();
    m_channel.stop();
    m_transfer.setConnected(false);
    m_pipe.reset();
    m_wifi.stop();
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    m_bluetooth.stop();
#endif
    m_pendingSecret.clear();
}

void WirelessSync::linkEnded(const QString &message)
{
    if (!active())
        return;
    // A failed attempt does not discard the unpaired tablet's displayed code.
    const QByteArray pending = m_tablet ? m_pendingSecret : QByteArray{};
    const bool bluetooth = m_link == Link::Bluetooth;
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
    m_secret.clear();
    m_transfer.forgetPeer();
    m_preferredLink = Link::None;
    m_preferredAddress.clear();
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
        if (transport == QStringLiteral("bluetooth"))
            m_preferredLink = Link::Bluetooth;
        else if (transport == QStringLiteral("wifi") && (m_tablet || !m_preferredAddress.isEmpty()))
            m_preferredLink = Link::Wifi;
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
        m_reconnect.stop();
        resetLink();
        m_bluetoothStatus.clear();
        report({});
    } else {
        scheduleReconnect();
    }
}

void WirelessSync::scheduleReconnect()
{
    if (!available() || active() || activatingBluetooth() || !paired() || !m_transfer.autoSync() || m_locked || m_paused
        || m_preferredLink == Link::None) {
        m_reconnect.stop();
    } else if (!m_reconnect.isActive()) {
        m_reconnect.start(m_tablet ? 3000 : 8000);
    }
}

void WirelessSync::saveConnection()
{
    auto settings = wirelessSettings();
    settings.beginGroup(QStringLiteral("wirelessTransport"));
    settings.setValue(QStringLiteral("type"), m_preferredLink == Link::Bluetooth ? QStringLiteral("bluetooth")
                                                                                 : QStringLiteral("wifi"));
    settings.setValue(QStringLiteral("address"), m_preferredAddress);
}
