#pragma once

#include <QFileSystemWatcher>
#include <QObject>

class ExternalKeyboard final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit ExternalKeyboard(bool tablet, QObject *parent = nullptr);

    bool connected() const { return m_connected; }

signals:
    void connectedChanged();

private:
#ifdef Q_OS_LINUX
    void updateConnected();

    QFileSystemWatcher m_devices;
#endif
    bool m_connected = false;
};
