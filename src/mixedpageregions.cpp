#include "mixedpageregions.h"

#include <QMetaType>
#include <QPainter>
#include <QPolygonF>
#include <QRegion>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace {
constexpr int maximumRegions = 6;
constexpr int maximumPoints = 512;
constexpr double minimumSide = .02;
constexpr double minimumArea = .00035;
constexpr int areaResolution = 4096;
constexpr QRectF fullPage(0, 0, 1, 1);

bool validPoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y()) && fullPage.contains(point);
}

bool validBounds(const QRectF &bounds)
{
    return validPoint(bounds.topLeft()) && validPoint(bounds.bottomRight())
        && bounds.width() >= minimumSide && bounds.height() >= minimumSide;
}

QRegion pixelRegion(const QPainterPath &path, const QTransform &transform)
{
    return QRegion(path.toFillPolygon(transform).toPolygon(), Qt::OddEvenFill);
}

bool enoughArea(const QPainterPath &path)
{
    // Measure the same odd-even fill as the final mask. This rejects a
    // retraced or nearly flat contour without rejecting a valid concavity
    // or allowing its bounding rectangle to decide.
    const auto grid = pixelRegion(path, QTransform::fromScale(areaResolution, areaResolution));
    constexpr double minimumGridArea = minimumArea * areaResolution * areaResolution;
    int covered = 0;
    for (const QRect &part : grid) {
        covered += part.width() * part.height();
        if (covered >= minimumGridArea)
            return true;
    }
    return false;
}

QPainterPath contourPath(const QVariantMap &entry)
{
    const QVariantList samples = entry.value(QStringLiteral("points")).toList();
    if (samples.size() < 3 || samples.size() > maximumPoints)
        return {};
    QPolygonF contour;
    contour.reserve(samples.size());
    for (const auto &sample : samples) {
        if (sample.typeId() != QMetaType::QPointF && sample.typeId() != QMetaType::QPoint)
            return {};
        const auto point = sample.toPointF();
        if (!validPoint(point))
            return {};
        if (contour.isEmpty() || contour.constLast() != point)
            contour.append(point);
    }
    if (contour.size() > 1 && contour.constFirst() == contour.constLast())
        contour.removeLast();
    if (contour.size() < 3 || !validBounds(contour.boundingRect()))
        return {};
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    path.addPolygon(contour);
    path.closeSubpath();
    return enoughArea(path) ? path : QPainterPath{};
}
}

MixedPageRegions::Regions MixedPageRegions::validate(const QVariantList &input)
{
    if (input.isEmpty() || input.size() > maximumRegions)
        return {};
    Regions regions;
    regions.reserve(input.size());
    for (const auto &entry : input) {
        QPainterPath path;
        if (entry.typeId() == QMetaType::QVariantMap) {
            path = contourPath(entry.toMap());
        } else if (entry.typeId() == QMetaType::QRectF || entry.typeId() == QMetaType::QRect) {
            const auto rect = entry.toRectF();
            if (validBounds(rect) && rect.width() * rect.height() >= minimumArea)
                path.addRect(rect);
        }
        if (path.isEmpty())
            return {};
        regions.append(path);
    }
    return regions;
}

bool MixedPageRegions::contains(const QPointF &point, const Regions &regions)
{
    return validPoint(point) && std::any_of(regions.cbegin(), regions.cend(), [&point](const QPainterPath &path) {
        return path.contains(point);
    });
}

QImage MixedPageRegions::mask(const QImage &image, const Regions &regions, const QRect &pixelCrop)
{
    if (image.isNull())
        return {};
    const QRect crop = pixelCrop == QRect{} ? image.rect() : image.rect().intersected(pixelCrop);
    if (crop.isEmpty())
        return {};
    QImage result(crop.size(), QImage::Format_RGB32);
    if (result.isNull())
        return {};
    result.fill(Qt::white);

    const QTransform transform(image.width(), 0, 0, image.height(), -crop.x(), -crop.y());
    QRegion selected;
    for (const auto &path : regions)
        selected |= pixelRegion(path, transform);
    selected &= result.rect();
    if (!selected.isEmpty()) {
        QPainter painter(&result);
        painter.setClipRegion(selected);
        painter.drawImage(result.rect(), image, crop);
    }
    return result;
}
