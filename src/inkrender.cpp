#include "inkrender.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <cmath>

namespace {
QPointF pagePosition(QSizeF pageSize, const InkPoint &point)
{
    return {point.position.x() * pageSize.width(), point.position.y() * pageSize.height()};
}
}

InkTool inkToolFromName(const QString &name)
{
    if (name == QStringLiteral("pencil"))
        return InkTool::Pencil;
    if (name == QStringLiteral("marker"))
        return InkTool::Marker;
    if (name == QStringLiteral("highlighter"))
        return InkTool::Highlighter;
    if (name == QStringLiteral("underline"))
        return InkTool::Underline;
    if (name == QStringLiteral("eraser"))
        return InkTool::Eraser;
    if (name == QStringLiteral("line"))
        return InkTool::Line;
    if (name == QStringLiteral("rectangle"))
        return InkTool::Rectangle;
    if (name == QStringLiteral("ellipse"))
        return InkTool::Ellipse;
    return InkTool::Pen;
}

QString inkToolName(InkTool tool)
{
    switch (tool) {
    case InkTool::Pencil: return QStringLiteral("pencil");
    case InkTool::Marker: return QStringLiteral("marker");
    case InkTool::Highlighter: return QStringLiteral("highlighter");
    case InkTool::Underline: return QStringLiteral("underline");
    case InkTool::Eraser: return QStringLiteral("eraser");
    case InkTool::Line: return QStringLiteral("line");
    case InkTool::Rectangle: return QStringLiteral("rectangle");
    case InkTool::Ellipse: return QStringLiteral("ellipse");
    case InkTool::Pen: return QStringLiteral("pen");
    }
    return {};
}

bool isInkShape(InkTool tool)
{
    return tool == InkTool::Line || tool == InkTool::Underline
        || tool == InkTool::Rectangle || tool == InkTool::Ellipse;
}

qreal inkRadius(qreal pageWidth, qreal strokeWidth, qreal pressure, InkTool tool)
{
    const qreal scale = strokeWidth * pageWidth / 1200.0;
    switch (tool) {
    case InkTool::Pencil: return qMax(0.35, scale * (0.15 + 0.5 * pressure));
    case InkTool::Marker: return qMax(0.35, scale * (0.75 + 0.25 * pressure));
    case InkTool::Highlighter: return qMax(0.35, scale * 1.45);
    case InkTool::Line:
    case InkTool::Underline:
    case InkTool::Rectangle:
    case InkTool::Ellipse: return qMax(0.35, scale);
    case InkTool::Eraser:
    case InkTool::Pen: return qMax(0.35, scale * (0.25 + 0.75 * pressure));
    }
    return 0.35;
}

QList<InkPoint> shapeOutline(const QList<InkPoint> &anchors, InkTool tool)
{
    if (!isInkShape(tool) || anchors.size() != 2)
        return {};

    const QPointF a = anchors.constFirst().position;
    const QPointF b = anchors.constLast().position;
    const QRectF box = QRectF(a, b).normalized();
    if (a == b)
        return {};
    if (tool == InkTool::Line || tool == InkTool::Underline || box.isEmpty())
        return {{a, 1}, {b, 1}};
    if (tool == InkTool::Rectangle)
        return {{box.topLeft(), 1}, {box.topRight(), 1}, {box.bottomRight(), 1},
                {box.bottomLeft(), 1}, {box.topLeft(), 1}};

    constexpr int segments = 128;
    constexpr qreal turn = 6.283185307179586;
    const QPointF center = box.center();
    QList<InkPoint> outline;
    outline.reserve(segments + 1);
    for (int step = 0; step < segments; ++step) {
        const qreal angle = step * turn / segments;
        outline.append({{center.x() + std::cos(angle) * box.width() / 2,
                         center.y() + std::sin(angle) * box.height() / 2}, 1});
    }
    outline.append(outline.constFirst());
    return outline;
}

QList<InkPoint> storedInkPoints(const QVariantMap &stroke)
{
    const QVariantList entries = stroke.value(QStringLiteral("points")).toList();
    QList<InkPoint> points;
    points.reserve(entries.size());
    for (const QVariant &entry : entries) {
        const QVariantMap point = entry.toMap();
        points.append({{point.value(QStringLiteral("x")).toReal(), point.value(QStringLiteral("y")).toReal()},
                       point.value(QStringLiteral("p")).toReal()});
    }
    return points;
}

