#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTranslator>
#include <QtQml/qqmlregistration.h>

class AppLanguage : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(QString keyboardLayout READ keyboardLayout NOTIFY keyboardLayoutChanged)

public:
    explicit AppLanguage(QObject *parent);
    static AppLanguage *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    ~AppLanguage() override;
    QString current() const { return m_current; }
    QString keyboardLayout() const { return m_keyboardLayout.isEmpty() ? m_current : m_keyboardLayout; }
    Q_INVOKABLE bool select(const QString &language);
    Q_INVOKABLE void selectKeyboardLayout(const QString &layout);

signals:
    void changed();
    void keyboardLayoutChanged();

private:
    bool loadEnglish();
    void updateEngine();

    QTranslator m_main;
    QTranslator m_dialogs;
    QTranslator m_cpp;
    QTranslator m_notebook;
    QTranslator m_sync;
    QString m_current = QStringLiteral("it");
    QString m_keyboardLayout;
    QPointer<QQmlEngine> m_engine;
};
