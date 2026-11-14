#pragma once

#include "inkrender.h"

#include <optional>

struct RecognizedShape {
    InkTool tool;
    QList<InkPoint> anchors;
};

std::optional<RecognizedShape> recognizeInkShape(const QList<InkPoint> &points, QSizeF pageSize);
