#include "pagepreviewitem.h"

#include "inkrender.h"
#include "pagecontent.h"
#include "pdfimport.h"
#include "documentreader.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QImageReader>
#include <QPainter>
#include <QThreadPool>
#include <QQuickWindow>
#include <QtMath>
#include <cmath>

namespace {
QThreadPool *renderPool()
{
    static QThreadPool *pool = [] {
        auto *value = new QThreadPool(QCoreApplication::instance());
        value->setMaxThreadCount(1);
        value->setExpiryTimeout(10000);
        return value;
    }();
    return pool;
}

class RenderJob final : public QObject
{
    Q_OBJECT
public:
    void run(const QVariantMap &content, int maximumEdge, const std::shared_ptr<std::atomic_bool> &cancel)
    {
        if (cancel->load()) {
            deleteLater();
            return;
        }
        const QVariantMap source = content.value(QStringLiteral("source")).toMap();
        const QByteArray data = content.value(QStringLiteral("background")).toByteArray();
        const QString key = source.isEmpty()
            ? QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) + u'/' + QString::number(maximumEdge)
            : source.value(QStringLiteral("asset")).toString() + u'/' + source.value(QStringLiteral("page")).toString()
                + u'/' + QString::number(maximumEdge);
        const quint64 cacheGeneration = DocumentCache::generation();
        QImage image = DocumentCache::image(key);
        QString error;
        if (image.isNull()) {
            if (!source.isEmpty()) {
                image = renderDocumentPage(source, maximumEdge, error);
            } else if (!data.isEmpty()) {
                QBuffer input;
                input.setData(data);
                input.open(QIODevice::ReadOnly);
                QImageReader reader(&input);
                const QSize size = reader.size();
                if (size.isValid() && qMax(size.width(), size.height()) > maximumEdge)
                    reader.setScaledSize(size.scaled(maximumEdge, maximumEdge, Qt::KeepAspectRatio));
                image = reader.read();
                if (image.isNull())
                    error = reader.errorString();
            }
            if (!image.isNull() && !cancel->load()) {
                DocumentCache::insertImage(key, image, cacheGeneration);
            }
        }
        if (!cancel->load())
            emit ready(image, error);
        deleteLater();
    }
signals:
    void ready(const QImage &image, const QString &error);
};
}

PagePreviewItem::PagePreviewItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setOpaquePainting(true);
    m_renderDelay.setSingleShot(true);
    connect(&m_renderDelay, &QTimer::timeout, this, &PagePreviewItem::decodeBackground);
}

PagePreviewItem::~PagePreviewItem()
{
    if (m_cancel)
        m_cancel->store(true);
}

void PagePreviewItem::setContent(const QVariantMap &content)
{
    if (content == m_content)
        return;
    const bool backgroundChanged = content.value(QStringLiteral("background")) != m_content.value(QStringLiteral("background"))
        || content.value(QStringLiteral("source")) != m_content.value(QStringLiteral("source"));
    m_content = content;
    if (backgroundChanged)
        scheduleBackground(true);
    emit contentChanged();
    update();
}

void PagePreviewItem::setThumbnail(bool thumbnail)
{
    if (thumbnail == m_thumbnail)
        return;
    m_thumbnail = thumbnail;
    scheduleBackground(false);
    emit thumbnailChanged();
}

void PagePreviewItem::setRenderScale(qreal scale)
{
    scale = std::isfinite(scale) ? qBound(1.0, scale, 4.0) : 1.0;
    if (qFuzzyCompare(m_renderScale, scale))
        return;
    m_renderScale = scale;
    scheduleBackground(false);
    emit renderScaleChanged();
}

void PagePreviewItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (!m_thumbnail && newGeometry.size() != oldGeometry.size())
        scheduleBackground(false);
}

void PagePreviewItem::scheduleBackground(bool clear)
{
    ++m_generation;
    if (m_cancel)
        m_cancel->store(true);
    if (clear)
        m_background = {};
    m_error.clear();
    m_busy = !m_content.value(QStringLiteral("background")).toByteArray().isEmpty()
        || !m_content.value(QStringLiteral("source")).toMap().isEmpty();
    emit renderStateChanged();
    if (m_busy)
        m_renderDelay.start(clear ? 0 : 90);
    else
        m_renderDelay.stop();
    update();
}

void PagePreviewItem::decodeBackground()
{
    const quint64 generation = m_generation;
    const qreal pixelRatio = window() ? window()->effectiveDevicePixelRatio() : 1;
    const qreal logicalEdge = qMax(width(), height());
    const int maximumEdge = m_thumbnail ? 440 : qBound(1200, qCeil(logicalEdge * m_renderScale * pixelRatio), 4096);
    const qreal textureScale = m_thumbnail ? 1 : qMin(m_renderScale, 4096.0 / qMax(1.0, logicalEdge * pixelRatio));
    const QSize requestedTexture = m_thumbnail ? QSize{} : QSize(qMax(1, qCeil(width() * textureScale)),
                                                                qMax(1, qCeil(height() * textureScale)));
    const QVariantMap content = m_content;
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    m_cancel = cancel;
    auto *job = new RenderJob;
    connect(job, &RenderJob::ready, this, [this, generation, requestedTexture](const QImage &image, const QString &error) {
        if (generation != m_generation)
            return;
        m_background = image;
        setTextureSize(requestedTexture);
        m_error = error;
        m_busy = false;
        emit renderStateChanged();
        update();
    }, Qt::QueuedConnection);
    renderPool()->start([job, content, maximumEdge, cancel] { job->run(content, maximumEdge, cancel); });
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

#include "pagepreviewitem.moc"
