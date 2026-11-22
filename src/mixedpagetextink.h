#pragma once

#include <QImage>
#include <QVariantList>

namespace MixedPageTextInk {

// Recover centerline strokes for each handwritten text row of a masked image.
// Both the QRectF bounds and the stroke points use normalized page coordinates.
QVariantList lines(const QImage &image);

}
