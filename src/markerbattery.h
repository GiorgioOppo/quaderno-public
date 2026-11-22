#pragma once

#include <QString>
#include <QVariant>

namespace MarkerBattery {

enum class State { Unavailable, Unknown, Absent, Ready, Charging, Full, Discharging, NotCharging };

struct Reading {
    bool available = false;
    bool present = false;
    int percent = -1;
    bool charging = false;
    State state = State::Unavailable;
};

// Only the two Paper Pro Marker drivers are considered. The root is injectable
// so malformed, missing and stale sysfs readings can be tested without hardware.
Reading readPowerSupplies(const QString &root);

// The firmware's public percentage is BatteryCapacity, not RealBatteryCapacity.
// Undocked daemon values can be cached: only a present sysfs source may replace
// them. QVariant types are checked rather than coercing invalid values to zero.
Reading fromService(const QVariant &capacity, const QVariant &docked,
                    const QVariant &charging, const Reading &fallback);

}
