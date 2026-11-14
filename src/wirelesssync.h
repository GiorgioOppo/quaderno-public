#pragma once

#include "blepipe.h"
#include "blesync.h"
#include "syncchannel.h"
#include "wifisynclink.h"

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#ifdef Q_OS_MACOS
#include "blecentral.h"
#elif defined(Q_OS_LINUX)
#include "bleperipheral.h"
#endif

class WirelessSync final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(BleSync *transfer READ transfer CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool tablet READ tablet CONSTANT)
    Q_PROPERTY(bool paired READ paired NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool bluetoothActive READ bluetoothActive NOTIFY changed)
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QString address READ address NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY changed)
    Q_PROPERTY(bool activatingBluetooth READ activatingBluetooth NOTIFY changed)

public:
    WirelessSync(Notebook *notebook, const QString &pairingPath, bool tablet, bool canActivateBluetooth = false,
                 QObject *parent = nullptr);
    ~WirelessSync() override;
    BleSync *transfer() { return &m_transfer; }
    bool available() const;
    bool tablet() const { return m_tablet; }
    bool paired() const { return !m_secret.isEmpty(); }
    bool active() const { return m_link != Link::None; }
    bool bluetoothActive() const { return m_link == Link::Bluetooth; }
    QString code() const;
    QString address() const;
    QString status() const { return m_status; }
    QString bluetoothStatus() const;
    bool activatingBluetooth() const { return m_activateBluetooth.state() != QProcess::NotRunning; }
    void setLocked(bool locked);

    Q_INVOKABLE void preparePairing();
    Q_INVOKABLE void refreshBluetooth();
    Q_INVOKABLE void startBluetooth(const QString &code);
    Q_INVOKABLE void startWifi(const QString &address, const QString &code);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void forget();

signals:
    void changed();

private:
    enum class Link { None, Bluetooth, Wifi };
    bool prepareConnection(Link link, const QString &code);
    void connectBluetooth(const QString &code, bool allowActivation);
    void activateBluetooth();
    void startChannel();
    void acceptPeer();
    void linkEnded(const QString &message);
    void report(const QString &message);
    bool storePairing(const QByteArray &secret);
    void loadPairing();
    void updatePeer();
    void resetLink();
    void scheduleReconnect();
    void saveConnection();

    const QString m_pairingPath;
    const bool m_tablet;
    const bool m_canActivateBluetooth;
    BleSync m_transfer;
    SyncChannel m_channel;
    BlePipe m_pipe;
    WifiSyncLink m_wifi;
#ifdef Q_OS_MACOS
    BleCentral m_bluetooth;
#elif defined(Q_OS_LINUX)
    BlePeripheral m_bluetooth;
#endif
    Link m_link = Link::None;
    Link m_preferredLink = Link::None;
    QString m_preferredAddress;
    QTimer m_reconnect;
    QProcess m_activateBluetooth;
    QTimer m_activationDeadline;
    bool m_paused = false;
    bool m_locked = true;
    bool m_tryBluetoothActivation = false;
    QByteArray m_secret;
    QByteArray m_pendingSecret;
    QString m_status;
    QString m_bluetoothStatus;
};
