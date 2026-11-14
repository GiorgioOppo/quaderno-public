#include "bleperipheral.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QDir>
#include <QMap>
#include <QTimer>
#include <QUuid>

using BleInterfaces = QMap<QString, QVariantMap>;
using BleManagedObjects = QMap<QDBusObjectPath, BleInterfaces>;
Q_DECLARE_METATYPE(BleInterfaces)
Q_DECLARE_METATYPE(BleManagedObjects)

namespace {
const QString bluez = QStringLiteral("org.bluez");
const QString dbus = QStringLiteral("org.freedesktop.DBus");
const QString objectManager = QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString properties = QStringLiteral("org.freedesktop.DBus.Properties");
const QString adapterInterface = QStringLiteral("org.bluez.Adapter1");
const QString deviceInterface = QStringLiteral("org.bluez.Device1");
const QString gattManager = QStringLiteral("org.bluez.GattManager1");
const QString advertisingManager = QStringLiteral("org.bluez.LEAdvertisingManager1");
const QString serviceInterface = QStringLiteral("org.bluez.GattService1");
const QString characteristic = QStringLiteral("org.bluez.GattCharacteristic1");
const QString advertisementInterface = QStringLiteral("org.bluez.LEAdvertisement1");

const QString applicationPath = QStringLiteral("/org/quaderno/ble");
const QString servicePath = applicationPath + QStringLiteral("/service");
const QString writePath = servicePath + QStringLiteral("/receive");
const QString notifyPath = servicePath + QStringLiteral("/send");
const QString advertisementPath = applicationPath + QStringLiteral("/advertisement");
const QString serviceUuid = QStringLiteral("31cb599d-0664-4b7b-a1c8-d854da913c40");
const QString writeUuid = QStringLiteral("31cb599d-0664-4b7b-a1c8-d854da913c41");
const QString notifyUuid = QStringLiteral("31cb599d-0664-4b7b-a1c8-d854da913c42");
constexpr int dbusTimeoutMs = 5000;
constexpr qsizetype maximumFrame = 244;

QDBusMessage methodCall(const QString &destination, const QString &path, const QString &interface,
                        const QString &method, const QVariantList &arguments = {})
{
    auto message = QDBusMessage::createMethodCall(destination, path, interface, method);
    message.setAutoStartService(false);
    message.setArguments(arguments);
    return message;
}

QVariant objectPath(const QString &path)
{
    return QVariant::fromValue(QDBusObjectPath(path));
}
}

class BleGattObjects : public QDBusVirtualObject
{
public:
    explicit BleGattObjects(BlePeripheral &peripheral) : m_peripheral(peripheral) {}

    QString introspect(const QString &path) const override
    {
        if (path == applicationPath) {
            return QStringLiteral(R"(
                <interface name="org.freedesktop.DBus.ObjectManager">
                    <method name="GetManagedObjects">
                        <arg type="a{oa{sa{sv}}}" direction="out"/>
                    </method>
                </interface>
                <node name="service"/>
                <node name="advertisement"/>
            )");
        }
        if (path == servicePath) {
            return QStringLiteral(R"(
                <interface name="org.bluez.GattService1">
                    <property name="UUID" type="s" access="read"/>
                    <property name="Primary" type="b" access="read"/>
                </interface>
                <node name="receive"/>
                <node name="send"/>
            )");
        }
        if (path == advertisementPath) {
            return QStringLiteral(R"(
                <interface name="org.bluez.LEAdvertisement1">
                    <method name="Release"/>
                    <property name="Type" type="s" access="read"/>
                    <property name="ServiceUUIDs" type="as" access="read"/>
                </interface>
            )");
        }
        if (path != writePath && path != notifyPath)
            return {};
        const QString methods = path == writePath ? QStringLiteral(R"(
                <method name="WriteValue">
                    <arg type="ay" direction="in"/>
                    <arg type="a{sv}" direction="in"/>
                </method>
            )") : QStringLiteral(R"(
                <method name="StartNotify"/>
                <method name="StopNotify"/>
                <property name="Value" type="ay" access="read"/>
            )");
        return QStringLiteral(R"(
            <interface name="org.bluez.GattCharacteristic1">
                <property name="UUID" type="s" access="read"/>
                <property name="Service" type="o" access="read"/>
                <property name="Flags" type="as" access="read"/>
                %1
            </interface>
        )").arg(methods);
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.type() != QDBusMessage::MethodCallMessage)
            return false;
        if (message.service() != m_peripheral.m_bluezOwner) {
            bus.send(message.createErrorReply(QDBusError::AccessDenied, QStringLiteral("BlueZ only")));
            return true;
        }
        if (message.interface() == properties)
            return handleProperties(message, bus);

