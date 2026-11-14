#include "shaperecognition.h"

#include <QLineF>
#include <QPolygonF>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int sampleCount = 64;
constexpr qreal fullTurn = 6.283185307179586;

qreal length(QPointF point)
{
    return std::hypot(point.x(), point.y());
}

qreal cross(QPointF first, QPointF second)
{
    return first.x() * second.y() - first.y() * second.x();
}

QPolygonF spacedSamples(const QPolygonF &path, qreal pathLength)
{
    QPolygonF samples;
    samples.reserve(sampleCount);
    samples.append(path.first());
    qsizetype segment = 1;
    qreal traversed = 0;
    qreal segmentLength = length(path.at(segment) - path.at(segment - 1));
    for (int index = 1; index < sampleCount - 1; ++index) {
        const qreal distance = index * pathLength / (sampleCount - 1);
        while (segment < path.size() - 1 && traversed + segmentLength < distance) {
            traversed += segmentLength;
            ++segment;
            segmentLength = length(path.at(segment) - path.at(segment - 1));
        }
        const qreal fraction = std::clamp((distance - traversed) / segmentLength, 0.0, 1.0);
        samples.append(path.at(segment - 1) + fraction * (path.at(segment) - path.at(segment - 1)));
    }
    samples.append(path.last());
    return samples;
}

bool looksLikeLine(const QPolygonF &samples, qreal pathLength)
{
    const QPointF start = samples.first();
    const QPointF chord = samples.last() - start;
    const qreal chordLength = length(chord);
    if (chordLength < 0.038 || pathLength > chordLength * 1.18)
        return false;

    const QPointF direction = chord / chordLength;
    const qreal tolerance = std::clamp(chordLength * 0.053, 0.0022, 0.010);
    qreal squaredError = 0;
    qreal previous = 0;
    qreal backtracking = 0;
    for (const QPointF &point : samples) {
        const QPointF fromStart = point - start;
        const qreal perpendicular = qAbs(cross(direction, fromStart));
        if (perpendicular > tolerance)
            return false;
        squaredError += perpendicular * perpendicular;
        const qreal projection = QPointF::dotProduct(fromStart, direction);
        backtracking += qMax(0.0, previous - projection);
        previous = projection;
    }
    return std::sqrt(squaredError / samples.size()) <= tolerance * 0.68 && backtracking <= chordLength * 0.045;
}

bool followsClosedOutline(const QPolygonF &samples, const QRectF &bounds, qreal &areaRatio)
{
    const QPointF center = bounds.center();
    QPointF previous = samples.last() - center;
    qreal sweep = 0;
    qreal totalSweep = 0;
    qreal twiceArea = 0;
    for (const QPointF &point : samples) {
        const QPointF current = point - center;
        const qreal product = cross(previous, current);
        const qreal turn = std::atan2(product, QPointF::dotProduct(previous, current));
        sweep += turn;
        totalSweep += qAbs(turn);
        twiceArea += product;
        previous = current;
    }
    areaRatio = qAbs(twiceArea) / (2 * bounds.width() * bounds.height());
    return qAbs(sweep) >= fullTurn * 0.98 && totalSweep <= fullTurn * 1.19;
}

bool looksLikeRectangle(const QPolygonF &samples, const QRectF &box, qreal areaRatio)
{
    if (areaRatio < 0.86 || areaRatio > 1.02)
        return false;

    const qreal shortSide = qMin(box.width(), box.height());
    const std::array<QPointF, 4> corners{box.topLeft(), box.topRight(), box.bottomRight(), box.bottomLeft()};
    const qreal tolerance = shortSide * 0.16;
    std::array<bool, 4> visited{};
    qreal meanError = 0;
    for (const QPointF &point : samples) {
        const qreal edgeError = std::min({point.x() - box.left(), box.right() - point.x(),
                                          point.y() - box.top(), box.bottom() - point.y()});
        if (edgeError > tolerance)
            return false;
        meanError += edgeError;
        for (size_t index = 0; index < corners.size(); ++index) {
            if (!visited[index] && length(point - corners[index]) <= tolerance)
                visited[index] = true;
        }
    }
    return meanError / samples.size() <= shortSide * 0.06
        && std::all_of(visited.cbegin(), visited.cend(), [](bool corner) { return corner; });
}

bool looksLikeEllipse(const QPolygonF &samples, const QRectF &box, qreal areaRatio)
{
    if (areaRatio < 0.65 || areaRatio > 0.87 || qMax(box.width(), box.height()) > 6 * qMin(box.width(), box.height()))
        return false;

    qreal squaredError = 0;
    for (const QPointF &point : samples) {
        const QPointF centered = point - box.center();
        const qreal radius = std::hypot(2 * centered.x() / box.width(), 2 * centered.y() / box.height());
        const qreal error = qAbs(radius - 1);
        if (error > 0.21)
            return false;
        squaredError += error * error;
    }
    return std::sqrt(squaredError / samples.size()) <= 0.09;
}
}

std::optional<RecognizedShape> recognizeInkShape(const QList<InkPoint> &points, QSizeF pageSize)
{
    if (points.size() < 2 || pageSize.isEmpty())
        return std::nullopt;

    const qreal ratio = pageSize.height() / pageSize.width();
    const auto asAnchor = [ratio](QPointF point) {
        return InkPoint{{std::clamp(point.x(), 0.0, 1.0), std::clamp(point.y() / ratio, 0.0, 1.0)}, 1};
    };

    QPolygonF path;
    path.reserve(points.size());
    qreal pathLength = 0;
    for (const InkPoint &point : points) {
        const QPointF position(point.position.x(), point.position.y() * ratio);
        if (!path.isEmpty()) {
            const qreal step = length(position - path.last());
            if (step < 0.0007)
                continue;
            pathLength += step;
        }
        path.append(position);
    }
    if (path.size() < 2 || pathLength < 0.038)
        return std::nullopt;

    const QPolygonF samples = spacedSamples(path, pathLength);
    if (looksLikeLine(samples, pathLength))
        return RecognizedShape{InkTool::Line, {asAnchor(samples.first()), asAnchor(samples.last())}};

    const QRectF bounds = samples.boundingRect();
    const qreal shortSide = qMin(bounds.width(), bounds.height());
    const qreal gap = length(samples.first() - samples.last());
    if (path.size() < 4 || shortSide < 0.033 || gap > qMin(0.026, 0.22 * shortSide))
        return std::nullopt;

    qreal areaRatio = 0;
    if (!followsClosedOutline(samples, bounds, areaRatio))
        return std::nullopt;

    const QList<InkPoint> anchors{asAnchor(bounds.topLeft()), asAnchor(bounds.bottomRight())};
    if (looksLikeRectangle(samples, bounds, areaRatio))
        return RecognizedShape{InkTool::Rectangle, anchors};
    if (looksLikeEllipse(samples, bounds, areaRatio))
        return RecognizedShape{InkTool::Ellipse, anchors};
    return std::nullopt;
}
