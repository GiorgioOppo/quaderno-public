#include "epaperrefresh.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QQuickWindow>
#include <QSettings>

#ifdef Q_OS_LINUX
#include <dlfcn.h>
#endif

Q_LOGGING_CATEGORY(lcEpaperRefresh, "quaderno.epaper.refresh", QtInfoMsg)

namespace {
QSettings displaySettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
}

const auto automaticRefreshKey = QStringLiteral("display/automaticRefresh");
}

EpaperRefresh::EpaperRefresh(QObject *parent) : QObject(parent)
{
    m_automaticEnabled = displaySettings().value(automaticRefreshKey, true).toBool();
    m_pageFrameTimeout.setSingleShot(true);
    m_pageFrameTimeout.setInterval(350);
    connect(&m_pageFrameTimeout, &QTimer::timeout, this, [this] {
        if (m_automaticEnabled && ready()) {
            // Ask Qt for a frame only when it has not submitted one for the page change.
            // Its normal damage detection decides which pixels the panel needs to update.
            m_window->requestUpdate();
            qCDebug(lcEpaperRefresh) << "Requested a Qt frame for a pending page change";
        }
    });
}

EpaperRefresh::~EpaperRefresh()
{
#ifdef Q_OS_LINUX
    if (m_screenMode)
        *m_screenMode = m_contentMode;
    if (m_plugin)
        dlclose(m_plugin);
#endif
}

void EpaperRefresh::setAutomaticEnabled(bool enabled)
{
    if (enabled == m_automaticEnabled)
        return;
    m_automaticEnabled = enabled;
    if (!enabled)
        m_pageFrameTimeout.stop();
    auto settings = displaySettings();
    settings.setValue(automaticRefreshKey, enabled);
    settings.sync();
    emit automaticEnabledChanged();
}

void EpaperRefresh::attach(QQuickWindow *window)
{
    if (!window)
        return;
    m_window = window;
    m_firstFrame = connect(window, &QQuickWindow::frameSwapped, this, &EpaperRefresh::findFramebuffer,
                           Qt::QueuedConnection);
    connect(window, &QQuickWindow::frameSwapped, &m_pageFrameTimeout, &QTimer::stop, Qt::QueuedConnection);
    window->requestUpdate();
}

void EpaperRefresh::findFramebuffer()
{
#ifdef Q_OS_LINUX
    if (available() || !m_window || !m_window->isSceneGraphInitialized() || !m_window->isExposed())
        return;
    disconnect(m_firstFrame);

    // The e-paper scene graph creates this singleton. Resolve it only after Qt has drawn a frame.
    m_plugin = dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_NOLOAD | RTLD_LAZY | RTLD_LOCAL);
    if (!m_plugin)
        return;

    const auto instance = reinterpret_cast<QObject *(*)()>(dlsym(m_plugin, "_ZN13EPFramebuffer8instanceEv"));
    const auto *metaObject = static_cast<const QMetaObject *>(dlsym(m_plugin, "_ZN13EPFramebuffer16staticMetaObjectE"));
    if (!instance || !metaObject || metaObject->indexOfSlot("clearGhosting()") < 0)
        return;

    QObject *framebuffer = instance();
    if (!framebuffer || !framebuffer->metaObject()->inherits(metaObject) || framebuffer->thread() != thread())
        return;

    m_framebuffer = framebuffer;
    connect(framebuffer, &QObject::destroyed, this, &EpaperRefresh::availableChanged);
    qCDebug(lcEpaperRefresh) << "Manual display refresh available";

    // On the verified firmware, the e-paper render loop runs on the GUI thread and
    // reads this default for each dirty region. Changing it does not queue a repaint.
    const auto *modeObject = static_cast<const QMetaObject *>(dlsym(m_plugin,
        "_ZN16EPScreenModeItem16staticMetaObjectE"));
    const int enumIndex = modeObject ? modeObject->indexOfEnumerator("Mode") : -1;
    if (enumIndex >= 0) {
        const QMetaEnum modes = modeObject->enumerator(enumIndex);
        m_contentMode = modes.keyToValue("Content");
        m_uiMode = modes.keyToValue("UI");
        auto *defaultMode = static_cast<int *>(dlsym(m_plugin, "_ZN15EPScreenModeMap12uiScreenModeE"));
        if (defaultMode && m_uiMode >= 0 && m_contentMode >= 0 && m_uiMode != m_contentMode
            && *defaultMode == m_contentMode) {
            m_screenMode = defaultMode;
            setFastRefresh(m_fastRefresh);
        }
    }
    emit availableChanged();
#endif
}

bool EpaperRefresh::ready() const
{
    return available() && m_window && m_window->isExposed() && m_window->isSceneGraphInitialized()
        && !QGuiApplication::modalWindow();
}

bool EpaperRefresh::request()
{
    if (!ready())
        return false;

    m_pageFrameTimeout.stop();
    // Called on the GUI thread, outside the render loop. This also works on a static page.
    if (!QMetaObject::invokeMethod(m_framebuffer.data(), "clearGhosting", Qt::DirectConnection))
        return false;
    qCDebug(lcEpaperRefresh) << "Full display refresh completed";
    return true;
}

void EpaperRefresh::setFastRefresh(bool enabled)
{
    m_fastRefresh = enabled;
    if (m_screenMode)
        *m_screenMode = enabled ? m_uiMode : m_contentMode;
}

void EpaperRefresh::watchPageChange(bool visiblePage)
{
    if (visiblePage && m_automaticEnabled && ready())
        m_pageFrameTimeout.start();
    else
        m_pageFrameTimeout.stop();
}
