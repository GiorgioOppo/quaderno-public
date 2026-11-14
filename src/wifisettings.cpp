#include "wifisettings.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QPasswordDigestor>

#include <algorithm>
#include <utility>

namespace {
const QString service = QStringLiteral("fi.w1.wpa_supplicant1");
const QString servicePath = QStringLiteral("/fi/w1/wpa_supplicant1");
const QString interfaceApi = QStringLiteral("fi.w1.wpa_supplicant1.Interface");
const QString networkApi = QStringLiteral("fi.w1.wpa_supplicant1.Network");
const QString accessPointApi = QStringLiteral("fi.w1.wpa_supplicant1.BSS");
const QString propertiesApi = QStringLiteral("org.freedesktop.DBus.Properties");
QString operationFailed()
{
    return QCoreApplication::translate("WifiSettings", "Impossibile comunicare con il servizio Wi-Fi.");
}

QString displayName(const QByteArray &ssid)
{
    QString name = QString::fromUtf8(ssid);
    for (QChar &character : name) {
        const auto category = character.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format
            || category == QChar::Separator_Line || category == QChar::Separator_Paragraph) {
            character = QChar::ReplacementCharacter;
        }
    }
    return name;
}

QByteArray preSharedKey(QByteArray passphrase, const QByteArray &ssid)
{
    const auto isHex = [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')
            || (character >= 'A' && character <= 'F');
    };
    QByteArray key;
    if (passphrase.size() == 64 && std::all_of(passphrase.cbegin(), passphrase.cend(), isHex)) {
        key = QByteArray::fromHex(passphrase);
    } else if (passphrase.size() >= 8 && passphrase.size() <= 63
               && std::all_of(passphrase.cbegin(), passphrase.cend(), [](unsigned char character) {
                   return character >= 32 && character <= 126;
               })) {
        key = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha1, passphrase, ssid, 4096, 32);
    }
    passphrase.fill('\0');
    return key;
}

QVariant objectPath(const QString &path)
{
    return QVariant::fromValue(QDBusObjectPath(path));
}

QString usablePath(const QDBusObjectPath &path)
{
    return path.path() == QStringLiteral("/") ? QString{} : path.path();
}
}

WifiSettings::WifiSettings(bool deviceMode, QObject *parent)
    : QObject(parent), m_deviceMode(deviceMode)
{
#ifndef Q_OS_LINUX
    m_deviceMode = false;
#endif
    if (!m_deviceMode)
        return;

    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        if (m_operation == Operation::Scan)
            finish(tr("La ricerca delle reti Wi-Fi ha impiegato troppo tempo."));
        else if (m_operation == Operation::Connect)
            abortConnection(tr("Connessione non riuscita. Controlla la password e il segnale."));
    });
    m_connectPoll.setInterval(1000);
    connect(&m_connectPoll, &QTimer::timeout, this, &WifiSettings::readStatus);

    auto *watcher = new QDBusServiceWatcher(service, QDBusConnection::systemBus(),
                                            QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
        clearInterface();
        if (!owner.isEmpty())
            refresh();
    });
    refresh();
}

WifiSettings::~WifiSettings()
{
    if (!available() || m_trialNetwork.isEmpty())
        return;
    auto message = QDBusMessage::createMethodCall(service, m_interfacePath, interfaceApi,
                                                 QStringLiteral("RemoveNetwork"));
    message.setAutoStartService(false);
    message.setArguments({objectPath(m_trialNetwork)});
    QDBusConnection::systemBus().call(message, QDBus::Block, 500);
}

QString WifiSettings::status() const
{
    if (!m_deviceMode)
        return tr("Controlli Wi-Fi disponibili sul Paper Pro");
    if (m_probing)
        return tr("Verifica del Wi-Fi…");
    if (!available())
        return tr("Servizio Wi-Fi non disponibile");
    switch (m_operation) {
    case Operation::Scan:
        return tr("Ricerca reti…");
    case Operation::Connect:
        return tr("Connessione in corso…");
    case Operation::Disconnect:
        return tr("Disconnessione in corso…");
    case Operation::Idle:
        break;
    }
    if (connected())
        return tr("Connesso");
    if (m_state == QStringLiteral("interface_disabled"))
        return tr("Wi-Fi disattivato");
    return tr("Non connesso");
}

