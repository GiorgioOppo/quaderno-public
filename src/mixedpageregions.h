#pragma once

#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QRect>
#include <QVariantList>

namespace MixedPageRegions {

using Regions = QList<QPainterPath>;

// Contours use full-page coordinates in [0, 1] and are closed implicitly.
// Legacy QRectF entries use the same coordinates. Invalid input is rejected.
Regions validate(const QVariantList &input);

bool contains(const QPointF &point, const Regions &regions);

// Selects the union of the contours and sets all other pixels to white.
// An optional crop is in source-image pixels. The result is the size of
// that crop clipped to the source image, or the full image when omitted.
QImage mask(const QImage &image, const Regions &regions, const QRect &pixelCrop = {});

}
