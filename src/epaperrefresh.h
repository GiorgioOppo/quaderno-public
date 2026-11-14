#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQuickWindow;

class EpaperRefresh : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool automaticEnabled READ automaticEnabled WRITE setAutomaticEnabled NOTIFY automaticEnabledChanged)

public:
    explicit EpaperRefresh(QObject *parent = nullptr);
    ~EpaperRefresh() override;

    void attach(QQuickWindow *window);
    bool available() const { return !m_framebuffer.isNull(); }
    bool automaticEnabled() const { return m_automaticEnabled; }
    void setAutomaticEnabled(bool enabled);
    Q_INVOKABLE bool request();
    Q_INVOKABLE void setFastRefresh(bool enabled);
    Q_INVOKABLE void watchPageChange(bool visiblePage);

signals:
    void availableChanged();
    void automaticEnabledChanged();

private:
    bool ready() const;
    void findFramebuffer();

    QPointer<QQuickWindow> m_window;
    QPointer<QObject> m_framebuffer;
    QMetaObject::Connection m_firstFrame;
    QTimer m_pageFrameTimeout;
    void *m_plugin = nullptr;
    int *m_screenMode = nullptr;
    int m_contentMode = -1;
    int m_uiMode = -1;
    bool m_fastRefresh = false;
    bool m_automaticEnabled = true;
};
