#include "paperprostandby.h"
#include "pinlock.h"
#include "deviceinfo.h"
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>

PaperProStandby::PaperProStandby(bool enabled, PinLock *pin, DeviceInfo *device, QObject *parent)
    : QObject(parent), m_enabled(enabled), m_pin(pin), m_device(device)
{
    m_idle.setSingleShot(true);
    connect(&m_idle, &QTimer::timeout, this, [this] {
        // Full-page mode and the PIN screen pause UI status polling. Read power
        // again so unplugging the cable cannot leave screen sleep deferred forever.
        if (m_keepAwakeWhileCharging && m_device)
            m_device->refresh();
        if (m_drawing || !m_allowed || (m_keepAwakeWhileCharging && m_device && m_device->charging())) {
            // Defer only screen sleep; do not report activity or postpone PIN locking.
            m_idle.start(60000);
            return;
        }
        suspend();
    });
    if (m_enabled && QCoreApplication::instance())
        QCoreApplication::instance()->installEventFilter(this);
    recordActivity();
}

void PaperProStandby::setDrawing(bool drawing)
{
    if (m_drawing == drawing)
        return;
    m_drawing = drawing;
    recordActivity();
    emit drawingChanged();
}

void PaperProStandby::setIdleMinutes(int minutes)
{
    if (minutes < 1 || minutes > 120 || minutes == m_idleMinutes)
        return;
    m_idleMinutes = minutes;
    recordActivity();
    emit idleMinutesChanged();
}

void PaperProStandby::recordActivity()
{
    if (!m_enabled || m_sleeping)
        return;
    // Raw evdev Marker events do not pass through Qt's event filter. InkItem
    // explicitly calls this for drawing and selection, as well as normal UI.
    m_idle.start(m_idleMinutes * 60000);
    if (m_pin)
        m_pin->recordActivity();
}

void PaperProStandby::setKeepAwakeWhileCharging(bool enabled)
{
    if (enabled == m_keepAwakeWhileCharging) return;
    m_keepAwakeWhileCharging = enabled;
    if (m_enabled && !m_sleeping)
        m_idle.start(m_idleMinutes * 60000);
    emit keepAwakeWhileChargingChanged();
}

void PaperProStandby::suspend()
{
    if (!m_enabled || m_sleeping || m_drawing || !m_allowed)
        return;
    m_sleeping = true;
    m_idle.stop();
    // Notify synchronously before touching the light or lock so native ink
    // stops accepting points immediately, including installations without PIN.
    emit sleepingChanged();
    if (m_pin)
        m_pin->lock();
    if (m_device && m_device->frontlightAvailable()) {
        m_savedLight = m_device->frontlightLevel();
        m_device->setFrontlightLevel(0);
    }
}

void PaperProStandby::wake()
{
    if (!m_sleeping)
        return;
    if (m_device && m_savedLight >= 0)
        m_device->setFrontlightLevel(m_savedLight);
    m_savedLight = -1;
    m_sleeping = false;
    emit sleepingChanged();
    recordActivity();
}

bool PaperProStandby::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_PowerOff || key->key() == Qt::Key_Sleep || key->key() == Qt::Key_WakeUp) {
            if (!key->isAutoRepeat()) {
                if (m_sleeping) wake(); else suspend();
            }
            return true;
        }
    }
    switch (event->type()) {
    case QEvent::KeyPress: case QEvent::MouseButtonPress: case QEvent::MouseButtonRelease:
    case QEvent::Wheel: case QEvent::TouchBegin: case QEvent::TouchUpdate:
    case QEvent::TabletPress: case QEvent::TabletMove: case QEvent::TabletRelease:
        recordActivity();
        break;
    default: break;
    }
    return QObject::eventFilter(object, event);
}
