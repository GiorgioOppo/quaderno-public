#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Handwriting : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString result READ result NOTIFY changed)

public:
    explicit Handwriting(QObject *parent = nullptr);
    ~Handwriting() override;

    bool available() const;
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString result() const { return m_result; }

    Q_INVOKABLE void recognize(const QVariantList &strokes);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void finished(bool success);

private:
    static QString helperPath();
    void startProcess();
    void complete(const QString &text, const QString &error);

    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_input;
    QString m_error;
    QString m_result;
    bool m_busy = false;
    bool m_timedOut = false;
    bool m_waitingForExit = false;
};
