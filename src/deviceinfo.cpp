#include "deviceinfo.h"
#include "markerbattery.h"

#include <QSysInfo>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>

namespace {
const QString lightDirectory = QStringLiteral("/sys/class/backlight/rm_frontlight/");
const QString lightBrightness = lightDirectory + QStringLiteral("brightness");
const QString lightPower = lightDirectory + QStringLiteral("bl_power");
constexpr int expectedMaximum = 2047;
constexpr int lightOn = 0;
constexpr int lightOff = 4;

// Normal Paper Pro presets with the driver's stock non-linear mapping.
constexpr std::array<int, 6> brightnessPresets{0, 260, 694, 1040, 1387, 1734};

QString readText(const QString &path, qint64 limit = 4096)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    return QString::fromUtf8(file.read(limit).replace('\0', "")).trimmed();
}

int readInteger(const QString &path)
{
    bool ok = false;
    const int value = readText(path).toInt(&ok);
    return ok ? value : -1;
}

bool writeInteger(const QString &path, int value)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Unbuffered))
        return false;

    const QByteArray contents = QByteArray::number(value) + '\n';
    return file.write(contents) == contents.size() && file.flush();
}

bool isPaperProModel(const QString &identity)
{
    const QString model = identity.simplified().toLower();
    return model == QStringLiteral("remarkable ferrari")
        || model == QStringLiteral("remarkable paper pro")
        || model == QStringLiteral("ferrari");
}

QString readOsVersion()
{
    static const QRegularExpression versionLine(
        QStringLiteral(R"re(^IMG_VERSION="?([0-9]+(?:\.[0-9]+)+)"?$)re"));
    const QStringList lines = readText(QStringLiteral("/etc/os-release"), 16384).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const auto match = versionLine.match(line.trimmed());
        if (match.hasMatch())
            return match.captured(1);
    }
    return {};
}

struct Battery {
    int percent = -1;
    QString status;
};

Battery readBattery()
{
    const QDir supplies(QStringLiteral("/sys/class/power_supply"));
    const auto entries = supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        const QDir supply(supplies.filePath(entry));
        if (readText(supply.filePath(QStringLiteral("type"))) != QStringLiteral("Battery")
            || readText(supply.filePath(QStringLiteral("scope"))) == QStringLiteral("Device")
            || readInteger(supply.filePath(QStringLiteral("present"))) == 0) {
            continue;
        }

        const int percent = readInteger(supply.filePath(QStringLiteral("capacity")));
        if (percent >= 0 && percent <= 100)
            return {percent, readText(supply.filePath(QStringLiteral("status")))};
    }
    return {};
}

bool hasBluetoothController()
{
    static const QRegularExpression controller(QStringLiteral("^hci[0-9]+$"));
    const QDir bluetooth(QStringLiteral("/sys/class/bluetooth"));
    const auto entries = bluetooth.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return std::any_of(entries.cbegin(), entries.cend(), [](const QString &entry) {
        return controller.match(entry).hasMatch();
    });
}

void readMarkerProperty(QObject *owner, const QString &property,
                        std::function<void(QVariant)> completed)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("no.remarkable.marker"),
        QStringLiteral("/no/remarkable/marker"), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    message.setAutoStartService(false);
    message.setArguments({QStringLiteral("no.remarkable.marker"), property});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 1000), owner);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, owner,
                     [completed = std::move(completed)](QDBusPendingCallWatcher *finished) {
        const QDBusReply<QDBusVariant> reply(finished->reply());
        finished->deleteLater();
        completed(reply.isValid() ? reply.value().variant() : QVariant{});
    });
}
}
#endif

DeviceInfo::DeviceInfo(bool tabletMode, QObject *parent)
    : QObject(parent), m_tabletMode(tabletMode)
{
    refresh();
}

DeviceInfo::~DeviceInfo()
{
#ifdef Q_OS_LINUX
    if (m_originalLight && m_writtenLight) {
        const LightState current = readLight();
        // Another owner can change the reading light during the session.
        if (current.controllable && current.matches(*m_writtenLight))
            applyLight(current, *m_originalLight);
    }
#endif
}

