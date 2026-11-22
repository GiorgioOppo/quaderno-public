#pragma once

#include "cloudstore.h"
#include "notebook.h"
#include "smtpclient.h"

#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

class QTemporaryDir;

class SmtpMail : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)

public:
    explicit SmtpMail(const QString &settingsDirectory = {}, QObject *parent = nullptr);
    ~SmtpMail() override;

    bool configured() const;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QVariantMap settings() const;
    void setLocked(bool locked);

    Q_INVOKABLE bool saveSettings(const QVariantMap &settings, const QString &password);
    Q_INVOKABLE bool clearSettings();
    Q_INVOKABLE void sendPdf(Notebook *notebook, const QVariantMap &option, const QString &recipient,
                             const QString &subject, const QString &message);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void finished(bool success);

private:
    void setState(bool busy, const QString &status);
    void complete(bool success, const QString &status);
    CloudStore m_store;
    SmtpClient m_client;
    SmtpClient::Configuration m_configuration;
    std::unique_ptr<QTemporaryDir> m_export;
    bool m_locked = false;
    bool m_busy = false;
    QString m_status;
    quint64 m_operation = 0;
};
