#include "bluetoothsettings.h"

#ifdef Q_OS_LINUX
#include "paperprobluetooth.h"
#endif

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QDebug>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <utility>

Q_DECLARE_METATYPE(BluetoothInterfaces)
Q_DECLARE_METATYPE(BluetoothObjects)

namespace {
const QString bluez = QStringLiteral("org.bluez");
const QString dbus = QStringLiteral("org.freedesktop.DBus");
const QString dbusPath = QStringLiteral("/org/freedesktop/DBus");
const QString objectManager = QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString propertiesApi = QStringLiteral("org.freedesktop.DBus.Properties");
const QString adapterApi = QStringLiteral("org.bluez.Adapter1");
const QString deviceApi = QStringLiteral("org.bluez.Device1");
const QString agentManager = QStringLiteral("org.bluez.AgentManager1");
const QString agentApi = QStringLiteral("org.bluez.Agent1");
const QString agentManagerPath = QStringLiteral("/org/bluez");
const QString agentPath = QStringLiteral("/org/quaderno/peripherals/agent");
const QString rejected = QStringLiteral("org.bluez.Error.Rejected");
const QString canceled = QStringLiteral("org.bluez.Error.Canceled");

QVariant objectPath(const QString &path)
{
    return QVariant::fromValue(QDBusObjectPath(path));
}

QVariant variant(const QVariant &value)
{
    return QVariant::fromValue(QDBusVariant(value));
}

bool deviceFlag(const QVariantMap &device, const QString &property)
{
    return device.value(property).toBool();
}

QString deviceKind(const QVariantMap &device)
{
    const QString icon = device.value(QStringLiteral("Icon")).toString();
    if (icon == QStringLiteral("input-keyboard"))
        return QStringLiteral("keyboard");
    if (icon == QStringLiteral("input-mouse"))
        return QStringLiteral("mouse");
    if (icon == QStringLiteral("computer") || icon == QStringLiteral("phone"))
        return icon;
    if (icon == QStringLiteral("audio-headset") || icon == QStringLiteral("audio-headphones"))
        return QStringLiteral("headset");

    const auto appearance = device.value(QStringLiteral("Appearance")).toUInt();
    const auto deviceClass = device.value(QStringLiteral("Class")).toUInt();
    if (appearance == 0x03c1 || (deviceClass & 0x1f40) == 0x0540)
        return QStringLiteral("keyboard");
    if (appearance == 0x03c2 || (deviceClass & 0x1f80) == 0x0580)
        return QStringLiteral("mouse");
    return QStringLiteral("other");
}

QString passkeyText(const QVariant &value)
{
    return QString::number(value.toUInt()).rightJustified(6, QLatin1Char('0'));
}
}

class BluetoothAgent final : public QDBusVirtualObject
{
public:
    explicit BluetoothAgent(BluetoothSettings &settings) : m_settings(settings) {}

