#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QVariantList>

#include <functional>
#include <memory>

using BluetoothInterfaces = QMap<QString, QVariantMap>;
using BluetoothObjects = QMap<QDBusObjectPath, BluetoothInterfaces>;

class BluetoothAgent;

class BluetoothSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool canPowerOn READ canPowerOn NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString promptType READ promptType NOTIFY changed)
    Q_PROPERTY(QString promptCode READ promptCode NOTIFY changed)
    Q_PROPERTY(QString promptName READ promptName NOTIFY changed)
    Q_PROPERTY(int promptEntered READ promptEntered NOTIFY changed)

public:
    explicit BluetoothSettings(bool deviceMode, bool canActivate, QObject *parent = nullptr);
    ~BluetoothSettings() override;

    bool available() const { return m_deviceMode; }
    bool ready() const { return !m_adapterPath.isEmpty(); }
    bool canPowerOn() const { return ready() || m_canActivate; }
    bool powered() const { return m_powered; }
    bool scanning() const { return m_scanRequested; }
    bool busy() const { return m_probing || m_operation != Operation::None; }
    QString status() const;
    QVariantList devices() const { return m_devices; }
    QString promptType() const { return m_promptType; }
    QString promptCode() const { return m_promptCode; }
    QString promptName() const { return m_promptName; }
    int promptEntered() const { return m_promptEntered; }

    void setLocked(bool locked);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setPowered(bool powered);
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void pair(const QString &id);
    Q_INVOKABLE void connectDevice(const QString &id);
    Q_INVOKABLE void disconnectDevice(const QString &id);
    Q_INVOKABLE void forget(const QString &id);
    Q_INVOKABLE void answerPrompt(const QString &value);
    Q_INVOKABLE void cancelPairing();

signals:
    void changed();
    void devicesChanged();

private slots:
    void interfacesAdded(const QDBusObjectPath &path, const BluetoothInterfaces &interfaces);
    void interfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void propertiesChanged(const QString &interface, const QVariantMap &properties,
                           const QStringList &invalidated, const QDBusMessage &message);

private:
    friend class BluetoothAgent;
    enum class Operation { None, Power, Pair, Connect, Disconnect, Forget };
    using ReplyHandler = std::function<void(const QDBusMessage &)>;

    void call(const QString &path, const QString &interface, const QString &method,
              const QVariantList &arguments, ReplyHandler handler = {}, int timeout = 5000);
    void updateAdapter();
    void updateDevices();
    void setStatus(const QString &status);
    void setPoweredOnAdapter(bool powered);
    void clearService();
    bool usableDevice(const QString &id) const;
    void startPair(const QString &id);
    void connectPaired(const QString &id);
    void deviceAction(const QString &id, Operation operation, const QString &method,
                      const QString &success, const QString &failure, const QString &alreadyDone);
    void finish(const QString &status);
    void setPrompt(const QString &type, const QString &code = {}, int entered = 0);
    void clearPrompt(const QString &error = {});
    bool agentMessage(const QDBusMessage &message);
    void updateDiscovery();

    const bool m_deviceMode;
    const bool m_canActivate;
    std::unique_ptr<QDBusConnection> m_bus;
    std::unique_ptr<BluetoothAgent> m_agent;
    QString m_owner;
    QString m_adapterPath;
    QMap<QString, QVariantMap> m_adapters;
    QHash<QString, QVariantMap> m_knownDevices;
    QSet<QString> m_seen;
    QVariantList m_devices;
    QString m_status;
    QTimer m_scanDeadline;
    QTimer m_activationDeadline;
    QProcess m_activation;
    quint64 m_generation = 0;
    bool m_locked = true;
    bool m_probing = false;
    bool m_powered = false;
    bool m_scanRequested = false;
    bool m_scanOwned = false;
    bool m_scanPending = false;
    bool m_agentRegistered = false;
    Operation m_operation = Operation::None;
    QString m_operationDevice;
    bool m_pairStarted = false;
    bool m_pairCancelled = false;
    QDBusMessage m_promptRequest;
    QString m_promptType;
    QString m_promptCode;
    QString m_promptName;
    int m_promptEntered = 0;
};