void WifiSettings::call(const QString &path, const QString &interface, const QString &method,
                        const QVariantList &arguments, ReplyHandler handler)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setAutoStartService(false);
    message.setArguments(arguments);
    const auto pending = QDBusConnection::systemBus().asyncCall(message, 4000);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation = m_generation, handler = std::move(handler)](QDBusPendingCallWatcher *finished) {
        const QDBusMessage reply = finished->reply();
        finished->deleteLater();
        if (generation == m_generation)
            handler(reply);
    });
}

void WifiSettings::callInterface(const QString &method, const QVariantList &arguments, ReplyHandler handler)
{
    call(m_interfacePath, interfaceApi, method, arguments, std::move(handler));
}

void WifiSettings::refresh()
{
    if (!m_deviceMode || m_probing)
        return;
    if (available()) {
        readStatus();
        return;
    }

    m_probing = true;
    emit changed();
    call(servicePath, service, QStringLiteral("GetInterface"), {QStringLiteral("wlan0")},
         [this](const QDBusMessage &message) {
        m_probing = false;
        const QDBusReply<QDBusObjectPath> reply(message);
        if (reply.isValid())
            m_interfacePath = usablePath(reply.value());
        if (available()) {
            auto bus = QDBusConnection::systemBus();
            bus.connect(service, m_interfacePath, propertiesApi, QStringLiteral("PropertiesChanged"),
                        this, SLOT(interfaceChanged(QString,QVariantMap,QStringList)));
            bus.connect(service, m_interfacePath, interfaceApi, QStringLiteral("ScanDone"),
                        this, SLOT(scanDone(bool)));
            m_error.clear();
            readStatus();
        }
        emit changed();
    });
}

void WifiSettings::interfaceChanged(const QString &interface, const QVariantMap &properties,
                                    const QStringList &invalidated)
{
    if (interface != interfaceApi)
        return;
    for (const auto &property : {QStringLiteral("State"), QStringLiteral("CurrentBSS"),
                                 QStringLiteral("CurrentNetwork")}) {
        if (properties.contains(property) || invalidated.contains(property)) {
            readStatus();
            return;
        }
    }
}

void WifiSettings::readStatus()
{
    if (!available())
        return;
    if (m_readingStatus) {
        m_statusPending = true;
        return;
    }

    m_readingStatus = true;
    call(m_interfacePath, propertiesApi, QStringLiteral("GetAll"), {interfaceApi},
         [this](const QDBusMessage &message) {
        m_readingStatus = false;
        const QDBusReply<QVariantMap> reply(message);
        if (!reply.isValid()) {
            m_statusPending = false;
            // Keep the trial profile path through transient status failures so cleanup can still remove it.
            if (m_operation != Operation::Connect)
                clearInterface();
            return;
        }

        const QVariantMap properties = reply.value();
        const QString state = properties.value(QStringLiteral("State")).toString();
        const QString accessPoint = usablePath(properties.value(QStringLiteral("CurrentBSS")).value<QDBusObjectPath>());
        const bool statusChanged = state != m_state || accessPoint != m_currentBss;
        if (accessPoint != m_currentBss || state != QStringLiteral("completed"))
            m_ssid.clear();
        m_state = state;
        m_currentBss = accessPoint;
        if (statusChanged)
            emit changed();

        if (connected() && !accessPoint.isEmpty() && m_ssid.isEmpty()) {
            call(accessPoint, propertiesApi, QStringLiteral("Get"),
                 {accessPointApi, QStringLiteral("SSID")}, [this, accessPoint](const QDBusMessage &message) {
                const QDBusReply<QDBusVariant> reply(message);
                if (reply.isValid() && connected() && accessPoint == m_currentBss) {
                    const QString name = displayName(reply.value().variant().toByteArray());
                    if (name != m_ssid) {
                        m_ssid = name;
                        emit changed();
                    }
                }
            });
        }

        const QString currentNetwork = usablePath(properties.value(QStringLiteral("CurrentNetwork")).value<QDBusObjectPath>());
        if (m_connectArmed && connected() && currentNetwork == m_trialNetwork) {
            const QString previous = std::exchange(m_addedNetwork, std::exchange(m_trialNetwork, {}));
            // Keep other profiles enabled and return the new profile to normal priority after association.
            call(m_addedNetwork, propertiesApi, QStringLiteral("Set"),
                 {networkApi, QStringLiteral("Properties"), QVariant::fromValue(QDBusVariant(QVariantMap{
                     {QStringLiteral("priority"), 0},
                 }))}, [](const QDBusMessage &) {});
            removeNetwork(previous);
            finish();
        }
        if (std::exchange(m_statusPending, false))
            readStatus();
    });
}

