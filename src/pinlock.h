#pragma once

#include <QByteArray>
#include <QDeadlineTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QLockFile;
class QEvent;

class PinLock : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Il blocco con PIN è fornito dall’applicazione")

    Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(int secondsRemaining READ secondsRemaining NOTIFY secondsRemainingChanged)
    Q_PROPERTY(int autoLockMinutes READ autoLockMinutes NOTIFY autoLockMinutesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit PinLock(const QString &filePath, QObject *parent = nullptr);

    bool configured() const;
    bool locked() const { return m_locked; }
    bool available() const { return m_available; }
    int secondsRemaining() const { return m_secondsRemaining; }
    int autoLockMinutes() const { return m_record.autoLockMinutes; }
    QString error() const { return m_error; }

    Q_INVOKABLE bool setPin(const QString &pin);
    Q_INVOKABLE bool changePin(const QString &currentPin, const QString &newPin);
    Q_INVOKABLE bool removePin(const QString &currentPin);
    Q_INVOKABLE bool unlock(const QString &pin);
    Q_INVOKABLE void lock();
    Q_INVOKABLE void recordActivity();
    Q_INVOKABLE bool setAutoLockMinutes(int minutes);

signals:
    void configuredChanged();
    void lockedChanged();
    void availableChanged();
    void secondsRemainingChanged();
    void autoLockMinutesChanged();
    void errorChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Record {
        QByteArray salt;
        QByteArray digest;
        int failedAttempts = 0;
        qint64 retryAfter = 0;
        int autoLockMinutes = 5;
    };

    bool readRecord(Record &record) const;
    bool beginUpdate(QLockFile &guard);
    bool saveRecord(const Record &record);
    bool verifyPin(const QString &pin);
    bool storeNewPin(const QString &pin);
    bool fail(const QString &message);
    void markUnavailable(const QString &message);
    void updateRecord(const Record &record);
    void updateCountdown();
    void setLocked(bool locked);
    void setError(const QString &error);

    const QString m_filePath;
    Record m_record;
    QDeadlineTimer m_retryDeadline;
    QTimer m_countdown;
    QTimer m_idleLock;
    QString m_error;
    int m_secondsRemaining = 0;
    bool m_locked = false;
    bool m_available = true;
};