    QString introspect(const QString &path) const override
    {
        if (path != agentPath)
            return {};
        return QStringLiteral(R"(
            <interface name="org.bluez.Agent1">
                <method name="Release"/>
                <method name="RequestPinCode"><arg type="o" direction="in"/><arg type="s" direction="out"/></method>
                <method name="DisplayPinCode"><arg type="o" direction="in"/><arg type="s" direction="in"/></method>
                <method name="RequestPasskey"><arg type="o" direction="in"/><arg type="u" direction="out"/></method>
                <method name="DisplayPasskey"><arg type="o" direction="in"/><arg type="u" direction="in"/><arg type="q" direction="in"/></method>
                <method name="RequestConfirmation"><arg type="o" direction="in"/><arg type="u" direction="in"/></method>
                <method name="RequestAuthorization"><arg type="o" direction="in"/></method>
                <method name="AuthorizeService"><arg type="o" direction="in"/><arg type="s" direction="in"/></method>
                <method name="Cancel"/>
            </interface>)");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override
    {
        return message.path() == agentPath && message.interface() == agentApi && m_settings.agentMessage(message);
    }

private:
    BluetoothSettings &m_settings;
};

BluetoothSettings::BluetoothSettings(bool deviceMode, bool canActivate, QObject *parent)
    : QObject(parent), m_deviceMode(deviceMode), m_canActivate(canActivate)
{
    if (!available())
        return;

    qDBusRegisterMetaType<BluetoothInterfaces>();
    qDBusRegisterMetaType<BluetoothObjects>();
    const QString busName = QStringLiteral("quaderno-peripherals-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_bus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SystemBus, busName));
    if (!m_bus->isConnected()) {
        setStatus(tr("Il bus di sistema Bluetooth non è disponibile."));
        return;
    }

    m_agent = std::make_unique<BluetoothAgent>(*this);
    if (!m_bus->registerVirtualObject(agentPath, m_agent.get())) {
        setStatus(tr("Impossibile avviare la richiesta di abbinamento Bluetooth."));
        return;
    }
    auto *watcher = new QDBusServiceWatcher(bluez, *m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &previous, const QString &owner) {
        if (!previous.isEmpty())
            clearService();
        if (!owner.isEmpty()) {
            if (m_operation == Operation::None)
                m_status.clear();
            refresh();
        }
    });
    m_scanDeadline.setSingleShot(true);
    connect(&m_scanDeadline, &QTimer::timeout, this, &BluetoothSettings::stopScan);
    m_activationDeadline.setSingleShot(true);
    connect(&m_activationDeadline, &QTimer::timeout, &m_activation, &QProcess::kill);
    connect(&m_activation, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_activationDeadline.stop();
            finish(tr("Impossibile avviare il Bluetooth sul Paper Pro."));
        }
    });
    connect(&m_activation, &QProcess::finished, this, [this](int result, QProcess::ExitStatus exitStatus) {
        m_activationDeadline.stop();
        const auto diagnostics = m_activation.readAll().trimmed();
        if (exitStatus != QProcess::NormalExit || result != 0) {
            qWarning().noquote() << "Paper Pro Bluetooth activation:" << diagnostics;
            finish(tr("Impossibile avviare il Bluetooth sul Paper Pro."));
            return;
        }
        finish(tr("Bluetooth attivato."));
        refresh();
    });
}

BluetoothSettings::~BluetoothSettings()
{
    if (!m_bus)
        return;
    m_bus->unregisterObject(agentPath);
    // Releasing the private connection also releases discovery and the registered agent.
    QDBusConnection::disconnectFromBus(m_bus->name());
    m_bus.reset();
}

QString BluetoothSettings::status() const
{
    if (!m_status.isEmpty())
        return m_status;
    if (m_probing)
        return tr("Verifica dell’adattatore Bluetooth…");
    if (!ready())
        return m_canActivate ? tr("Attiva il Bluetooth per cercare dispositivi.")
                             : tr("Nessun adattatore Bluetooth disponibile.");
    return powered() ? tr("Bluetooth attivo.") : tr("Bluetooth disattivato.");
}

void BluetoothSettings::setLocked(bool locked)
{
    m_locked = locked;
    if (locked) {
        stopScan();
        cancelPairing();
    }
}

void BluetoothSettings::call(const QString &path, const QString &interface, const QString &method,
                             const QVariantList &arguments, ReplyHandler handler, int timeout)
{
    const bool queryOwner = interface == dbus;
    auto message = QDBusMessage::createMethodCall(queryOwner ? dbus : m_owner, path, interface, method);
    message.setAutoStartService(false);
    message.setArguments(arguments);
    const auto pending = m_bus->asyncCall(message, timeout);
    if (!handler)
        return;
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation = m_generation, handler = std::move(handler)](QDBusPendingCallWatcher *finished) {
        const auto reply = finished->reply();
        finished->deleteLater();
        if (generation == m_generation)
            handler(reply);
    });
}

