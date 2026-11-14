#include "applanguage.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QSettings>

namespace {
AppLanguage *instance = nullptr;
}

AppLanguage::AppLanguage(QObject *parent) : QObject(parent)
{
    instance = this;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
    if (settings.value(QStringLiteral("language")) == QStringLiteral("en") && loadEnglish())
        m_current = QStringLiteral("en");
    const QString layout = settings.value(QStringLiteral("keyboardLayout")).toString();
    if (layout == QStringLiteral("it") || layout == QStringLiteral("en"))
        m_keyboardLayout = layout;
}

AppLanguage::~AppLanguage()
{
    for (auto *translator : {&m_main, &m_dialogs, &m_cpp, &m_notebook, &m_sync})
        QCoreApplication::removeTranslator(translator);
    if (instance == this)
        instance = nullptr;
}

AppLanguage *AppLanguage::create(QQmlEngine *engine, QJSEngine *)
{
    auto *language = instance ? instance : new AppLanguage(engine);
    language->m_engine = engine;
    QQmlEngine::setObjectOwnership(language, QQmlEngine::CppOwnership);
    return language;
}

bool AppLanguage::loadEnglish()
{
    if (!m_main.load(QStringLiteral(":/translations/quaderno_main_en.qm"))
        || !m_dialogs.load(QStringLiteral(":/translations/quaderno_dialogs_en.qm"))
        || !m_cpp.load(QStringLiteral(":/translations/quaderno_cpp_en.qm"))
        || !m_notebook.load(QStringLiteral(":/translations/quaderno_notebook_en.qm"))
        || !m_sync.load(QStringLiteral(":/translations/quaderno_sync_en.qm")))
        return false;
    for (auto *translator : {&m_main, &m_dialogs, &m_cpp, &m_notebook, &m_sync})
        QCoreApplication::installTranslator(translator);
    return true;
}

bool AppLanguage::select(const QString &language)
{
    if (language != QStringLiteral("it") && language != QStringLiteral("en"))
        return false;
    if (language == m_current)
        return true;
    if (language == QStringLiteral("en")) {
        if (!loadEnglish())
            return false;
    } else {
        for (auto *translator : {&m_main, &m_dialogs, &m_cpp, &m_notebook, &m_sync})
            QCoreApplication::removeTranslator(translator);
    }
    m_current = language;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
    settings.setValue(QStringLiteral("language"), language);
    settings.sync();
    emit changed();
    if (m_keyboardLayout.isEmpty())
        emit keyboardLayoutChanged();
    updateEngine();
    return true;
}

void AppLanguage::selectKeyboardLayout(const QString &layout)
{
    if ((layout != QStringLiteral("it") && layout != QStringLiteral("en")) || layout == m_keyboardLayout)
        return;
    const QString previousLayout = keyboardLayout();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
    settings.setValue(QStringLiteral("keyboardLayout"), layout);
    settings.sync();
    m_keyboardLayout = layout;
    if (layout != previousLayout)
        emit keyboardLayoutChanged();
}

void AppLanguage::updateEngine()
{
    if (m_engine)
        m_engine->retranslate();
}
