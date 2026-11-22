#pragma once

#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QRectF>
#include <QVariantList>

namespace MixedPageTable {

// Returns interiors of an explicit ruled table, in full-image normalized
// coordinates, in row-major order. Each cell is included, even when empty.
// An empty result requests ordinary line recognition for the selection.
QList<QList<QRectF>> cells(const QImage &image, const QRectF &selection);

// Keep original pen strokes fully inside one cell, in capture order. Grid
// rules and strokes that cross a divider or the selected contour stay out.
QVariantList cellInk(const QList<QList<QRectF>> &grid, const QVariantList &strokes, const QPainterPath &contour);

}
