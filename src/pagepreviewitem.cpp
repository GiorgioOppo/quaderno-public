#include "pagepreviewitem.h"

#include "inkrender.h"
#include "pagecontent.h"
#include "pdfimport.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>

PagePreviewItem::PagePreviewItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setOpaquePainting(true);
}

void PagePreviewItem::setContent(const QVariantMap &content)
{
    if (content == m_content)
        return;
    const auto key = QStringLiteral("background");
    const bool backgroundChanged = content.value(key) != m_content.value(key);
    m_content = content;
    if (backgroundChanged)
        decodeBackground();
    emit contentChanged();
    update();
}

void PagePreviewItem::setThumbnail(bool thumbnail)
{
    if (thumbnail == m_thumbnail)
        return;
    m_thumbnail = thumbnail;
    decodeBackground();
    emit thumbnailChanged();
    update();
}

void PagePreviewItem::decodeBackground()
{
    m_background = {};
    const QByteArray data = m_content.value(QStringLiteral("background")).toByteArray();
    if (data.isEmpty())
        return;

    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly))
        return;
    QImageReader reader(&buffer, "jpeg");
    const QSize source = reader.size();
    if (source.isEmpty())
        return;
    const int maxEdge = m_thumbnail ? 440 : 1200;
    if (qMax(source.width(), source.height()) > maxEdge) {
        QSize scaled = source.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio);
        reader.setScaledSize(scaled.expandedTo(QSize(1, 1)));
    }
    m_background = reader.read();
}

void PagePreviewItem::paint(QPainter *painter)
{
    // The Paper Pro scene graph ignores QQuickPaintedItem's fill color.
    painter->fillRect(boundingRect(), Qt::white);
    const QSizeF pageSize = size();
    if (m_background.isNull())
        PageContent::paintTemplate(*painter, pageSize, m_content.value(QStringLiteral("paper")).toString());
    else
        paintPdfBackground(*painter, pageSize, m_background);
    PageContent::paintItems(*painter, pageSize, m_content.value(QStringLiteral("items")).toList());
    paintStoredStrokes(*painter, pageSize, m_content.value(QStringLiteral("strokes")).toList());
}
