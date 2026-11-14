#pragma once

#include <QPointF>
#include <QRect>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <QVariantList>

class QColor;
class QPainter;

enum class InkTool { Pen, Pencil, Marker, Highlighter, Underline, Eraser, Line, Rectangle, Ellipse };

struct InkPoint {
    QPointF position;
    qreal pressure;
};

InkTool inkToolFromName(const QString &name);
QString inkToolName(InkTool tool);
bool isInkShape(InkTool tool);
qreal inkRadius(qreal pageWidth, qreal strokeWidth, qreal pressure, InkTool tool);
QList<InkPoint> storedInkPoints(const QVariantMap &stroke);
QList<InkPoint> shapeOutline(const QList<InkPoint> &anchors, InkTool tool);
QRect drawInkSegment(QPainter &painter, QSizeF pageSize, const InkPoint &from, const InkPoint &to,
                     const QColor &color, qreal strokeWidth, InkTool tool = InkTool::Pen);
void drawHighlighterStroke(QPainter &painter, QSizeF pageSize, const QList<InkPoint> &points,
                           const QColor &color, qreal strokeWidth);
void drawInkShape(QPainter &painter, QSizeF pageSize, const QList<InkPoint> &anchors,
                  const QColor &color, qreal strokeWidth, InkTool tool);
void paintStoredStrokes(QPainter &painter, QSizeF pageSize, const QVariantList &strokes,
                        const QSet<int> &excluded = {});
