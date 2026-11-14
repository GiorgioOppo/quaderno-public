#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class PagePreviewItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap content READ content WRITE setContent NOTIFY contentChanged)
    Q_PROPERTY(bool thumbnail READ thumbnail WRITE setThumbnail NOTIFY thumbnailChanged)

public:
    explicit PagePreviewItem(QQuickItem *parent = nullptr);

    QVariantMap content() const { return m_content; }
    void setContent(const QVariantMap &content);
    bool thumbnail() const { return m_thumbnail; }
    void setThumbnail(bool thumbnail);
    void paint(QPainter *painter) override;

signals:
    void contentChanged();
    void thumbnailChanged();

private:
    void decodeBackground();
    QVariantMap m_content;
    QImage m_background;
    bool m_thumbnail = true;
};