QRect drawInkSegment(QPainter &painter, QSizeF pageSize, const InkPoint &from, const InkPoint &to,
                     const QColor &color, qreal strokeWidth, InkTool tool)
{
    const QPointF a = pagePosition(pageSize, from);
    const QPointF b = pagePosition(pageSize, to);
    const qreal startRadius = inkRadius(pageSize.width(), strokeWidth, from.pressure, tool);
    const qreal endRadius = inkRadius(pageSize.width(), strokeWidth, to.pressure, tool);
    const QPointF delta = b - a;
    const qreal length = std::hypot(delta.x(), delta.y());

    painter.setPen(Qt::NoPen);
    painter.setBrush(tool == InkTool::Pencil ? color.lighter(160) : color);
    if (length > 0) {
        const QPointF normal(-delta.y() / length, delta.x() / length);
        const QPointF outline[] = {a + normal * startRadius, b + normal * endRadius,
                                   b - normal * endRadius, a - normal * startRadius};
        painter.drawConvexPolygon(outline, 4);
    }
    painter.drawEllipse(b, endRadius, endRadius);

    const qreal padding = qMax(startRadius, endRadius) + 2;
    return QRectF(a, b).normalized().adjusted(-padding, -padding, padding, padding).toAlignedRect();
}

void drawHighlighterStroke(QPainter &painter, QSizeF pageSize, const QList<InkPoint> &points,
                           const QColor &color, qreal strokeWidth)
{
    if (points.isEmpty())
        return;
    QColor translucent = color;
    translucent.setAlphaF(0.34);
    const qreal radius = inkRadius(pageSize.width(), strokeWidth, 1, InkTool::Highlighter);
    const QPointF start = pagePosition(pageSize, points.constFirst());
    QPainterPath centerLine(start);
    for (const InkPoint &point : points)
        centerLine.lineTo(pagePosition(pageSize, point));
    if (centerLine.elementCount() == 1) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(translucent);
        painter.drawEllipse(start, radius, radius);
        return;
    }

    QPainterPathStroker outline;
    outline.setWidth(2 * radius);
    outline.setCapStyle(Qt::RoundCap);
    outline.setJoinStyle(Qt::RoundJoin);
    // Fill one outline so a translucent stroke does not darken at every sample or self-crossing.
    painter.fillPath(outline.createStroke(centerLine), translucent);
}

void drawInkShape(QPainter &painter, QSizeF pageSize, const QList<InkPoint> &anchors,
                  const QColor &color, qreal strokeWidth, InkTool tool)
{
    const auto outline = shapeOutline(anchors, tool);
    if (outline.isEmpty())
        return;

    const qreal radius = inkRadius(pageSize.width(), strokeWidth, 1, tool);
    QPolygonF polygon;
    polygon.reserve(outline.size());
    for (const InkPoint &point : outline)
        polygon.append(pagePosition(pageSize, point));

    painter.setPen(QPen(color, 2 * radius, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (polygon.size() > 2)
        painter.drawPolygon(polygon);
    else
        painter.drawPolyline(polygon);
}

void paintStoredStrokes(QPainter &painter, QSizeF pageSize, const QVariantList &strokes,
                        const QSet<int> &excluded)
{
    painter.setRenderHint(QPainter::Antialiasing);
    for (qsizetype index = 0; index < strokes.size(); ++index) {
        if (excluded.contains(index))
            continue;
        const QVariantMap stroke = strokes.at(index).toMap();
        const QColor color(stroke.value(QStringLiteral("color")).toString());
        const qreal width = stroke.value(QStringLiteral("width")).toReal();
        if (!color.isValid() || !std::isfinite(width) || width <= 0)
            continue;

        const auto tool = inkToolFromName(stroke.value(QStringLiteral("tool")).toString());
        const auto points = storedInkPoints(stroke);
        if (tool == InkTool::Highlighter) {
            drawHighlighterStroke(painter, pageSize, points, color, width);
            continue;
        }
        if (isInkShape(tool)) {
            drawInkShape(painter, pageSize, points, color, width, tool);
            continue;
        }

        if (points.isEmpty())
            continue;
        InkPoint previous = points.constFirst();
        for (const InkPoint &point : points) {
            drawInkSegment(painter, pageSize, previous, point, color, width, tool);
            previous = point;
        }
    }
}