void BluetoothSettings::refresh()
{
    if (!m_bus || !m_bus->isConnected() || m_probing)
        return;
    m_probing = true;
    emit changed();
    auto loadObjects = [this] {
        call(QStringLiteral("/"), objectManager, QStringLiteral("GetManagedObjects"), {},
             [this](const QDBusMessage &message) {
            m_probing = false;
            const QDBusReply<BluetoothObjects> reply(message);
            if (reply.isValid()) {
                m_adapters.clear();
                m_knownDevices.clear();
                const auto objects = reply.value();
                for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
                    if (it.value().contains(adapterApi))
                        m_adapters.insert(it.key().path(), it.value().value(adapterApi));
                    if (it.value().contains(deviceApi))
                        m_knownDevices.insert(it.key().path(), it.value().value(deviceApi));
                }
                updateAdapter();
            }
            emit changed();
        });
    };
    if (!m_owner.isEmpty()) {
        loadObjects();
        return;
    }
    call(dbusPath, dbus, QStringLiteral("GetNameOwner"), {bluez}, [this, loadObjects](const QDBusMessage &message) {
        const QDBusReply<QString> reply(message);
        if (!reply.isValid() || reply.value().isEmpty()) {
            m_probing = false;
            emit changed();
            return;
        }
        m_owner = reply.value();
        m_bus->connect(m_owner, QStringLiteral("/"), objectManager, QStringLiteral("InterfacesAdded"),
                       this, SLOT(interfacesAdded(QDBusObjectPath,BluetoothInterfaces)));
        m_bus->connect(m_owner, QStringLiteral("/"), objectManager, QStringLiteral("InterfacesRemoved"),
                       this, SLOT(interfacesRemoved(QDBusObjectPath,QStringList)));
        m_bus->connect(m_owner, QString(), propertiesApi, QStringLiteral("PropertiesChanged"),
                       this, SLOT(propertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
        loadObjects();
    });
}

void BluetoothSettings::clearService()
{
    ++m_generation;
    m_bus->disconnect(m_owner, QStringLiteral("/"), objectManager, QStringLiteral("InterfacesAdded"),
                      this, SLOT(interfacesAdded(QDBusObjectPath,BluetoothInterfaces)));
    m_bus->disconnect(m_owner, QStringLiteral("/"), objectManager, QStringLiteral("InterfacesRemoved"),
                      this, SLOT(interfacesRemoved(QDBusObjectPath,QStringList)));
    m_bus->disconnect(m_owner, QString(), propertiesApi, QStringLiteral("PropertiesChanged"),
                      this, SLOT(propertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    m_owner.clear();
    m_agentRegistered = false;
    m_probing = false;
    m_adapters.clear();
    m_knownDevices.clear();
    updateAdapter();
    if (m_activation.state() == QProcess::NotRunning)
        finish(tr("Il servizio Bluetooth si è scollegato."));
}

void BluetoothSettings::interfacesAdded(const QDBusObjectPath &path, const BluetoothInterfaces &interfaces)
{
    const QString id = path.path();
    if (interfaces.contains(adapterApi)) {
        m_adapters.insert(id, interfaces.value(adapterApi));
        updateAdapter();
    }
    if (interfaces.contains(deviceApi)) {
        m_knownDevices.insert(id, interfaces.value(deviceApi));
        if (scanning() && id.startsWith(m_adapterPath + QLatin1Char('/')))
            m_seen.insert(id);
        updateDevices();
    }
}

void BluetoothSettings::interfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces)
{
    const QString id = path.path();
    if (interfaces.contains(deviceApi)) {
        m_knownDevices.remove(id);
        m_seen.remove(id);
        updateDevices();
    }
    if (interfaces.contains(adapterApi)) {
        m_adapters.remove(id);
        updateAdapter();
    }
}

void BluetoothSettings::propertiesChanged(const QString &interface, const QVariantMap &properties,
                                          const QStringList &invalidated, const QDBusMessage &message)
{
    const auto id = message.path();
    if (interface == adapterApi && m_adapters.contains(id)) {
        auto &adapter = m_adapters[id];
        if (invalidated.contains(QStringLiteral("Powered"))) {
            refresh();
            return;
        }
        const QString poweredProperty = QStringLiteral("Powered");
        if (properties.contains(poweredProperty)) {
            adapter.insert(poweredProperty, properties.value(poweredProperty));
            updateAdapter();
        }
        return;
    }
    if (interface != deviceApi || !m_knownDevices.contains(id))
        return;

    auto &device = m_knownDevices[id];
    for (auto it = properties.cbegin(); it != properties.cend(); ++it)
        device.insert(it.key(), it.value());
    for (const auto &property : invalidated)
        device.remove(property);
    if (scanning() && id.startsWith(m_adapterPath + QLatin1Char('/')) &&
        (properties.contains(QStringLiteral("RSSI")) || properties.contains(QStringLiteral("Name"))))
        m_seen.insert(id);
    updateDevices();
}

void BluetoothSettings::updateAdapter()
{
    // Keep the current adapter while it is present; hci0 sorts first on a Paper Pro.
    const QString previous = m_adapterPath;
    const bool wasPowered = m_powered;
    if (!m_adapters.contains(m_adapterPath))
        m_adapterPath = m_adapters.isEmpty() ? QString{} : m_adapters.firstKey();
    m_powered = deviceFlag(m_adapters.value(m_adapterPath), QStringLiteral("Powered"));
    if (wasPowered != powered() && m_operation == Operation::None)
        m_status.clear();
    if (m_adapterPath != previous || !powered()) {
        m_seen.clear();
        m_scanDeadline.stop();
        m_scanRequested = false;
        m_scanOwned = false;
        m_scanPending = false;
    }
    updateDevices();
    if (wasPowered != powered() || m_adapterPath != previous)
        emit changed();
}

void BluetoothSettings::updateDevices()
{
    QVariantList devices;
    const QString prefix = m_adapterPath + QLatin1Char('/');
    for (auto it = m_knownDevices.cbegin(); ready() && it != m_knownDevices.cend(); ++it) {
        if (!it.key().startsWith(prefix))
            continue;
        const auto &device = it.value();
        const bool paired = deviceFlag(device, QStringLiteral("Paired"));
        const bool connected = deviceFlag(device, QStringLiteral("Connected"));
        if (!paired && !connected && !m_seen.contains(it.key()))
            continue;
        const QString address = device.value(QStringLiteral("Address")).toString();
        QString name = device.value(QStringLiteral("Alias")).toString().trimmed();
        const QString kind = deviceKind(device);
        if (name == address || name == QString(address).replace(QLatin1Char(':'), QLatin1Char('-')))
            name.clear();
        if (name.isEmpty() && !paired && !connected && kind == QStringLiteral("other"))
            continue;
        devices.append(QVariantMap{{QStringLiteral("id"), it.key()}, {QStringLiteral("name"), name.left(248)},
                                   {QStringLiteral("address"), address}, {QStringLiteral("kind"), kind},
                                   {QStringLiteral("paired"), paired}, {QStringLiteral("connected"), connected}});
    }
    std::sort(devices.begin(), devices.end(), [](const QVariant &first, const QVariant &second) {
        const auto a = first.toMap();
        const auto b = second.toMap();
        for (const auto &flag : {QStringLiteral("connected"), QStringLiteral("paired")}) {
            if (deviceFlag(a, flag) != deviceFlag(b, flag))
                return deviceFlag(a, flag);
        }
        const int names = QString::localeAwareCompare(a.value(QStringLiteral("name")).toString(),
                                                     b.value(QStringLiteral("name")).toString());
        if (names != 0)
            return names < 0;
        return a.value(QStringLiteral("id")).toString() < b.value(QStringLiteral("id")).toString();
    });
    if (devices != m_devices) {
        m_devices = std::move(devices);
        emit devicesChanged();
    }
}

void BluetoothSettings::setStatus(const QString &status)
{
    m_status = status;
    emit changed();
}

void BluetoothSettings::setPowered(bool enabled)
{
    if (!available() || m_locked || busy() || enabled == powered())
        return;
    if (!ready() && !m_canActivate)
        return;
    if (!enabled)
        stopScan();
    m_operation = Operation::Power;
    setStatus(enabled ? tr("Attivazione del Bluetooth…") : tr("Disattivazione del Bluetooth…"));
#ifdef Q_OS_LINUX
    if (enabled && m_canActivate) {
        m_activationDeadline.start(75000);
        startPaperProBluetooth(m_activation, true);
        return;
    }
#endif
    setPoweredOnAdapter(enabled);
}

void BluetoothSettings::setPoweredOnAdapter(bool enabled)
{
    call(m_adapterPath, propertiesApi, QStringLiteral("Set"),
         {adapterApi, QStringLiteral("Powered"), variant(enabled)},
         [this, enabled](const QDBusMessage &reply) {
        if (reply.type() == QDBusMessage::ErrorMessage) {
            finish(tr("Impossibile cambiare lo stato del Bluetooth."));
            return;
        }
        finish(enabled ? tr("Bluetooth attivato.") : tr("Bluetooth disattivato."));
        refresh();
    });
}

void BluetoothSettings::startScan()
{
    if (m_locked || !powered() || busy() || scanning())
        return;
    m_seen.clear();
    m_scanRequested = true;
    m_scanDeadline.start(20000);
    updateDevices();
    setStatus(tr("Ricerca dei dispositivi Bluetooth…"));
    updateDiscovery();
}

void BluetoothSettings::stopScan()
{
    if (!scanning())
        return;
    m_scanRequested = false;
    m_scanDeadline.stop();
    if (m_operation == Operation::None)
        setStatus({});
    else
        emit changed();
    updateDiscovery();
}

void BluetoothSettings::updateDiscovery()
{
    if (!powered() || m_scanPending || m_scanRequested == m_scanOwned)
        return;
    const auto adapter = m_adapterPath;
    const bool start = m_scanRequested;
    m_scanPending = true;
    call(adapter, adapterApi, start ? QStringLiteral("StartDiscovery") : QStringLiteral("StopDiscovery"), {},
         [this, adapter, start](const QDBusMessage &reply) {
        if (adapter != m_adapterPath || !m_scanPending)
            return;
        m_scanPending = false;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            m_scanOwned = false;
            if (start && m_scanRequested) {
                m_scanRequested = false;
                m_scanDeadline.stop();
                setStatus(tr("Impossibile avviare la ricerca dei dispositivi Bluetooth."));
            } else {
                updateDiscovery();
            }
            return;
        }
        m_scanOwned = start;
        updateDiscovery();
    });
}

bool BluetoothSettings::usableDevice(const QString &id) const
{
    if (m_locked || busy() || !powered())
        return false;
    return std::any_of(m_devices.cbegin(), m_devices.cend(), [&id](const QVariant &device) {
        return device.toMap().value(QStringLiteral("id")) == id;
    });
}

void BluetoothSettings::pair(const QString &id)
{
    if (!usableDevice(id) || deviceFlag(m_knownDevices.value(id), QStringLiteral("Paired")))
        return;
    stopScan();
    m_operation = Operation::Pair;
    m_operationDevice = id;
    setStatus(tr("Abbinamento Bluetooth in corso…"));
    if (m_agentRegistered) {
        startPair(id);
    } else {
        call(agentManagerPath, agentManager, QStringLiteral("RegisterAgent"),
             {objectPath(agentPath), QStringLiteral("KeyboardDisplay")}, [this, id](const QDBusMessage &reply) {
            m_agentRegistered = reply.type() != QDBusMessage::ErrorMessage
                || reply.errorName() == QStringLiteral("org.bluez.Error.AlreadyExists");
            if (m_operation != Operation::Pair || m_operationDevice != id)
                return;
            if (m_pairCancelled || m_locked)
                finish(tr("Abbinamento Bluetooth annullato."));
            else if (m_agentRegistered)
                startPair(id);
            else
                finish(tr("Impossibile avviare la richiesta di abbinamento Bluetooth."));
        });
    }
}

void BluetoothSettings::startPair(const QString &id)
{
    m_pairStarted = true;
    call(id, deviceApi, QStringLiteral("Pair"), {}, [this, id](const QDBusMessage &reply) {
        if (m_operation != Operation::Pair || m_operationDevice != id)
            return;
        m_pairStarted = false;
        clearPrompt(canceled);
        if (m_pairCancelled || m_locked) {
            finish(tr("Abbinamento Bluetooth annullato."));
        } else if (reply.type() == QDBusMessage::ErrorMessage
                   && reply.errorName() != QStringLiteral("org.bluez.Error.AlreadyExists")) {
            if (reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.NoReply"))
                call(id, deviceApi, QStringLiteral("CancelPairing"), {});
            finish(tr("Abbinamento non riuscito. Rendi visibile il dispositivo e riprova."));
        } else {
            call(id, propertiesApi, QStringLiteral("Set"), {deviceApi, QStringLiteral("Trusted"), variant(true)},
                 [this, id](const QDBusMessage &reply) {
                if (m_pairCancelled || m_locked) {
                    finish(tr("Abbinamento Bluetooth annullato."));
                } else if (reply.type() == QDBusMessage::ErrorMessage) {
                    finish(tr("Dispositivo abbinato. Il collegamento automatico non è disponibile."));
                } else {
                    connectPaired(id);
                }
            });
        }
    }, 90000);
}

void BluetoothSettings::connectPaired(const QString &id)
{
    call(id, deviceApi, QStringLiteral("Connect"), {}, [this](const QDBusMessage &reply) {
        const bool connected = reply.type() != QDBusMessage::ErrorMessage
            || reply.errorName() == QStringLiteral("org.bluez.Error.AlreadyConnected");
        const bool paired = m_operation == Operation::Pair;
        finish(connected ? tr("Dispositivo Bluetooth collegato.")
                         : paired ? tr("Dispositivo abbinato. Riattivalo e premi Connetti.")
                                  : tr("Impossibile collegare il dispositivo Bluetooth."));
        refresh();
    }, 30000);
}

void BluetoothSettings::connectDevice(const QString &id)
{
    if (!usableDevice(id) || !deviceFlag(m_knownDevices.value(id), QStringLiteral("Paired")))
        return;
    stopScan();
    m_operation = Operation::Connect;
    m_operationDevice = id;
    setStatus(tr("Collegamento Bluetooth in corso…"));
    connectPaired(id);
}

void BluetoothSettings::disconnectDevice(const QString &id)
{
    deviceAction(id, Operation::Disconnect, QStringLiteral("Disconnect"), tr("Dispositivo Bluetooth scollegato."),
                 tr("Impossibile scollegare il dispositivo Bluetooth."), QStringLiteral("org.bluez.Error.NotConnected"));
}

void BluetoothSettings::forget(const QString &id)
{
    deviceAction(id, Operation::Forget, QStringLiteral("RemoveDevice"), tr("Abbinamento Bluetooth rimosso."),
                 tr("Impossibile rimuovere l’abbinamento Bluetooth."), QStringLiteral("org.bluez.Error.DoesNotExist"));
}

void BluetoothSettings::deviceAction(const QString &id, Operation operation, const QString &method,
                                     const QString &success, const QString &failure, const QString &alreadyDone)
{
    if (!usableDevice(id))
        return;
    const bool removing = operation == Operation::Forget;
    const auto flag = removing ? QStringLiteral("Paired") : QStringLiteral("Connected");
    if (!deviceFlag(m_knownDevices.value(id), flag))
        return;
    stopScan();
    m_operation = operation;
    m_operationDevice = id;
    setStatus(removing ? tr("Rimozione dell’abbinamento Bluetooth…") : tr("Disconnessione Bluetooth in corso…"));
    call(removing ? m_adapterPath : id, removing ? adapterApi : deviceApi, method,
         removing ? QVariantList{objectPath(id)} : QVariantList{}, [this, success, failure, alreadyDone](const QDBusMessage &reply) {
        finish(reply.type() != QDBusMessage::ErrorMessage || reply.errorName() == alreadyDone ? success : failure);
        refresh();
    });
}

void BluetoothSettings::finish(const QString &status)
{
    clearPrompt(canceled);
    m_operation = Operation::None;
    m_operationDevice.clear();
    m_pairStarted = false;
    m_pairCancelled = false;
    setStatus(status);
}

void BluetoothSettings::setPrompt(const QString &type, const QString &code, int entered)
{
    const auto device = m_knownDevices.value(m_operationDevice);
    QString name = device.value(QStringLiteral("Alias")).toString().trimmed();
    if (name.isEmpty())
        name = device.value(QStringLiteral("Address")).toString();
    m_promptType = type;
    m_promptCode = code;
    m_promptName = name.left(248);
    m_promptEntered = qBound(0, entered, 6);
    emit changed();
}

void BluetoothSettings::clearPrompt(const QString &error)
{
    if (!error.isEmpty() && m_promptRequest.type() == QDBusMessage::MethodCallMessage)
        m_bus->send(m_promptRequest.createErrorReply(error, error));
    m_promptRequest = {};
    if (m_promptType.isEmpty())
        return;
    m_promptType.clear();
    m_promptCode.clear();
    m_promptName.clear();
    m_promptEntered = 0;
    emit changed();
}

void BluetoothSettings::answerPrompt(const QString &value)
{
    if (m_locked || m_promptRequest.type() != QDBusMessage::MethodCallMessage)
        return;
    QVariantList reply;
    if (m_promptType == QStringLiteral("passkey")) {
        static const QRegularExpression digits(QStringLiteral("^[0-9]{1,6}$"));
        if (!digits.match(value).hasMatch())
            return;
        reply.append(value.toUInt());
    } else if (m_promptType == QStringLiteral("pin")) {
        static const QRegularExpression pin(QStringLiteral("^[!-~]{1,16}$"));
        if (!pin.match(value).hasMatch())
            return;
        reply.append(value);
    }
    m_bus->send(m_promptRequest.createReply(reply));
    clearPrompt();
}

void BluetoothSettings::cancelPairing()
{
    if (m_operation != Operation::Pair || m_pairCancelled)
        return;
    m_pairCancelled = true;
    clearPrompt(canceled);
    if (m_pairStarted)
        call(m_operationDevice, deviceApi, QStringLiteral("CancelPairing"), {});
}

bool BluetoothSettings::agentMessage(const QDBusMessage &message)
{
    // Only this BlueZ instance may ask the app to display or supply a pairing secret.
    if (m_owner.isEmpty() || message.service() != m_owner) {
        m_bus->send(message.createErrorReply(rejected, rejected));
        return true;
    }
    const QString method = message.member();
    if (method == QStringLiteral("Release") || method == QStringLiteral("Cancel")) {
        if (method == QStringLiteral("Release"))
            m_agentRegistered = false;
        clearPrompt(canceled);
        m_bus->send(message.createReply());
        return true;
    }
    const QVariantList args = message.arguments();
    const auto path = args.value(0).value<QDBusObjectPath>().path();
    if (m_operation != Operation::Pair || m_pairCancelled || m_locked || path != m_operationDevice) {
        m_bus->send(message.createErrorReply(rejected, rejected));
        return true;
    }
    if (m_promptRequest.type() == QDBusMessage::MethodCallMessage) {
        m_bus->send(message.createErrorReply(rejected, rejected));
        return true;
    }
    if (method == QStringLiteral("DisplayPinCode") || method == QStringLiteral("DisplayPasskey")) {
        const bool isPin = method == QStringLiteral("DisplayPinCode");
        setPrompt(QStringLiteral("display"), isPin ? args.value(1).toString().left(16) : passkeyText(args.value(1)),
                  isPin ? 0 : args.value(2).toInt());
        m_bus->send(message.createReply());
        return true;
    }

    QString type;
    QString code;
    if (method == QStringLiteral("RequestPinCode")) {
        type = QStringLiteral("pin");
    } else if (method == QStringLiteral("RequestPasskey")) {
        type = QStringLiteral("passkey");
    } else if (method == QStringLiteral("RequestConfirmation")) {
        type = QStringLiteral("confirm");
        code = passkeyText(args.value(1));
    } else if (method == QStringLiteral("RequestAuthorization") || method == QStringLiteral("AuthorizeService")) {
        type = QStringLiteral("confirm");
    } else {
        return false;
    }
    message.setDelayedReply(true);
    m_promptRequest = message;
    setPrompt(type, code);
    return true;
}
