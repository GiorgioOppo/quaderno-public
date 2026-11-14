#pragma once

#include "notebook.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QTemporaryDir;

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

public:
    explicit AppleShare(bool tablet, QObject *parent = nullptr);
    ~AppleShare() override;

    bool available() const;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }

    Q_INVOKABLE void sharePdf(Notebook *notebook);
    Q_INVOKABLE void savePdf(Notebook *notebook);
    Q_INVOKABLE void importNotebookFile(Notebook *notebook);

signals:
    void changed();

private:
    void setState(bool busy, const QString &status = {});
    QString sharingPath(const QString &fileName);

    const bool m_tablet;
    bool m_busy = false;
    QString m_status;
    std::unique_ptr<QTemporaryDir> m_sharedFiles;
};