void WifiSettings::scan()
{
    if (!available() || busy())
        return;

    m_operation = Operation::Scan;
    m_error.clear();
    m_scanReading = false;
    ++m_scanGeneration;
    emit changed();
    m_deadline.start(20000);
    const QVariantMap options{{QStringLiteral("Type"), QStringLiteral("active")},
                              {QStringLiteral("AllowRoam"), false}};
    callInterface(QStringLiteral("Scan"), {options}, [this, generation = m_scanGeneration](const QDBusMessage &message) {
        if (m_operation == Operation::Scan && m_scanGeneration == generation
            && !m_scanReading && message.type() == QDBusMessage::ErrorMessage) {
            // A scan by the stock daemon may already be running. Its completion is equally useful.
            call(m_interfacePath, propertiesApi, QStringLiteral("Get"), {interfaceApi, QStringLiteral("Scanning")},
                 [this, generation](const QDBusMessage &message) {
                const QDBusReply<QDBusVariant> reply(message);
                if (m_operation == Operation::Scan && m_scanGeneration == generation && !m_scanReading
                    && (!reply.isValid() || !reply.value().variant().toBool())) {
                    finish(tr("Impossibile avviare la ricerca delle reti Wi-Fi."));
                }
            });
        }
    });
}

void WifiSettings::scanDone(bool success)
{
    if (m_operation != Operation::Scan || m_scanReading)
        return;
    if (!success) {
        finish(tr("Ricerca delle reti Wi-Fi non riuscita."));
        return;
    }

    m_scanReading = true;
    m_scanResults.clear();
    m_scanPaths.clear();
    call(m_interfacePath, propertiesApi, QStringLiteral("Get"), {interfaceApi, QStringLiteral("BSSs")},
         [this, generation = m_scanGeneration](const QDBusMessage &message) {
        if (m_operation != Operation::Scan || m_scanGeneration != generation)
            return;
        const QDBusReply<QDBusVariant> reply(message);
        if (!reply.isValid()) {
            finish(tr("Impossibile leggere le reti Wi-Fi rilevate."));
            return;
        }
        const auto paths = qdbus_cast<QList<QDBusObjectPath>>(reply.value().variant());
        for (const auto &path : paths) {
            if (!usablePath(path).isEmpty())
                m_scanPaths.append(path.path());
        }
        readAccessPoint();
    });
}

