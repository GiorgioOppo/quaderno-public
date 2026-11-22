#pragma once

#include "notebook.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>


#ifdef Q_OS_MACOS
int runSshAskpass(const QString &prompt);
#endif

class AppleShare : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList deliveries READ deliveries NOTIFY changed)

public:
    explicit AppleShare(bool tablet, QObject *parent = nullptr, const QString &deliveryDirectory = {});
    ~AppleShare() override;

    bool available() const;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QVariantList deliveries() const { return m_deliveries; }

    Q_INVOKABLE void sharePdf(Notebook *notebook);
    Q_INVOKABLE void savePdf(Notebook *notebook);
    Q_INVOKABLE void importNotebookFile(Notebook *notebook);
    Q_INVOKABLE void prepareDelivery(Notebook *notebook, const QString &destination);
    Q_INVOKABLE bool confirmDelivery(const QString &id);
    Q_INVOKABLE void retryDelivery(const QString &id);
    Q_INVOKABLE bool cancelDelivery(const QString &id);
    Q_INVOKABLE bool archiveCompletedDeliveries();
    Q_INVOKABLE QUrl deliveryFileUrl(const QString &id) const;
    Q_INVOKABLE bool revealDelivery(const QString &id);

signals:
    void changed();

private:
    void setState(bool busy, const QString &status = {});
    bool saveDeliveries(const QVariantList &deliveries);
    bool changeDelivery(const QString &id, const QString &state, const QString &error = {});
    void loadDeliveries();

    const bool m_tablet;
    bool m_busy = false;
    QString m_status;
    QString m_deliveryDirectory;
    QVariantList m_deliveries;
    QByteArray m_deliveryRevision;
    bool m_deliveryLoadFailed = false;
};
