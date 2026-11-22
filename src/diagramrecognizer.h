#pragma once

#include <QVariantList>
#include <QVariantMap>

// Pure offline geometry analysis. All indexes in the result are offsets into
// the supplied stroke list, never page-global indexes. No document is mutated.
namespace DiagramRecognition {
inline constexpr qsizetype maximumStrokes = 4000;
inline constexpr qsizetype maximumPoints = 120000;
// Supplying an existing page diagram extends that item. Existing IDs, labels,
// node placement and properties are retained; candidate indexes refer only to
// newly recognized ink. A valid no-op has hasChanges=false.
QVariantMap recognize(const QVariantList &strokes, const QVariantMap &existingDiagram = {});
}
