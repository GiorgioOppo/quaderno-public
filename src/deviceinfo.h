#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace MarkerBattery { struct Reading; }

class DeviceInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Il dispositivo è fornito dall’applicazione")

    Q_PROPERTY(bool tabletMode READ tabletMode CONSTANT)
    Q_PROPERTY(bool paperPro READ paperPro NOTIFY changed)
    Q_PROPERTY(QString model READ model NOTIFY changed)
    Q_PROPERTY(QString osVersion READ osVersion NOTIFY changed)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY changed)
    Q_PROPERTY(bool markerAvailable READ markerAvailable NOTIFY changed)
    Q_PROPERTY(bool markerPresent READ markerPresent NOTIFY changed)
    Q_PROPERTY(int markerBatteryPercent READ markerBatteryPercent NOTIFY changed)
    Q_PROPERTY(bool markerCharging READ markerCharging NOTIFY changed)
    Q_PROPERTY(QString markerStatus READ markerStatus NOTIFY changed)
    Q_PROPERTY(bool bluetoothAvailable READ bluetoothAvailable NOTIFY changed)
    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY changed)
    Q_PROPERTY(bool frontlightAvailable READ frontlightAvailable NOTIFY changed)
    Q_PROPERTY(int frontlightLevel READ frontlightLevel NOTIFY changed)
    Q_PROPERTY(QString frontlightStatus READ frontlightStatus NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    explicit DeviceInfo(bool tabletMode, QObject *parent = nullptr);
    ~DeviceInfo() override;

    bool tabletMode() const { return m_tabletMode; }
    bool paperPro() const { return m_paperPro; }
    QString model() const { return m_model; }
    QString osVersion() const { return m_osVersion; }
    int batteryPercent() const { return m_batteryPercent; }
    bool charging() const { return m_charging; }
    QString batteryStatus() const { return m_batteryStatus; }
    bool markerAvailable() const { return m_markerAvailable; }
    bool markerPresent() const { return m_markerPresent; }
    int markerBatteryPercent() const { return m_markerBatteryPercent; }
    bool markerCharging() const { return m_markerCharging; }
    QString markerStatus() const { return m_markerStatus; }
    bool bluetoothAvailable() const { return m_bluetoothAvailable; }
    QString bluetoothStatus() const { return m_bluetoothStatus; }
    bool frontlightAvailable() const { return m_frontlightAvailable; }
    int frontlightLevel() const { return m_frontlightLevel; }
    QString frontlightStatus() const { return m_frontlightStatus; }
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshMarker();
    Q_INVOKABLE bool setFrontlightLevel(int level);

signals:
    void changed();

private:
    void publishChanges();
    void applyMarkerReading(const MarkerBattery::Reading &reading);
#ifdef Q_OS_LINUX
    struct LightState {
        int brightness = -1;
        int power = -1;
        bool controllable = false;

        bool matches(const LightState &other) const;
    };

    static LightState readLight();
    static bool applyLight(const LightState &before, const LightState &after);

    std::optional<LightState> m_originalLight;
    std::optional<LightState> m_writtenLight;
#endif

    const bool m_tabletMode;
    bool m_paperPro = false;
    QString m_model;
    QString m_osVersion;
    int m_batteryPercent = -1;
    bool m_charging = false;
    QString m_batteryStatus;
    bool m_markerAvailable = false;
    bool m_markerPresent = false;
    int m_markerBatteryPercent = -1;
    bool m_markerCharging = false;
    QString m_markerStatus;
    bool m_markerRefreshPending = false;
    bool m_bluetoothAvailable = false;
    QString m_bluetoothStatus;
    bool m_frontlightAvailable = false;
    int m_frontlightLevel = -1;
    QString m_frontlightStatus;
    QString m_error;
    QVariantList m_publishedState;
};
