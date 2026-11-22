#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include <memory>

class PagePreviewItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap content READ content WRITE setContent NOTIFY contentChanged)
    Q_PROPERTY(bool thumbnail READ thumbnail WRITE setThumbnail NOTIFY thumbnailChanged)
    Q_PROPERTY(qreal renderScale READ renderScale WRITE setRenderScale NOTIFY renderScaleChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY renderStateChanged)
    Q_PROPERTY(QString error READ error NOTIFY renderStateChanged)

public:
    explicit PagePreviewItem(QQuickItem *parent = nullptr);
    ~PagePreviewItem() override;
    QVariantMap content() const { return m_content; }
    void setContent(const QVariantMap &content);
    bool thumbnail() const { return m_thumbnail; }
    void setThumbnail(bool thumbnail);
    qreal renderScale() const { return m_renderScale; }
    void setRenderScale(qreal scale);
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    void paint(QPainter *painter) override;

signals:
    void contentChanged();
    void thumbnailChanged();
    void renderScaleChanged();
    void renderStateChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void scheduleBackground(bool clear);
    void decodeBackground();
    QVariantMap m_content;
    QImage m_background;
    QTimer m_renderDelay;
    std::shared_ptr<std::atomic_bool> m_cancel;
    quint64 m_generation = 0;
    qreal m_renderScale = 1;
    bool m_thumbnail = true;
    bool m_busy = false;
    QString m_error;
};