void DeviceInfo::refresh()
{
    m_model = m_tabletMode ? tr("Anteprima tablet") : tr("Anteprima desktop");
    m_osVersion = QSysInfo::prettyProductName();
    m_paperPro = false;
    m_batteryPercent = -1;
    m_charging = false;
    m_batteryStatus = tr("Disponibile sul Paper Pro");
    m_bluetoothAvailable = false;
    m_bluetoothStatus = tr("Stato rilevabile sul Paper Pro");
    m_frontlightAvailable = false;
    m_frontlightLevel = -1;
    m_frontlightStatus = tr("Disponibile sul Paper Pro");

#ifdef Q_OS_LINUX
    m_paperPro = m_tabletMode && (isPaperProModel(readText(QStringLiteral("/sys/devices/soc0/machine")))
        || isPaperProModel(readText(QStringLiteral("/proc/device-tree/model"))));
    if (m_paperPro) {
        m_model = QStringLiteral("reMarkable Paper Pro");
        m_osVersion = readOsVersion();

        const Battery battery = readBattery();
        m_batteryPercent = battery.percent;
        m_charging = battery.status == QStringLiteral("Charging");
        m_batteryStatus = tr("Stato di carica non disponibile");
        if (m_charging)
            m_batteryStatus = tr("In carica");
        else if (battery.status == QStringLiteral("Full"))
            m_batteryStatus = tr("Carica completa");
        else if (battery.status == QStringLiteral("Discharging"))
            m_batteryStatus = tr("A batteria");
        else if (battery.status == QStringLiteral("Not charging"))
            m_batteryStatus = tr("Carica sospesa");

        m_bluetoothAvailable = hasBluetoothController();
        m_bluetoothStatus = m_bluetoothAvailable ? tr("Adattatore rilevato") : tr("Nessun adattatore esposto dal sistema");

        const LightState light = readLight();
        m_frontlightAvailable = light.controllable;
        if (light.brightness < 0 || light.power < 0) {
            m_frontlightStatus = tr("Luce frontale non rilevata");
        } else if (!light.controllable) {
            m_frontlightStatus = tr("Controllo della luce non disponibile");
        } else if (light.power == lightOff || light.brightness == 0) {
            m_frontlightLevel = 0;
            m_frontlightStatus = tr("Spenta");
        } else {
            const auto preset = std::find(brightnessPresets.begin(), brightnessPresets.end(), light.brightness);
            if (preset != brightnessPresets.end()) {
                m_frontlightLevel = int(preset - brightnessPresets.begin());
                m_frontlightStatus = tr("Livello %1 di 5").arg(m_frontlightLevel);
            } else {
                m_frontlightStatus = tr("Livello impostato dal sistema");
            }
        }
    }
#endif

    refreshMarker();
    publishChanges();
}

void DeviceInfo::publishChanges()
{
    const QVariantList state{m_paperPro, m_model, m_osVersion, m_batteryPercent, m_charging,
        m_batteryStatus, m_markerAvailable, m_markerPresent, m_markerBatteryPercent,
        m_markerCharging, m_markerStatus, m_bluetoothAvailable, m_bluetoothStatus,
        m_frontlightAvailable, m_frontlightLevel, m_frontlightStatus, m_error};
    if (state == m_publishedState)
        return;
    m_publishedState = state;
    emit changed();
}

void DeviceInfo::applyMarkerReading(const MarkerBattery::Reading &reading)
{
    m_markerAvailable = reading.available;
    m_markerPresent = reading.present;
    m_markerBatteryPercent = reading.percent;
    m_markerCharging = reading.charging;
    switch (reading.state) {
    case MarkerBattery::State::Unavailable:
        m_markerStatus = tr("Telemetria della penna non disponibile");
        break;
    case MarkerBattery::State::Absent:
        m_markerStatus = tr("Penna non rilevata");
        break;
    case MarkerBattery::State::Ready:
        m_markerStatus = tr("Penna agganciata");
        break;
    case MarkerBattery::State::Charging:
        m_markerStatus = tr("In carica");
        break;
    case MarkerBattery::State::Full:
        m_markerStatus = tr("Carica completa");
        break;
    case MarkerBattery::State::Discharging:
        m_markerStatus = tr("Penna in uso");
        break;
    case MarkerBattery::State::NotCharging:
        m_markerStatus = tr("Carica sospesa");
        break;
    case MarkerBattery::State::Unknown:
        m_markerStatus = tr("Stato di carica della penna non disponibile");
        break;
    }
}