        const QString path = message.path();
        const QString method = message.member();
        if (path == applicationPath && message.interface() == objectManager
                && method == QStringLiteral("GetManagedObjects") && message.arguments().isEmpty()) {
            const BleManagedObjects objects{
                {QDBusObjectPath(servicePath), interfacesAt(servicePath)},
                {QDBusObjectPath(writePath), interfacesAt(writePath)},
                {QDBusObjectPath(notifyPath), interfacesAt(notifyPath)}
            };
            bus.send(message.createReply(QVariant::fromValue(objects)));
            return true;
        }
        if (path == advertisementPath && message.interface() == advertisementInterface
                && method == QStringLiteral("Release") && message.arguments().isEmpty()) {
            bus.send(message.createReply());
            const quint64 generation = m_peripheral.m_generation;
            QTimer::singleShot(0, &m_peripheral, [&peripheral = m_peripheral, generation] {
                if (generation == peripheral.m_generation)
                    peripheral.finish(BlePeripheral::tr("La trasmissione Bluetooth è stata interrotta."));
            });
            return true;
        }
        if (message.interface() != characteristic)
            return false;
        if (path == writePath && method == QStringLiteral("WriteValue"))
            return write(message, bus);
        if (path == notifyPath && message.arguments().isEmpty()
                && (method == QStringLiteral("StartNotify") || method == QStringLiteral("StopNotify"))) {
            const bool notifying = method == QStringLiteral("StartNotify");
            m_peripheral.m_notifying = notifying;
            bus.send(message.createReply());
            if (!notifying) {
                const quint64 generation = m_peripheral.m_generation;
                QTimer::singleShot(0, &m_peripheral, [&peripheral = m_peripheral, generation] {
                    if (generation == peripheral.m_generation && !peripheral.m_notifying)
                        peripheral.clearPeer();
                });
            }
            return true;
        }
        return false;
    }

