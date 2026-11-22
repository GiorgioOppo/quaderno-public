#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class PaperProSshAccess final : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantList transports READ transports NOTIFY changed)

public:
    explicit PaperProSshAccess(bool verifiedDevice);
    ~PaperProSshAccess() override;

    bool available() const { return m_available; }
    bool busy() const { return m_step != Step::Idle; }
    QString message() const { return m_message; }
    QVariantList transports() const;
    void setLocked(bool locked) { m_locked = locked; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(const QString &key, bool enabled);

signals:
    void changed();

private:
    friend class PaperProSshAccessTests;
    enum class Step { Idle, Probe, WiFi, MaskUsb, UnmaskUsb, StartUsb, Verify };
    enum class Transport { None, WiFi, Usb };
    enum class Socket { Unrecognized, Inactive, Listening, RuntimeMasked };
    enum class State { Unknown, Unavailable, Disabled, Enabled, Partial };
    struct Policy {
        State wifi = State::Unknown;
        State usb = State::Unknown;
    };
    struct Command {
        QString program;
        QStringList arguments;
    };

    void run(Step step);
    void completed(bool success);
    void finish(const QString &message = {});
    static Command command(Step step, bool enabled);
    static Policy inspect(const QByteArray &systemctlOutput, bool wifiTool, bool wifiMarker,
                          bool usb0Masked, bool usb1Masked);

    const bool m_available;
    bool m_locked = true;
    bool m_targetEnabled = false;
    bool m_timedOut = false;
    bool m_writeSucceeded = false;
    Step m_step = Step::Idle;
    Transport m_target = Transport::None;
    Policy m_policy;
    QString m_message;
    QProcess m_process;
    QTimer m_deadline;
};
