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
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(QStringList alternatives READ alternatives NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString result READ result NOTIFY changed)
    Q_PROPERTY(QString originalResult READ originalResult NOTIFY changed)
    Q_PROPERTY(bool numericMode MEMBER m_numericMode NOTIFY preferencesChanged)
    Q_PROPERTY(QStringList wordList MEMBER m_wordList NOTIFY preferencesChanged)
    Q_PROPERTY(QStringList corrections MEMBER m_corrections NOTIFY preferencesChanged)

public:
    explicit Handwriting(QObject *parent = nullptr);
    ~Handwriting() override;

    bool available() const;
    bool busy() const { return m_busy; }
    QStringList availableLanguages() const;
    QStringList alternatives() const { return m_alternatives; }
    QString error() const { return m_error; }
    QString result() const { return m_result; }
    QString originalResult() const { return m_originalResult; }
    Q_INVOKABLE QString applyTextPreferences(const QString &text) const;

    Q_INVOKABLE void recognize(const QVariantList &strokes, const QString &language = QStringLiteral("it"));
    Q_INVOKABLE void recognizeDiagramLabel(const QVariantList &strokes, const QString &language, const QString &orientation);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void preferencesChanged();
    void finished(bool success);

private:
    static QString helperPath();
    void recognizeInk(const QVariantList &strokes, const QString &language, const QString &labelOrientation);
    void startProcess();
    void complete(const QString &text, const QString &error);

    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_input;
    QString m_error;
    QString m_result;
    QString m_originalResult;
    bool m_numericMode = false;
    QStringList m_wordList;
    QStringList m_corrections;
    QString m_language = QStringLiteral("it");
    QStringList m_alternatives;
    bool m_busy = false;
    bool m_timedOut = false;
    bool m_waitingForExit = false;
};