void WifiSettings::readAccessPoint()
{
    if (m_scanPaths.isEmpty()) {
        publishScan();
        return;
    }
    const QString path = m_scanPaths.takeLast();
    call(path, propertiesApi, QStringLiteral("GetAll"), {accessPointApi},
         [this, generation = m_scanGeneration](const QDBusMessage &message) {
        if (m_operation != Operation::Scan || m_scanGeneration != generation)
            return;
        const QDBusReply<QVariantMap> reply(message);
        if (reply.isValid()) {
            const QVariantMap properties = reply.value();
            const QByteArray ssid = properties.value(QStringLiteral("SSID")).toByteArray();
            const QString mode = properties.value(QStringLiteral("Mode")).toString();
            if (!ssid.isEmpty() && ssid.size() <= 32
                && (mode.isEmpty() || mode == QStringLiteral("infrastructure"))) {
                const auto wpa = qdbus_cast<QVariantMap>(properties.value(QStringLiteral("WPA")));
                const auto rsn = qdbus_cast<QVariantMap>(properties.value(QStringLiteral("RSN")));
                const QStringList keyManagement = wpa.value(QStringLiteral("KeyMgmt")).toStringList()
                    + rsn.value(QStringLiteral("KeyMgmt")).toStringList();
                Security security = Security::Unsupported;
                if (keyManagement.contains(QStringLiteral("wpa-psk"))) {
                    security = Security::Personal;
                } else if (keyManagement.isEmpty() && !properties.value(QStringLiteral("Privacy")).toBool()) {
                    security = Security::Open;
                }
                const QString id = QString::fromLatin1(ssid.toHex()) + QLatin1Char('-')
                    + QString::number(static_cast<int>(security));
                const int signal = properties.value(QStringLiteral("Signal"), -127).toInt();
                auto found = m_scanResults.constFind(id);
                if (found == m_scanResults.cend() || signal > found->signal)
                    m_scanResults.insert(id, {ssid, security, signal});
            }
        }
        readAccessPoint();
    });
}

void WifiSettings::publishScan()
{
    m_accessPoints = std::exchange(m_scanResults, {});
    auto ordered = m_accessPoints.keys();
    std::sort(ordered.begin(), ordered.end(), [this](const QString &left, const QString &right) {
        const auto &a = m_accessPoints.constFind(left).value();
        const auto &b = m_accessPoints.constFind(right).value();
        return a.signal != b.signal ? a.signal > b.signal : left < right;
    });

    QVariantList networks;
    networks.reserve(ordered.size());
    for (const auto &id : std::as_const(ordered)) {
        const auto &network = m_accessPoints.constFind(id).value();
        const bool secured = network.security != Security::Open;
        const bool supported = network.security != Security::Unsupported;
        networks.append(QVariantMap{{QStringLiteral("id"), id},
                                    {QStringLiteral("ssid"), displayName(network.ssid)},
                                    {QStringLiteral("security"), !supported ? tr("Protezione non supportata")
                                        : secured ? QStringLiteral("WPA/WPA2") : tr("Aperta")},
                                    {QStringLiteral("signal"), network.signal},
                                    {QStringLiteral("secured"), secured},
                                    {QStringLiteral("supported"), supported}});
    }
    if (m_networks != networks) {
        m_networks = std::move(networks);
        emit networksChanged();
    }
    finish();
}

