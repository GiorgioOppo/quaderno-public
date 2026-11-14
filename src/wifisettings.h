#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QDBusMessage;

class WifiSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Il gestore Wi-Fi è fornito dall'applicazione")

    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString ssid READ ssid NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)

public:
    explicit WifiSettings(bool deviceMode, QObject *parent = nullptr);
    ~WifiSettings() override;

    bool available() const { return !m_interfacePath.isEmpty(); }
    bool busy() const { return m_probing || m_operation != Operation::Idle; }
    bool connected() const { return m_state == QStringLiteral("completed"); }
    QString ssid() const { return m_ssid; }
    QString status() const;
    QString error() const { return m_error; }
    QVariantList networks() const { return m_networks; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void scan();
    Q_INVOKABLE void connectNetwork(const QString &id, const QString &password);
    Q_INVOKABLE void disconnectNetwork();

signals:
    void changed();
    void networksChanged();

private slots:
    void interfaceChanged(const QString &interface, const QVariantMap &properties,
                          const QStringList &invalidated);
    void scanDone(bool success);

private:
    enum class Operation { Idle, Scan, Connect, Disconnect };
    enum class Security { Open, Personal, Unsupported };

    struct AccessPoint {
        QByteArray ssid;
        Security security = Security::Unsupported;
        int signal = -127;
    };

    using ReplyHandler = std::function<void(const QDBusMessage &)>;
    void call(const QString &path, const QString &interface, const QString &method,
              const QVariantList &arguments, ReplyHandler handler);
    void callInterface(const QString &method, const QVariantList &arguments, ReplyHandler handler);
    void readStatus();
    void readAccessPoint();
    void publishScan();
    void removeNetwork(const QString &path);
    void abortConnection(const QString &message);
    void finish(const QString &error = {});
    void clearInterface();
    void setError(const QString &error);

    bool m_deviceMode;
    bool m_probing = false;
    bool m_readingStatus = false;
    bool m_statusPending = false;
    bool m_scanReading = false;
    bool m_connectArmed = false;
    bool m_reconnectOnFailure = false;
    quint64 m_generation = 0;
    quint64 m_scanGeneration = 0;
    QString m_interfacePath;
    QString m_state;
    QString m_currentBss;
    QString m_ssid;
    QString m_error;
    QString m_trialNetwork;
    QString m_addedNetwork;
    Operation m_operation = Operation::Idle;
    QVariantList m_networks;
    QHash<QString, AccessPoint> m_accessPoints;
    QHash<QString, AccessPoint> m_scanResults;
    QStringList m_scanPaths;
    QTimer m_deadline;
    QTimer m_connectPoll;
};
