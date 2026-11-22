#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class PinLock;
class DeviceInfo;

// Application-owned screen sleep. Ink must be disabled while sleeping, even
// when the user has not configured a PIN. It does not claim kernel suspend.
class PaperProStandby : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(bool sleeping READ sleeping NOTIFY sleepingChanged)
    Q_PROPERTY(bool drawing READ drawing WRITE setDrawing NOTIFY drawingChanged)
    Q_PROPERTY(bool allowed MEMBER m_allowed)
    Q_PROPERTY(int idleMinutes READ idleMinutes WRITE setIdleMinutes NOTIFY idleMinutesChanged)
    Q_PROPERTY(bool keepAwakeWhileCharging READ keepAwakeWhileCharging WRITE setKeepAwakeWhileCharging NOTIFY keepAwakeWhileChargingChanged)
public:
    explicit PaperProStandby(bool enabled, PinLock *pin, DeviceInfo *device, QObject *parent = nullptr);
    bool sleeping() const { return m_sleeping; }
    bool drawing() const { return m_drawing; }
    void setDrawing(bool drawing);
    int idleMinutes() const { return m_idleMinutes; }
    void setIdleMinutes(int minutes);
    bool keepAwakeWhileCharging() const { return m_keepAwakeWhileCharging; }
    void setKeepAwakeWhileCharging(bool enabled);
    Q_INVOKABLE void recordActivity();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void wake();
signals:
    void sleepingChanged();
    void drawingChanged();
    void idleMinutesChanged();
    void keepAwakeWhileChargingChanged();
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    const bool m_enabled;
    PinLock *m_pin;
    DeviceInfo *m_device;
    QTimer m_idle;
    bool m_sleeping = false;
    bool m_drawing = false;
    bool m_allowed = true;
    bool m_keepAwakeWhileCharging = false;
    int m_idleMinutes = 20;
    int m_savedLight = -1;
};