void WifiSettings::connectNetwork(const QString &id, const QString &password)
{
    if (!available() || busy())
        return;
    const auto network = m_accessPoints.constFind(id);
    if (network == m_accessPoints.cend() || network->security == Security::Unsupported) {
        setError(tr("Seleziona una rete aperta o WPA/WPA2 personale."));
        return;
    }

    const bool secured = network->security == Security::Personal;
    QVariantMap settings{{QStringLiteral("ssid"), network->ssid},
                         {QStringLiteral("key_mgmt"), secured ? QStringLiteral("WPA-PSK") : QStringLiteral("NONE")},
                         {QStringLiteral("priority"), 1000000}};
    if (secured) {
        const QByteArray key = preSharedKey(password.toUtf8(), network->ssid);
        if (key.isEmpty()) {
            setError(tr("Usa una password WPA da 8 a 63 caratteri ASCII o una chiave di 64 cifre esadecimali."));
            return;
        }
        settings.insert(QStringLiteral("psk"), key);
    }

    m_operation = Operation::Connect;
    m_reconnectOnFailure = connected();
    m_error.clear();
    emit changed();
    callInterface(QStringLiteral("AddNetwork"), {settings}, [this](const QDBusMessage &message) {
        const QDBusReply<QDBusObjectPath> reply(message);
        if (reply.isValid())
            m_trialNetwork = usablePath(reply.value());
        if (m_trialNetwork.isEmpty()) {
            finish(tr("Il servizio Wi-Fi non ha accettato la rete."));
            return;
        }

        call(m_trialNetwork, propertiesApi, QStringLiteral("Set"),
             {networkApi, QStringLiteral("Enabled"), QVariant::fromValue(QDBusVariant(true))},
             [this](const QDBusMessage &message) {
            if (message.type() == QDBusMessage::ErrorMessage) {
                abortConnection(operationFailed());
                return;
            }
            // Reassociate honors network priority without disabling the factory profiles.
            callInterface(QStringLiteral("Reassociate"), {}, [this](const QDBusMessage &message) {
                if (message.type() == QDBusMessage::ErrorMessage) {
                    abortConnection(operationFailed());
                    return;
                }
                m_connectArmed = true;
                m_deadline.start(20000);
                m_connectPoll.start();
                readStatus();
            });
        });
    });
}

void WifiSettings::disconnectNetwork()
{
    if (!available() || busy() || !connected())
        return;
    m_operation = Operation::Disconnect;
    m_error.clear();
    emit changed();
    callInterface(QStringLiteral("Disconnect"), {}, [this](const QDBusMessage &message) {
        finish(message.type() == QDBusMessage::ErrorMessage ? operationFailed() : QString{});
        readStatus();
    });
}

void WifiSettings::removeNetwork(const QString &path)
{
    if (available() && !path.isEmpty())
        callInterface(QStringLiteral("RemoveNetwork"), {objectPath(path)}, [](const QDBusMessage &) {});
}

void WifiSettings::abortConnection(const QString &message)
{
    m_connectArmed = false;
    m_connectPoll.stop();
    m_deadline.stop();
    const QString trial = std::exchange(m_trialNetwork, {});
    if (trial.isEmpty()) {
        finish(message);
        return;
    }

    callInterface(QStringLiteral("RemoveNetwork"), {objectPath(trial)}, [this, message](const QDBusMessage &reply) {
        if (m_reconnectOnFailure)
            callInterface(QStringLiteral("Reconnect"), {}, [](const QDBusMessage &) {});
        finish(reply.type() == QDBusMessage::ErrorMessage
                   ? tr("Impossibile rimuovere la rete di prova dal servizio Wi-Fi.") : message);
        readStatus();
    });
}

void WifiSettings::finish(const QString &error)
{
    m_deadline.stop();
    m_connectPoll.stop();
    m_connectArmed = false;
    m_scanReading = false;
    m_scanResults.clear();
    m_scanPaths.clear();
    m_operation = Operation::Idle;
    m_error = error;
    emit changed();
}

void WifiSettings::clearInterface()
{
    if (available()) {
        auto bus = QDBusConnection::systemBus();
        bus.disconnect(service, m_interfacePath, propertiesApi, QStringLiteral("PropertiesChanged"),
                       this, SLOT(interfaceChanged(QString,QVariantMap,QStringList)));
        bus.disconnect(service, m_interfacePath, interfaceApi, QStringLiteral("ScanDone"),
                       this, SLOT(scanDone(bool)));
    }
    ++m_generation;
    m_probing = false;
    m_readingStatus = false;
    m_statusPending = false;
    m_interfacePath.clear();
    m_currentBss.clear();
    m_state.clear();
    m_ssid.clear();
    m_trialNetwork.clear();
    m_addedNetwork.clear();
    m_accessPoints.clear();
    if (!m_networks.isEmpty()) {
        m_networks.clear();
        emit networksChanged();
    }
    finish(m_operation == Operation::Idle ? QString{} : operationFailed());
}

void WifiSettings::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit changed();
}
