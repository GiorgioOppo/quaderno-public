#include "markerbattery.h"

#include <QDir>
#include <QFile>

namespace MarkerBattery {
namespace {
QString attribute(const QDir &directory, const QString &name)
{
    QFile file(directory.filePath(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    constexpr int maximum = 96;
    const QByteArray value = file.read(maximum + 1);
    if (value.size() > maximum || value.contains('\0'))
        return {};
    return QString::fromLatin1(value).trimmed();
}

int bit(const QString &value)
{
    if (value == QStringLiteral("0"))
        return 0;
    if (value == QStringLiteral("1"))
        return 1;
    return -1;
}

int percentage(const QString &value)
{
    if (value.isEmpty() || value.size() > 3)
        return -1;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return -1;
    }
    bool ok = false;
    const int result = value.toInt(&ok);
    return ok && result >= 0 && result <= 100 ? result : -1;
}

struct Supply {
    Reading reading;
    int present = -1;
    bool current = false;
};

Supply readSupply(const QDir &directory, bool nfc)
{
    Supply result;
    if (attribute(directory, QStringLiteral("type")) != QStringLiteral("Wireless"))
        return result;

    result.reading.available = true;
    result.present = bit(attribute(directory, QStringLiteral("present")));
    result.reading.present = result.present == 1;
    result.reading.state = result.present == 0 ? State::Absent : State::Unknown;
    if (result.present != 1)
        return result;

    // NFC capacity can remain stored after a charging link ends. Elan presence
    // already identifies its live input link; NFC additionally requires online.
    if (nfc && bit(attribute(directory, QStringLiteral("online"))) != 1)
        return result;

    result.current = true;
    const QString status = attribute(directory, QStringLiteral("status"));
    const QString capacityLevel = attribute(directory, QStringLiteral("capacity_level"));
    result.reading.percent = nfc && capacityLevel == QStringLiteral("Unknown")
        ? -1 : percentage(attribute(directory, QStringLiteral("capacity")));
    result.reading.charging = status == QStringLiteral("Charging");
    if (result.reading.charging)
        result.reading.state = State::Charging;
    else if (status == QStringLiteral("Full"))
        result.reading.state = State::Full;
    else if (status == QStringLiteral("Discharging"))
        result.reading.state = State::Discharging;
    else if (status == QStringLiteral("Not charging"))
        result.reading.state = State::NotCharging;
    else
        result.reading.state = State::Unknown;

    // Do not publish a percentage if the link changed during these reads.
    if (bit(attribute(directory, QStringLiteral("present"))) != 1
        || (nfc && bit(attribute(directory, QStringLiteral("online"))) != 1)) {
        result.current = false;
        result.reading.present = false;
        result.reading.percent = -1;
        result.reading.charging = false;
        result.reading.state = State::Unknown;
    }
    return result;
}
}

Reading readPowerSupplies(const QString &root)
{
    const QDir supplies(root);
    const Supply nfc = readSupply(QDir(supplies.filePath(QStringLiteral("nfc-marker-battery"))), true);
    const Supply elan = readSupply(QDir(supplies.filePath(QStringLiteral("elants-marker-battery"))), false);
    if (nfc.current)
        return nfc.reading;
    if (elan.current)
        return elan.reading;

    Reading result;
    result.available = nfc.reading.available || elan.reading.available;
    result.present = nfc.reading.present || elan.reading.present;
    const bool absent = (!nfc.reading.available || nfc.present == 0)
        && (!elan.reading.available || elan.present == 0);
    result.state = !result.available ? State::Unavailable : absent ? State::Absent : State::Unknown;
    return result;
}

Reading fromService(const QVariant &capacity, const QVariant &docked,
                    const QVariant &charging, const Reading &fallback)
{
    if (docked.metaType().id() != QMetaType::Bool)
        return fallback;
    if (!docked.toBool()) {
        if (fallback.present)
            return fallback;
        return {true, false, -1, false, State::Absent};
    }

    Reading result{true, true, -1, false, State::Unknown};
    if (capacity.metaType().id() == QMetaType::Int) {
        const int percent = capacity.toInt();
        if (percent >= 0 && percent <= 100)
            result.percent = percent;
    }
    if (charging.metaType().id() == QMetaType::Bool) {
        result.charging = charging.toBool();
        result.state = result.charging ? State::Charging
            : result.percent == 100 ? State::Full : State::Ready;
    }
    return result;
}

}
