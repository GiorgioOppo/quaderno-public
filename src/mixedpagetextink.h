#pragma once

#include <QImage>
#include <QVariantList>

namespace MixedPageTextInk {

// Recover centerline strokes for each handwritten text row of a masked image.
// Both the QRectF bounds and the stroke points use normalized page coordinates.
QVariantList lines(const QImage &image);

// Diagram captions can be thinner than the body text or run along either
// axis. Return their component bounds and strokes before grouping into rows.
QVariantList diagramComponents(const QImage &image);

}