private:
    BleInterfaces interfacesAt(const QString &path) const
    {
        if (path == servicePath) {
            return {{serviceInterface, {
                {QStringLiteral("UUID"), serviceUuid},
                {QStringLiteral("Primary"), true}
            }}};
        }
        if (path == advertisementPath) {
            // A 128-bit UUID fits legacy advertising. Keep the remaining
            // space free so BlueZ can add flags on this controller.
            return {{advertisementInterface, {
                {QStringLiteral("Type"), QStringLiteral("peripheral")},
                {QStringLiteral("ServiceUUIDs"), QStringList{serviceUuid}}
            }}};
        }
        if (path != writePath && path != notifyPath)
            return {};
        const bool incoming = path == writePath;
        QVariantMap values{
            {QStringLiteral("UUID"), incoming ? writeUuid : notifyUuid},
            {QStringLiteral("Service"), objectPath(servicePath)},
            {QStringLiteral("Flags"), QStringList{incoming ? QStringLiteral("write")
                                                           : QStringLiteral("notify")}}
        };
        if (!incoming)
            values.insert(QStringLiteral("Value"), QByteArray());
        return {{characteristic, values}};
    }

    bool handleProperties(const QDBusMessage &message, const QDBusConnection &bus) const
    {
        const auto arguments = message.arguments();
        const bool all = message.member() == QStringLiteral("GetAll") && arguments.size() == 1;
        const bool one = message.member() == QStringLiteral("Get") && arguments.size() == 2;
        if (!all && !one)
            return false;

        const auto interfaces = interfacesAt(message.path());
        const auto values = interfaces.constFind(arguments.at(0).toString());
        if (values == interfaces.cend()) {
            bus.send(message.createErrorReply(QDBusError::UnknownInterface,
                                              QStringLiteral("Unknown interface")));
        } else if (all) {
            bus.send(message.createReply(QVariant::fromValue(values.value())));
        } else {
            const auto value = values->constFind(arguments.at(1).toString());
            if (value == values->cend()) {
                bus.send(message.createErrorReply(QDBusError::UnknownProperty,
                                                  QStringLiteral("Unknown property")));
            } else {
                bus.send(message.createReply(QVariant::fromValue(QDBusVariant(value.value()))));
            }
        }
        return true;
    }

    bool write(const QDBusMessage &message, const QDBusConnection &bus)
    {
        const auto arguments = message.arguments();
        if (message.signature() != QStringLiteral("aya{sv}") || arguments.size() != 2) {
            bus.send(message.createErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid GATT write")));
            return true;
        }

        const auto options = qdbus_cast<QVariantMap>(arguments.at(1));
        const QString peer = qdbus_cast<QDBusObjectPath>(options.value(QStringLiteral("device"))).path();
        const QString link = options.value(QStringLiteral("link")).toString();
        // BlueZ may omit the type for a local GATT characteristic.
        const QString writeType = options.value(QStringLiteral("type")).toString();
        if (!m_peripheral.running() || !m_peripheral.m_notifying
                || !peer.startsWith(m_peripheral.m_adapterPath + QStringLiteral("/dev_"))
                || (!link.isEmpty() && link != QStringLiteral("LE"))
                || (!writeType.isEmpty() && writeType != QStringLiteral("request"))
                || (!m_peripheral.m_peerPath.isEmpty() && m_peripheral.m_peerPath != peer)
                || options.value(QStringLiteral("prepare-authorize")).toBool()) {
            bus.send(message.createErrorReply(QStringLiteral("org.bluez.Error.NotPermitted"),
                                              QStringLiteral("This BLE session cannot accept the write")));
            return true;
        }
        if (options.value(QStringLiteral("offset")).toUInt() != 0) {
            bus.send(message.createErrorReply(QStringLiteral("org.bluez.Error.InvalidOffset"),
                                              QStringLiteral("Long GATT writes are unsupported")));
            return true;
        }
        const QByteArray frame = arguments.at(0).toByteArray();
        const unsigned mtu = options.value(QStringLiteral("mtu")).toUInt();
        const qsizetype size = mtu >= 23 ? qMin(qsizetype(mtu) - 3, maximumFrame)
                                        : m_peripheral.m_chunkSize;
        if (frame.isEmpty() || frame.size() > size) {
            bus.send(message.createErrorReply(QStringLiteral("org.bluez.Error.InvalidValueLength"),
                                              QStringLiteral("Invalid BLE frame size")));
            return true;
        }

        const bool first = m_peripheral.m_peerPath.isEmpty();
        m_peripheral.m_peerPath = peer;
        m_peripheral.m_chunkSize = size;
        const quint64 generation = m_peripheral.m_generation;
        bus.send(message.createReply());
        QMetaObject::invokeMethod(&m_peripheral, [&peripheral = m_peripheral, frame, peer, first, generation] {
            if (generation != peripheral.m_generation || peer != peripheral.m_peerPath
                    || !peripheral.m_notifying)
                return;
            if (first)
                emit peripheral.peerChanged(true);
            if (generation == peripheral.m_generation && peer == peripheral.m_peerPath)
                emit peripheral.frameReceived(frame);
        }, Qt::QueuedConnection);
        return true;
    }

    BlePeripheral &m_peripheral;
};

BlePeripheral::BlePeripheral(QObject *parent)
    : QObject(parent), m_status(tr("Bluetooth da verificare."))
{
    qDBusRegisterMetaType<BleInterfaces>();
    qDBusRegisterMetaType<BleManagedObjects>();
}

BlePeripheral::~BlePeripheral()
{
    closeBus();
}

void BlePeripheral::refresh()
{
    probe(false);
}

void BlePeripheral::start()
{
    probe(true);
}