void DeviceInfo::refreshMarker()
{
#ifdef Q_OS_LINUX
    if (m_paperPro) {
        if (m_markerRefreshPending)
            return;
        m_markerRefreshPending = true;
        struct PendingReading {
            QVariant capacity;
            QVariant docked;
            QVariant charging;
            int remaining = 3;
        };
        const auto reading = std::make_shared<PendingReading>();
        const auto finish = [this, reading](const QVariant &confirmedDocked) {
            // Use a fresh fallback after DBus replies, never a cached percentage.
            const auto fallback = MarkerBattery::readPowerSupplies(QStringLiteral("/sys/class/power_supply"));
            const bool sameDockState = reading->docked.metaType().id() == QMetaType::Bool
                && confirmedDocked.metaType().id() == QMetaType::Bool
                && reading->docked.toBool() == confirmedDocked.toBool();
            applyMarkerReading(MarkerBattery::fromService(reading->capacity,
                sameDockState ? confirmedDocked : QVariant{}, reading->charging, fallback));
            m_markerRefreshPending = false;
            publishChanges();
        };
        const auto received = [this, reading, finish] {
            if (--reading->remaining != 0)
                return;
            // Separate Get calls must not combine a pre-undocking percentage
            // with a later dock status. An undocked reading ignores capacity.
            if (reading->docked.metaType().id() == QMetaType::Bool && reading->docked.toBool())
                readMarkerProperty(this, QStringLiteral("Docked"), finish);
            else
                finish(reading->docked);
        };
        readMarkerProperty(this, QStringLiteral("BatteryCapacity"), [reading, received](QVariant value) {
            reading->capacity = std::move(value);
            received();
        });
        readMarkerProperty(this, QStringLiteral("Docked"), [reading, received](QVariant value) {
            reading->docked = std::move(value);
            received();
        });
        readMarkerProperty(this, QStringLiteral("IsCharging"), [reading, received](QVariant value) {
            reading->charging = std::move(value);
            received();
        });
        return;
    }
#endif
    applyMarkerReading({});
    m_markerStatus = tr("Disponibile sul Paper Pro");
    publishChanges();
}

bool DeviceInfo::setFrontlightLevel(int level)
{
#ifdef Q_OS_LINUX
    if (m_paperPro && level >= 0 && size_t(level) < brightnessPresets.size()) {
        const LightState before = readLight();
        if (before.controllable) {
            LightState target = before;
            target.power = level == 0 ? lightOff : lightOn;
            if (level > 0)
                target.brightness = brightnessPresets[size_t(level)];

            if (!m_writtenLight || !before.matches(*m_writtenLight))
                m_originalLight = before;

            const bool written = applyLight(before, target);
            const LightState current = readLight();
            if (current.matches(target) || current.matches(before))
                m_writtenLight = current;

            const bool success = written && current.matches(target);
            m_error = success ? QString{} : tr("Impossibile applicare il livello della luce frontale.");
            refresh();
            return success;
        }
    }
#else
    Q_UNUSED(level);
#endif

    m_error = tr("Controllo della luce non disponibile.");
    refresh();
    return false;
}

#ifdef Q_OS_LINUX
bool DeviceInfo::LightState::matches(const LightState &other) const
{
    return brightness == other.brightness && power == other.power;
}

DeviceInfo::LightState DeviceInfo::readLight()
{
    const int brightness = readInteger(lightBrightness);
    const int power = readInteger(lightPower);
    const bool controllable = brightness >= 0 && brightness <= expectedMaximum
        && (power == lightOn || power == lightOff)
        && readInteger(lightDirectory + QStringLiteral("max_brightness")) == expectedMaximum
        && readText(lightDirectory + QStringLiteral("linear_mapping")) == QStringLiteral("no")
        && QFileInfo(lightBrightness).isWritable() && QFileInfo(lightPower).isWritable();
    return {brightness, power, controllable};
}

bool DeviceInfo::applyLight(const LightState &before, const LightState &after)
{
    // On exit, blank first if the previous session left a stored intensity
    // with the light off. Restoring that intensity must not flash the panel.
    if (before.power != lightOff && after.power == lightOff) {
        if (!writeInteger(lightPower, after.power))
            return false;
        if (before.brightness == after.brightness || writeInteger(lightBrightness, after.brightness))
            return true;

        writeInteger(lightPower, before.power);
        return false;
    }

    if (before.brightness != after.brightness && !writeInteger(lightBrightness, after.brightness))
        return false;
    if (before.power == after.power || writeInteger(lightPower, after.power))
        return true;

    if (before.brightness != after.brightness)
        writeInteger(lightBrightness, before.brightness);
    return false;
}
#endif
