#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class Notebook;

class UsbTransfer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool applying READ applying NOTIFY changed)
    Q_PROPERTY(bool completed READ completed NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap previewInfo READ previewInfo NOTIFY changed)

public:
    explicit UsbTransfer(QObject *parent = nullptr);
    ~UsbTransfer() override;

    bool available() const;
    bool busy() const { return m_stage != Stage::Idle; }
    bool applying() const { return m_stage == Stage::Apply; }
    bool completed() const { return m_completed; }
    QString status() const { return m_status; }
    QVariantMap previewInfo() const { return m_preview; }

    Q_INVOKABLE void preview(Notebook *notebook, const QString &bookId);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void cancel();

signals:
    void changed();

private:
    enum class Stage { Idle, Preview, Apply };
    enum class Failure { None, TimedOut, Oversized, Cancelled, Input };

    void start(Stage stage, const QString &remote);
    void readOutput();
    void readDiagnostic();
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void resetPreview();
    void fail(const QString &message);

    QProcess m_process;
    QTimer m_deadline;
    QPointer<Notebook> m_source;
    QByteArray m_payload;
    QByteArray m_output;
    QByteArray m_diagnostic;
    QString m_bookId;
    QString m_token;
    QString m_revision;
    QVariantMap m_preview;
    QString m_status;
    Stage m_stage = Stage::Idle;
    Failure m_failure = Failure::None;
    bool m_completed = false;
};