void BlePeripheral::probe(bool launch)
{
    if (m_state == State::Probing) {
        m_launchAfterProbe |= launch;
        return;
    }
    if (m_state != State::Idle)
        return;

    m_ready = false;
    m_activationNeeded = false;
    m_launchAfterProbe = launch;
    const QString name = QStringLiteral("quaderno-ble-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_bus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SystemBus, name));
    if (!m_bus->isConnected()) {
        finish(tr("Bus di sistema Bluetooth non disponibile."));
        return;
    }

    m_state = State::Probing;
    m_status = tr("Verifica dell’adattatore Bluetooth…");
    const quint64 generation = ++m_generation;
    auto *watcher = new QDBusPendingCallWatcher(m_bus->asyncCall(methodCall(dbus, QStringLiteral("/org/freedesktop/DBus"),
                dbus, QStringLiteral("GetNameOwner"), {bluez}), dbusTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *ownerWatcher) {
        const QDBusPendingReply<QString> owner = *ownerWatcher;
        ownerWatcher->deleteLater();
        if (generation != m_generation)
            return;
        if (owner.isError() || owner.value().isEmpty()) {
            m_activationNeeded = owner.error().name() == QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner");
            finish(QDir(QStringLiteral("/sys/class/bluetooth")).entryList(
                       QStringList{QStringLiteral("hci*")}, QDir::Dirs).isEmpty()
                       ? tr("Nessun adattatore Bluetooth esposto dal sistema.")
                       : tr("Il servizio Bluetooth BlueZ non è attivo."));
            return;
        }
        m_bluezOwner = owner.value();
        if (!m_bus->connect(dbus, QStringLiteral("/org/freedesktop/DBus"), dbus,
                            QStringLiteral("NameOwnerChanged"), QStringList{bluez},
                            QStringLiteral("sss"), this,
                            SLOT(bluezOwnerChanged(QString,QString,QString)))
                || !m_bus->connect(m_bluezOwner, QString(), properties, QStringLiteral("PropertiesChanged"),
                                   this, SLOT(bluezPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)))) {
            finish(tr("Impossibile monitorare il servizio Bluetooth."));
            return;
        }
        auto *objectsWatcher = new QDBusPendingCallWatcher(m_bus->asyncCall(methodCall(m_bluezOwner,
                    QStringLiteral("/"), objectManager, QStringLiteral("GetManagedObjects")), dbusTimeoutMs), this);
        connect(objectsWatcher, &QDBusPendingCallWatcher::finished, this,
                [this, generation](QDBusPendingCallWatcher *replyWatcher) {
            const QDBusPendingReply<BleManagedObjects> reply = *replyWatcher;
            replyWatcher->deleteLater();
            if (generation != m_generation)
                return;
            if (reply.isError()) {
                finish(tr("Impossibile leggere gli adattatori Bluetooth."));
                return;
            }
            bool adapterFound = false;
            bool supportsPeripheral = false;
            const auto objects = reply.value();
            for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
                if (!it.value().contains(adapterInterface))
                    continue;
                adapterFound = true;
                if (!it.value().contains(gattManager) || !it.value().contains(advertisingManager))
                    continue;
                supportsPeripheral = true;
                const auto adapter = it.value().value(adapterInterface);
                if (!adapter.value(QStringLiteral("Powered")).toBool())
                    continue;
                m_adapterPath = it.key().path();
                break;
            }
            if (m_adapterPath.isEmpty()) {
                m_activationNeeded = !adapterFound || supportsPeripheral;
                finish(!adapterFound ? tr("Nessun adattatore Bluetooth esposto dal sistema.")
                                     : !supportsPeripheral ? tr("L’adattatore non espone i servizi BLE richiesti.")
                                                           : tr("L’adattatore Bluetooth è spento."));
                return;
            }
            m_ready = true;
            if (m_launchAfterProbe)
                registerApplication();
            else
                finish(tr("Adattatore Bluetooth pronto."));
        });
    });
    emit changed();
}

