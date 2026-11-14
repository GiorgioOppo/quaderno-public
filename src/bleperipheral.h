#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

class BleGattObjects;
class QDBusConnection;
class QDBusMessage;

class BlePeripheral : public QObject
{
    Q_OBJECT

public:
    explicit BlePeripheral(QObject *parent = nullptr);
    ~BlePeripheral() override;

    void refresh();
    void start();
    void stop();
    bool send(const QByteArray &frame);

    bool ready() const { return m_ready; }
    bool activationNeeded() const { return m_activationNeeded; }
    bool running() const { return m_state == State::Running; }
    bool busy() const { return m_state == State::Probing || m_state == State::Registering; }
    QString status() const { return m_status; }
    qsizetype maximumChunkSize() const { return m_chunkSize; }

signals:
    void changed();
    void frameReceived(const QByteArray &frame);
    void peerChanged(bool subscribed);

private slots:
    void bluezOwnerChanged(const QString &name, const QString &previous, const QString &current);
    void bluezPropertiesChanged(const QString &interface, const QVariantMap &properties,
                                const QStringList &invalidated, const QDBusMessage &message);

private:
    friend class BleGattObjects;
    enum class State { Idle, Probing, Registering, Running };

    void probe(bool launch);
    void registerApplication();
    void registerAdvertisement();
    void clearPeer();
    void closeBus();
    void finish(const QString &status);

    State m_state = State::Idle;
    bool m_ready = false;
    bool m_activationNeeded = false;
    bool m_launchAfterProbe = false;
    bool m_notifying = false;
    quint64 m_generation = 0;
    qsizetype m_chunkSize = 20;
    QString m_status;
    QString m_bluezOwner;
    QString m_adapterPath;
    QString m_peerPath;
    std::unique_ptr<QDBusConnection> m_bus;
    std::unique_ptr<BleGattObjects> m_objects;
};
