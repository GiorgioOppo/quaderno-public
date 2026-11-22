#pragma once

#include <QObject>
#include <QPointer>
#include <QDateTime>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "notebook.h"

class LibraryMaintenance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Notebook *notebook READ notebook WRITE setNotebook NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool allowed READ allowed WRITE setAllowed NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantMap storage READ storage NOTIFY changed)
    Q_PROPERTY(QVariantList backups READ backups NOTIFY changed)
    Q_PROPERTY(QString lastBackupPath READ lastBackupPath NOTIFY changed)
    Q_PROPERTY(QString backupSchedule READ backupSchedule WRITE setBackupSchedule NOTIFY changed)
    Q_PROPERTY(int backupRetention READ backupRetention WRITE setBackupRetention NOTIFY changed)
    Q_PROPERTY(int cacheLimitMiB READ cacheLimitMiB WRITE setCacheLimitMiB NOTIFY changed)
public:
    explicit LibraryMaintenance(QObject *parent = nullptr);
    Notebook *notebook() const { return m_notebook; }
    void setNotebook(Notebook *notebook);
    bool busy() const { return m_busy; }
    bool allowed() const { return m_allowed; }
    void setAllowed(bool value);
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    QVariantMap storage() const { return m_storage; }
    QVariantList backups() const { return m_backups; }
    QString lastBackupPath() const { return m_lastBackupPath; }
    QString backupSchedule() const { return m_backupSchedule; }
    void setBackupSchedule(const QString &value);
    int backupRetention() const { return m_backupRetention; }
    void setBackupRetention(int value);
    int cacheLimitMiB() const { return m_cacheLimitMiB; }
    void setCacheLimitMiB(int value);
    Q_INVOKABLE bool backup();
    Q_INVOKABLE bool clearCache();
    Q_INVOKABLE bool refreshStatus();
    // Checks elapsed time only. The timer never serializes a live Notebook.
    Q_INVOKABLE void checkSchedule();
signals:
    void changed();
    void finished(bool success);
private:
    bool start(const QString &operation);
    QPointer<Notebook> m_notebook;
    bool m_busy = false;
    bool m_allowed = true;
    bool m_retentionConfigured = false;
    QString m_status, m_error, m_lastBackupPath;
    QVariantMap m_storage;
    QVariantList m_backups;
    QString m_backupSchedule = QStringLiteral("manual");
    int m_backupRetention = 5;
    int m_cacheLimitMiB = 64;
    QDateTime m_lastBackupAt, m_nextAutoAttempt;
    QTimer m_scheduleTimer;
};