void BlePeripheral::registerApplication()
{
    m_objects = std::make_unique<BleGattObjects>(*this);
    if (!m_bus->registerVirtualObject(applicationPath, m_objects.get(), QDBusConnection::SubPath)) {
        finish(tr("Impossibile pubblicare gli oggetti Bluetooth di Quaderno."));
        return;
    }
    m_state = State::Registering;
    m_status = tr("Avvio della condivisione Bluetooth…");
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(m_bus->asyncCall(methodCall(m_bluezOwner, m_adapterPath,
                gattManager, QStringLiteral("RegisterApplication"),
                {objectPath(applicationPath), QVariantMap{}}), dbusTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *reply) {
        const bool error = reply->isError();
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (error)
            finish(tr("BlueZ non ha registrato il servizio Bluetooth di Quaderno."));
        else
            registerAdvertisement();
    });
    emit changed();
}

void BlePeripheral::registerAdvertisement()
{
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(m_bus->asyncCall(methodCall(m_bluezOwner, m_adapterPath,
                advertisingManager, QStringLiteral("RegisterAdvertisement"),
                {objectPath(advertisementPath), QVariantMap{}}), dbusTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *reply) {
        const bool error = reply->isError();
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (error) {
            finish(tr("L’adattatore non ha avviato la trasmissione Bluetooth."));
            return;
        }
        m_state = State::Running;
        m_status = tr("Quaderno è visibile al Mac via Bluetooth.");
        emit changed();
    });
}

bool BlePeripheral::send(const QByteArray &frame)
{
    if (!running() || !m_notifying || m_peerPath.isEmpty() || frame.isEmpty() || frame.size() > m_chunkSize)
        return false;
    auto message = QDBusMessage::createSignal(notifyPath, properties, QStringLiteral("PropertiesChanged"));
    message.setArguments({characteristic, QVariantMap{{QStringLiteral("Value"), frame}}, QStringList{}});
    return m_bus->send(message);
}

void BlePeripheral::bluezOwnerChanged(const QString &, const QString &previous, const QString &current)
{
    if (!m_bluezOwner.isEmpty() && previous == m_bluezOwner && current != m_bluezOwner) {
        m_ready = false;
        finish(tr("Il servizio Bluetooth BlueZ si è scollegato."));
    }
}

void BlePeripheral::bluezPropertiesChanged(const QString &interface, const QVariantMap &changed,
                                         const QStringList &invalidated, const QDBusMessage &message)
{
    const auto powered = QStringLiteral("Powered");
    if (interface == adapterInterface && message.path() == m_adapterPath
            && ((changed.contains(powered) && !changed.value(powered).toBool())
                || invalidated.contains(powered))) {
        m_ready = false;
        finish(tr("L’adattatore Bluetooth è spento."));
        return;
    }
    const auto connected = QStringLiteral("Connected");
    if (interface == deviceInterface && message.path() == m_peerPath
            && ((changed.contains(connected) && !changed.value(connected).toBool())
                || invalidated.contains(connected))) {
        clearPeer();
    }
}

void BlePeripheral::clearPeer()
{
    const bool hadPeer = !m_peerPath.isEmpty();
    m_peerPath.clear();
    m_chunkSize = 20;
    if (hadPeer)
        emit peerChanged(false);
}

void BlePeripheral::closeBus()
{
    if (!m_bus)
        return;
    m_bus->unregisterObject(applicationPath, QDBusConnection::UnregisterTree);
    m_objects.reset();
    // BlueZ owns both registrations by the unique bus name and removes
    // them when that connection closes, even after a partial registration.
    QDBusConnection::disconnectFromBus(m_bus->name());
    m_bus.reset();
}

void BlePeripheral::finish(const QString &status)
{
    ++m_generation;
    m_state = State::Idle;
    m_status = status;
    m_notifying = false;
    m_launchAfterProbe = false;
    closeBus();
    m_bluezOwner.clear();
    m_adapterPath.clear();
    clearPeer();
    emit changed();
}

void BlePeripheral::stop()
{
    if (m_state != State::Idle)
        finish(m_ready ? tr("Adattatore Bluetooth pronto.") : tr("Bluetooth da verificare."));
}
