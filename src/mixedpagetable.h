#pragma once

#include <QImage>
#include <QList>
#include <QRectF>

namespace MixedPageTable {

// Returns interiors of an explicit ruled table, in full-image normalized
// coordinates, in row-major order. Each cell is included, even when empty.
// An empty result requests ordinary line recognition for the selection.
QList<QList<QRectF>> cells(const QImage &image, const QRectF &selection);

}
