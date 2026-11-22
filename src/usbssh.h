#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class Notebook;
Q_MOC_INCLUDE("notebook.h")

#ifdef Q_OS_MACOS
namespace UsbConnection {
QStringList arguments(const QString &remote);
QProcessEnvironment environment();
QString deviceCheck();
}
#endif

class UsbSsh : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(QString command READ command CONSTANT)
    Q_PROPERTY(QString linkStatus READ linkStatus NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canOpenTerminal READ canOpenTerminal CONSTANT)
    Q_PROPERTY(bool importBusy READ importBusy NOTIFY changed)
    Q_PROPERTY(bool importCompleted READ importCompleted NOTIFY changed)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY changed)
    Q_PROPERTY(QVariantMap importPreview READ importPreview NOTIFY changed)

public:
    explicit UsbSsh(bool onTablet, QObject *parent = nullptr);
    ~UsbSsh() override;

    QString command() const;
    QString linkStatus() const { return m_linkStatus; }
    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    bool canOpenTerminal() const;
    bool importBusy() const { return m_importing; }
    bool importCompleted() const { return m_importCompleted; }
    QString importStatus() const { return m_importStatus; }
    QVariantMap importPreview() const { return m_importPreview; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copyCommand();
    Q_INVOKABLE bool openTerminal();
    Q_INVOKABLE void importFromPaperPro(Notebook *notebook);
    Q_INVOKABLE void confirmImport();
    Q_INVOKABLE void cancelImport();

signals:
    void changed();

private:
    enum class ImportFailure { None, Cancelled, Oversized, TimedOut };

    QHostAddress localUsbAddress();
    void readBanner();
    void finish(const QString &status);
    void readImportOutput();
    void readImportDiagnostic();
    void finishImport(int exitCode, QProcess::ExitStatus exitStatus);
    void clearImportPreview();

    const bool m_onTablet;
    QTcpSocket m_socket;
    QTimer m_timeout;
    QProcess m_importProcess;
    QTimer m_importTimeout;
    QPointer<Notebook> m_importNotebook;
    QByteArray m_importPayload;
    QByteArray m_importDiagnostic;
    QString m_importToken;
    QVariantMap m_importPreview;
    ImportFailure m_importFailure = ImportFailure::None;
    QString m_linkStatus;
    QString m_status;
    QString m_importStatus;
    qint64 m_received = 0;
    bool m_busy = false;
    bool m_importing = false;
    bool m_importCompleted = false;
};
