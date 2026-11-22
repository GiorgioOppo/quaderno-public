#include "diagramrecognizer.h"

#include "inkrender.h"
#include "pagecontent.h"

#include <QLineF>
#include <QObject>
#include <QMap>
#include <QPolygonF>
#include <QPainterPath>
#include <QSet>
#include <QStringList>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr qreal pageWidth = 600;
constexpr qreal pageHeight = 800;
constexpr qreal joinDistance = 7;
constexpr int maximumCandidates = 256;
constexpr int maximumNodes = 64;
constexpr int maximumEdges = 128;
using Path = QList<QPointF>;
struct Stroke { Path points; Path headShape; QRectF bounds; qreal length = 0; QString tool; };
struct Node { QString id; QString shape; QRectF bounds; QList<int> indexes; Path outline; bool existing = false; };
struct Edge { QString id; int from; int to; bool orthogonal; Path points; QList<int> indexes; };

qreal magnitude(QPointF point) { return std::hypot(point.x(), point.y()); }
qreal cross(QPointF first, QPointF second) { return first.x() * second.y() - first.y() * second.x(); }
qreal pathLength(const Path &points)
{
    qreal result = 0;
    for (qsizetype i = 1; i < points.size(); ++i) result += magnitude(points.at(i) - points.at(i - 1));
    return result;
}
qreal segmentDistance(QPointF point, QPointF first, QPointF last)
{
    const QPointF delta = last - first;
    const qreal square = QPointF::dotProduct(delta, delta);
    const qreal position = square > 0 ? std::clamp(QPointF::dotProduct(point - first, delta) / square, 0.0, 1.0) : 0;
    return magnitude(point - first - position * delta);
}
Path resample(const Path &points, int count = 128)
{
    if (points.size() < 2) return points;
    const qreal total = pathLength(points);
    if (total <= 0) return {points.first()};
    Path result{points.first()};
    qsizetype segment = 1;
    qreal traversed = 0;
    qreal step = magnitude(points.at(segment) - points.at(segment - 1));
    for (int i = 1; i < count - 1; ++i) {
        const qreal position = total * i / (count - 1);
        while (segment < points.size() - 1 && traversed + step < position) {
            traversed += step;
            ++segment;
            step = magnitude(points.at(segment) - points.at(segment - 1));
        }
        const qreal fraction = step > 0 ? std::clamp((position - traversed) / step, 0.0, 1.0) : 0;
        result.append(points.at(segment - 1) + fraction * (points.at(segment) - points.at(segment - 1)));
    }
    result.append(points.last());
    return result;
}
Path simplified(const Path &points, qreal tolerance)
{
    if (points.size() < 3) return points;
    QList<QPair<qsizetype, qsizetype>> pending{{0, points.size() - 1}};
    QList<bool> retained(points.size(), false);
    retained.first() = retained.last() = true;
    while (!pending.isEmpty()) {
        const auto [first, last] = pending.takeLast();
        qreal greatest = tolerance;
        qsizetype chosen = -1;
        for (qsizetype i = first + 1; i < last; ++i) {
            const qreal distance = segmentDistance(points.at(i), points.at(first), points.at(last));
            if (distance > greatest) { greatest = distance; chosen = i; }
        }
        if (chosen >= 0) {
            retained[chosen] = true;
            pending.append({first, chosen});
            pending.append({chosen, last});
        }
    }
    Path result;
    for (qsizetype i = 0; i < points.size(); ++i) if (retained.at(i)) result.append(points.at(i));
    return result;
}
bool numeric(const QVariant &value, qreal &result)
{
    switch (value.metaType().id()) {
    case QMetaType::Double: case QMetaType::Float: case QMetaType::Int: case QMetaType::UInt:
    case QMetaType::LongLong: case QMetaType::ULongLong: break;
    default: return false;
    }
    result = value.toDouble();
    return std::isfinite(result) && result >= 0 && result <= 1;
}
QString readStrokes(const QVariantList &input, QList<Stroke> &output)
{
    if (input.size() > DiagramRecognition::maximumStrokes)
        return QObject::tr("Il disegno contiene troppi tratti. Seleziona un diagramma più piccolo.");
    qsizetype totalPoints = 0;
    for (const QVariant &entry : input) {
        if (entry.metaType().id() != QMetaType::QVariantMap)
            return QObject::tr("Il disegno contiene un tratto non valido.");
        const QVariantMap data = entry.toMap();
        const QVariant pointData = data.value(QStringLiteral("points"));
        if (pointData.metaType().id() != QMetaType::QVariantList)
            return QObject::tr("Il disegno contiene un tratto non valido.");
        const QVariantList points = pointData.toList();
        totalPoints += points.size();
        if (totalPoints > DiagramRecognition::maximumPoints)
            return QObject::tr("Il disegno contiene troppi punti. Seleziona un diagramma più piccolo.");
        Stroke stroke;
        stroke.tool = data.value(QStringLiteral("tool"), QStringLiteral("pen")).toString();
        const bool supported = stroke.tool == QStringLiteral("pen") || stroke.tool == QStringLiteral("pencil")
            || stroke.tool == QStringLiteral("marker") || stroke.tool == QStringLiteral("line")
            || stroke.tool == QStringLiteral("rectangle") || stroke.tool == QStringLiteral("ellipse");
        for (const QVariant &entry : points) {
            const QVariantMap point = entry.toMap();
            qreal x = 0, y = 0;
            if (!numeric(point.value(QStringLiteral("x")), x) || !numeric(point.value(QStringLiteral("y")), y))
                return QObject::tr("Il disegno contiene coordinate non valide.");
            const QPointF next{x * pageWidth, y * pageHeight};
            if (supported && (stroke.points.isEmpty() || magnitude(next - stroke.points.last()) > 0.05))
                stroke.points.append(next);
        }
        if ((stroke.tool == QStringLiteral("rectangle") || stroke.tool == QStringLiteral("ellipse"))
            && stroke.points.size() == 2) {
            const auto anchors = storedInkPoints(data);
            stroke.points.clear();
            for (const InkPoint &point : shapeOutline(anchors, inkToolFromName(stroke.tool)))
                stroke.points.append({point.position.x() * pageWidth, point.position.y() * pageHeight});
        }
        stroke.length = pathLength(stroke.points);
        stroke.bounds = QPolygonF(stroke.points).boundingRect();
        if (stroke.points.size() > 128) stroke.points = resample(stroke.points);
        if (stroke.length >= 6 && stroke.length <= 70 && stroke.bounds.width() <= 56 && stroke.bounds.height() <= 56)
            stroke.headShape = simplified(resample(stroke.points, 32), 1.5);
        output.append(std::move(stroke));
    }
    return {};
}
bool fitsSketchedBox(const Path &points, const QRectF &bounds)
{
    const qreal shortSide = qMin(bounds.width(), bounds.height());
    const std::array<QPointF, 4> diagonals{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}};
    // Slightly different projection weights let the corners follow a tilted
    // side or a hand-drawn corner with a short rounded transition.
    for (const qreal verticalWeight : {0.66, 1.0, 1.5}) {
        std::array<QPointF, 4> corners;
        for (size_t i = 0; i < corners.size(); ++i) {
            const QPointF projection(diagonals[i].x() / bounds.width(),
                diagonals[i].y() * verticalWeight / bounds.height());
            corners[i] = *std::max_element(points.cbegin(), points.cend(), [&](QPointF a, QPointF b) {
                return QPointF::dotProduct(a, projection) < QPointF::dotProduct(b, projection);
            });
        }
        bool convex = true;
        for (size_t i = 0; i < corners.size(); ++i) {
            const QPointF incoming = corners[i] - corners[(i + 3) % 4];
            const QPointF outgoing = corners[(i + 1) % 4] - corners[i];
            if (magnitude(outgoing) < shortSide * 0.42 || cross(incoming, outgoing) <= 0
                || qAbs(QPointF::dotProduct(incoming, outgoing)) > magnitude(incoming) * magnitude(outgoing) * 0.65) {
                convex = false;
                break;
            }
        }
        if (!convex) continue;
        qreal squaredError = 0;
        for (const QPointF &point : points) {
            qreal distance = std::numeric_limits<qreal>::infinity();
            for (size_t i = 0; i < corners.size(); ++i)
                distance = qMin(distance, segmentDistance(point, corners[i], corners[(i + 1) % 4]));
            if (distance > shortSide * 0.18) {
                squaredError = std::numeric_limits<qreal>::infinity();
                break;
            }
            squaredError += distance * distance;
        }
        if (squaredError <= points.size() * std::pow(shortSide * 0.085, 2)) return true;
    }
    return false;
}

QString classifyOutline(const Path &input, QRectF &bounds, Path *recognizedOutline = nullptr)
{
    if (input.size() < 4) return {};
    Path points = resample(input);
    bounds = QPolygonF(points).boundingRect();
    if (bounds.width() < 30 || bounds.height() < 24 || bounds.width() * bounds.height() < 1200) return {};
    // Pen loops often overlap at their seam or end in a short lifted tail.
    // Trim only the ends, retain at least 80% of the perimeter, and still
    // require the entire remaining contour to fit a supported closed shape.
    qreal seam = magnitude(points.first() - points.last());
    int first = 0, last = points.size() - 1;
    for (int a = 0; a <= 19; ++a) for (int b = 108; b < points.size(); ++b) {
        if (b - a < 102) continue;
        const qreal distance = magnitude(points.at(a) - points.at(b));
        if (distance < seam) { seam = distance; first = a; last = b; }
    }
    if (first || last != points.size() - 1) points = resample(points.mid(first, last - first + 1));
    bounds = QPolygonF(points).boundingRect();
    const qreal shortSide = qMin(bounds.width(), bounds.height());
    const qreal gap = magnitude(points.first() - points.last());
    if (bounds.width() < 30 || bounds.height() < 24 || bounds.width() * bounds.height() < 1200
        || gap > qMin(34.0, shortSide * 0.62) || gap > pathLength(points) * 0.10) return {};
    qreal area = 0, sweep = 0, totalSweep = 0;
    QPointF previous = points.last() - bounds.center();
    for (const QPointF &point : points) {
        const QPointF current = point - bounds.center();
        const qreal turn = std::atan2(cross(previous, current), QPointF::dotProduct(previous, current));
        area += cross(previous, current);
        sweep += turn;
        totalSweep += qAbs(turn);
        previous = current;
    }
    if (qAbs(sweep) < 6.1 || totalSweep > 7.6) return {};
    const qreal ratio = qAbs(area) / (2 * bounds.width() * bounds.height());
    qreal rectError = 0, diamondError = 0, ovalError = 0;
    qreal rectMaximum = 0, diamondMaximum = 0, ovalMaximum = 0;
    std::array<bool, 4> corners{};
    const std::array<QPointF, 4> rectangleCorners{bounds.topLeft(), bounds.topRight(), bounds.bottomRight(), bounds.bottomLeft()};
    for (const QPointF &point : points) {
        const qreal x = 2 * (point.x() - bounds.center().x()) / bounds.width();
        const qreal y = 2 * (point.y() - bounds.center().y()) / bounds.height();
        const qreal rect = std::min({point.x() - bounds.left(), bounds.right() - point.x(),
                                    point.y() - bounds.top(), bounds.bottom() - point.y()}) / shortSide;
        const qreal diamond = qAbs(qAbs(x) + qAbs(y) - 1);
        const qreal oval = qAbs(std::hypot(x, y) - 1);
        rectError += rect * rect; diamondError += diamond * diamond; ovalError += oval * oval;
        rectMaximum = qMax(rectMaximum, rect); diamondMaximum = qMax(diamondMaximum, diamond);
        ovalMaximum = qMax(ovalMaximum, oval);
        for (size_t i = 0; i < corners.size(); ++i)
            corners[i] = corners[i] || magnitude(point - rectangleCorners[i]) <= shortSide * 0.19;
    }
    if (recognizedOutline) *recognizedOutline = points;
    const bool closeSeam = gap <= qMin(16.0, shortSide * 0.24);
    if (closeSeam && ratio >= 0.83 && ratio <= 1.03 && rectMaximum <= 0.17
        && std::sqrt(rectError / points.size()) <= 0.065
        && std::all_of(corners.cbegin(), corners.cend(), [](bool value) { return value; }))
        return QStringLiteral("process");
    if (closeSeam && ratio >= 0.37 && ratio <= 0.63 && diamondMaximum <= 0.34
        && std::sqrt(diamondError / points.size()) <= 0.13)
        return QStringLiteral("decision");
    const bool ovalSize = bounds.width() / bounds.height() < 6 && bounds.height() / bounds.width() < 6;
    const qreal ovalRms = std::sqrt(ovalError / points.size());
    if (closeSeam && ovalSize && ratio >= 0.65 && ratio <= 0.88 && ovalMaximum <= 0.22 && ovalRms <= 0.10)
        return QStringLiteral("terminal");

    // Keep the original winding checks for both fallbacks. A liberal box
    // seam still needs four convincing sides, and an oval needs a tighter
    // seam because there is no side fit to justify bridging a large gap.
    if (ratio >= 0.68 && fitsSketchedBox(points, bounds)) return QStringLiteral("process");
    if (ovalSize && gap <= qMin(22.0, shortSide * 0.30)
        && ratio >= 0.66 && ratio <= 0.88 && ovalMaximum <= 0.40 && ovalRms <= 0.18)
        return QStringLiteral("terminal");
    return {};
}
QVariantList variantIndexes(QList<int> indexes)
{
    std::sort(indexes.begin(), indexes.end());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
    QVariantList result;
    for (const int index : indexes) result.append(index);
    return result;
}
QList<Node> findNodes(const QList<Stroke> &strokes, bool &tooComplex, int nodeLimit = maximumNodes)
{
    QList<Node> nodes;
    QSet<int> used;
    QList<int> open;
    for (int i = 0; i < strokes.size(); ++i) {
        const Stroke &stroke = strokes.at(i);
        QRectF box;
        Path outline;
        const QString shape = classifyOutline(stroke.points, box, &outline);
        if (!shape.isEmpty()) { nodes.append({{}, shape, box, {i}, outline}); used.insert(i); }
        else if (stroke.length >= 24 && stroke.points.size() >= 2
                 && magnitude(stroke.points.first() - stroke.points.last()) >= 20) open.append(i);
    }
    if (open.size() > maximumCandidates || nodes.size() > maximumNodes * 2) { tooComplex = true; return {}; }
    std::sort(open.begin(), open.end(), [&](int a, int b) { return strokes.at(a).length > strokes.at(b).length; });
    for (const int start : open) {
        if (used.contains(start)) continue;
        Path joined = strokes.at(start).points;
        QList<int> parts{start};
        for (int depth = 1; depth < 4; ++depth) {
            int nearest = -1;
            bool reverse = false;
            qreal best = std::numeric_limits<qreal>::infinity();
            qreal second = best;
            for (const int other : open) {
                if (used.contains(other) || parts.contains(other)) continue;
                const Path &candidate = strokes.at(other).points;
                const qreal first = magnitude(joined.last() - candidate.first());
                const qreal last = magnitude(joined.last() - candidate.last());
                const qreal distance = qMin(first, last);
                if (distance < best) { second = best; best = distance; nearest = other; reverse = last < first; }
                else second = qMin(second, distance);
            }
            if (nearest < 0 || best > joinDistance || second - best < 2) break;
            Path next = strokes.at(nearest).points;
            if (reverse) std::reverse(next.begin(), next.end());
            joined.append(next);
            parts.append(nearest);
            if (magnitude(joined.last() - joined.first()) <= joinDistance) {
                QRectF box;
                Path outline;
                const QString shape = classifyOutline(joined, box, &outline);
                if (!shape.isEmpty()) {
                    nodes.append({{}, shape, box, parts, outline});
                    for (const int part : parts) used.insert(part);
                }
                break;
            }
        }
    }
    // A small closed letter inside a larger shape is label ink, not another node.
    // Substantial overlap between independent shapes is ambiguous and left untouched.
    QSet<int> excluded;
    for (int i = 0; i < nodes.size(); ++i) for (int j = i + 1; j < nodes.size(); ++j) {
        const QRectF a = nodes.at(i).bounds, b = nodes.at(j).bounds;
        const qreal aArea = a.width() * a.height(), bArea = b.width() * b.height();
        if (a.contains(b) && bArea < aArea * 0.45) excluded.insert(j);
        else if (b.contains(a) && aArea < bArea * 0.45) excluded.insert(i);
        else {
            const QRectF overlap = a.intersected(b);
            if (overlap.width() * overlap.height() > qMin(aArea, bArea) * 0.15) {
                excluded.insert(i); excluded.insert(j);
            }
        }
    }
    QList<Node> result;
    for (int i = 0; i < nodes.size(); ++i) if (!excluded.contains(i)) {
        Node node = nodes.at(i);
        node.id = QStringLiteral("node-%1").arg(result.size() + 1);
        result.append(node);
    }
    if (result.size() > nodeLimit) { tooComplex = true; return {}; }
    return result;
}
Path nodeOutline(const Node &node)
{
    // Attach to the recognized ink contour, including an imperfect/skewed
    // diamond, rather than to the idealized shape used by the final renderer.
    if (!node.outline.isEmpty()) {
        Path closed = node.outline;
        closed.append(closed.first());
        return closed;
    }
    const QRectF &r = node.bounds;
    if (node.shape == QStringLiteral("process"))
        return {r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft(), r.topLeft()};
    if (node.shape == QStringLiteral("decision"))
        return {{r.center().x(), r.top()}, {r.right(), r.center().y()}, {r.center().x(), r.bottom()},
                {r.left(), r.center().y()}, {r.center().x(), r.top()}};
    Path result;
    for (int i = 0; i <= 40; ++i) {
        const qreal angle = i * 6.283185307179586 / 40;
        result.append(r.center() + QPointF(std::cos(angle) * r.width() / 2, std::sin(angle) * r.height() / 2));
    }
    return result;
}
qreal distanceToOutline(const Node &node, QPointF point)
{
    const Path outline = nodeOutline(node);
    qreal result = std::numeric_limits<qreal>::infinity();
    for (qsizetype i = 1; i < outline.size(); ++i)
        result = qMin(result, segmentDistance(point, outline.at(i - 1), outline.at(i)));
    return result;
}
int nearestNode(const QList<Node> &nodes, QPointF point)
{
    qreal best = std::numeric_limits<qreal>::infinity(), second = best;
    int selected = -1;
    for (int i = 0; i < nodes.size(); ++i) {
        // The additional 5 px preserve the ambiguity margin even when a
        // competing contour is just outside the 18 px attachment radius.
        if (!nodes.at(i).bounds.adjusted(-23, -23, 23, 23).contains(point)) continue;
        const qreal distance = distanceToOutline(nodes.at(i), point);
        if (distance < best) { second = best; best = distance; selected = i; }
        else second = qMin(second, distance);
    }
    return selected >= 0 && best <= 18 && second - best >= 5 ? selected : -1;
}
bool insideNode(const Node &node, const Path &points)
{
    if (points.isEmpty()) return false;
    if (node.existing) {
        const QPolygonF contour(nodeOutline(node));
        for (const QPointF &point : points)
            if (!contour.containsPoint(point, Qt::OddEvenFill) || distanceToOutline(node, point) < 3) return false;
        return true;
    }
    for (const QPointF &point : points) {
        const qreal x = 2 * (point.x() - node.bounds.center().x()) / node.bounds.width();
        const qreal y = 2 * (point.y() - node.bounds.center().y()) / node.bounds.height();
        if (node.shape == QStringLiteral("process")) {
            if (!node.bounds.adjusted(3, 3, -3, -3).contains(point)) return false;
        } else if (node.shape == QStringLiteral("decision")) {
            if (qAbs(x) + qAbs(y) >= 0.86) return false;
        } else if (std::hypot(x, y) >= 0.89) return false;
    }
    return true;
}
bool exactConnectorPath(const Stroke &stroke, Path &path, bool &orthogonal)
{
    if (stroke.points.size() < 2 || stroke.length < 18) return false;
    path = simplified(resample(stroke.points, 64), 3);
    if (path.size() < 2 || path.size() > 5) return false;
    const qreal chord = magnitude(path.last() - path.first());
    if (chord < 18 || stroke.length > chord * 1.85) return false;
    orthogonal = path.size() > 2;
    if (path.size() == 2) {
        if (stroke.length > chord * 1.12) return false;
        for (const QPointF &point : stroke.points)
            if (segmentDistance(point, path.first(), path.last()) > 3.5) return false;
        return true;
    }
    int previousAxis = 0;
    for (qsizetype i = 1; i < path.size(); ++i) {
        const QPointF delta = path.at(i) - path.at(i - 1);
        const int axis = qAbs(delta.y()) < qAbs(delta.x()) * 0.20 ? 1
            : qAbs(delta.x()) < qAbs(delta.y()) * 0.20 ? 2 : 0;
        if (!axis || axis == previousAxis || magnitude(delta) < 8) return false;
        previousAxis = axis;
    }
    return true;
}
bool connectorPath(const Stroke &stroke, Path &path, bool &orthogonal)
{
    if (exactConnectorPath(stroke, path, orthogonal)) return true;
    if (stroke.points.size() < 2 || stroke.length < 18) return false;
    const Path points = resample(stroke.points, 64);
    qreal chord = 0;
    int first = 0, last = 63;
    // A short reversal when putting down/lifting the pen is not a second
    // connector. Keep the long central shaft; do not straighten broad curves.
    for (int a = 0; a <= 16; ++a) for (int b = 47; b < 64; ++b) {
        if (b - a < 40) continue;
        const qreal distance = magnitude(points.at(a) - points.at(b));
        if (distance > chord) { chord = distance; first = a; last = b; }
    }
    if (chord < 18) return false;
    const Path shaft = points.mid(first, last - first + 1);
    if (pathLength(shaft) > chord * 1.4) return false;
    const qreal tolerance = qMin(12.0, qMax(5.0, chord * 0.12));
    for (const QPointF &point : shaft)
        if (segmentDistance(point, shaft.first(), shaft.last()) > tolerance) return false;
    path = {shaft.first(), shaft.last()};
    orthogonal = false;
    return true;
}

// A hand-drawn closed triangle may retrace its base, unlike a three-vertex V.
// Check its complete outline against two wings and their base, so surrounding
// handwriting or an arbitrary loop cannot provide an inferred arrow direction.
bool triangularHead(const Path &input, QPointF nearTip, QPointF direction,
                    qreal shaftLength, bool shaftEntry, QPointF &tip, bool flattened = false)
{
    if (input.size() < 3) return false;
    const Path points = resample(input, 64);
    const QRectF bounds = QPolygonF(points).boundingRect();
    if (bounds.width() > 64 || bounds.height() > 64 || pathLength(points) > 180) return false;
    int front = 0, left = 0, right = 0;
    for (int i = 1; i < points.size(); ++i) {
        if (QPointF::dotProduct(points.at(i), direction) > QPointF::dotProduct(points.at(front), direction)) front = i;
        if (cross(direction, points.at(i)) < cross(direction, points.at(left))) left = i;
        if (cross(direction, points.at(i)) > cross(direction, points.at(right))) right = i;
    }
    tip = flattened ? points.first() : points.at(front);
    if (magnitude(tip - nearTip) > qMin(16.0, qMax(9.0, shaftLength * 0.3))) return false;
    const QPointF a = points.at(left), b = points.at(right);
    const qreal firstLength = magnitude(a - tip), secondLength = magnitude(b - tip);
    const qreal firstSide = cross(direction, a - tip), secondSide = cross(direction, b - tip);
    if (firstSide > -4 || secondSide < 4 || firstLength < 7 || secondLength < 7
        || qMax(firstLength, secondLength) > qMin(52.0, shaftLength * 0.8)
        || qMax(firstLength, secondLength) > 2.5 * qMin(firstLength, secondLength)) return false;
    qreal totalDepth = 0;
    for (const QPointF wingEnd : {a, b}) {
        const qreal behind = -QPointF::dotProduct(wingEnd - tip, direction);
        if (behind < (flattened ? 1.0 : 4.0) || behind > magnitude(wingEnd - tip) * 0.97) return false;
        totalDepth += behind;
    }
    if (flattened && (totalDepth < 8 || qMax(firstLength, secondLength) > 32)) return false;
    qreal enclosedArea = 0;
    QPointF previous = points.last();
    for (const QPointF &point : points) { enclosedArea += cross(previous, point); previous = point; }
    // A small circle can fit an absolute pixel tolerance but encloses about
    // three times the triangle's area. Preserve such loops as ordinary ink.
    const qreal triangleArea = qAbs(cross(a - tip, b - tip));
    if (qAbs(enclosedArea) > triangleArea * 1.65) return false;
    qreal error = 0;
    int boundaryPoints = 0;
    std::array<bool, 3> covered{};
    const std::array<QPair<QPointF, QPointF>, 3> sides{{{tip, a}, {tip, b}, {a, b}}};
    for (int i = 0; i < points.size(); ++i) {
        const qreal distance = qMin(segmentDistance(points.at(i), tip, a),
            qMin(segmentDistance(points.at(i), tip, b), segmentDistance(points.at(i), a, b)));
        // Only a one-stroke arrow may enter its head through the interior.
        if (shaftEntry && i < 13 && distance > 6) continue;
        if (distance > (shaftEntry ? 7.0 : 6.0)) return false;
        error += distance * distance;
        ++boundaryPoints;
        for (size_t edge = 0; edge < sides.size(); ++edge) {
            const QPointF delta = sides[edge].second - sides[edge].first;
            const qreal fraction = QPointF::dotProduct(points.at(i) - sides[edge].first, delta)
                / QPointF::dotProduct(delta, delta);
            if (fraction > 0.2 && fraction < 0.8
                && segmentDistance(points.at(i), sides[edge].first, sides[edge].second) <= 3.5) covered[edge] = true;
        }
    }
    // A closed head may continue over a wing after completing its loop.
    // Require evidence for every triangle side instead of equal endpoints.
    if ((!shaftEntry || flattened)
        && !std::all_of(covered.cbegin(), covered.cend(), [](bool value) { return value; })) return false;
    return boundaryPoints >= 51 && std::sqrt(error / boundaryPoints) <= 3.5;
}

bool continuousStraightArrow(const Stroke &stroke, Path &path)
{
    if (stroke.points.size() < 5 || stroke.length < 40 || stroke.length > 900) return false;
    const Path points = resample(stroke.points, 128);
    // Establish direction from the initial shaft, before examining the head.
    int initial = 1;
    while (initial < 32 && magnitude(points.at(initial) - points.first()) < 18) ++initial;
    QPointF direction = points.at(initial) - points.first();
    if (magnitude(direction) < 18) return false;
    direction /= magnitude(direction);
    int front = initial;
    for (int i = initial + 1; i < points.size(); ++i)
        if (QPointF::dotProduct(points.at(i) - points.first(), direction)
            > QPointF::dotProduct(points.at(front) - points.first(), direction)) front = i;
    const QPointF apex = points.at(front);
    const qreal length = magnitude(apex - points.first());
    if (length < 30) return false;
    direction = (apex - points.first()) / length;
    int entry = initial;
    qreal forward = 0;
    const qreal tolerance = qMin(12.0, qMax(6.0, length * 0.12));
    for (; entry < points.size(); ++entry) {
        const qreal position = QPointF::dotProduct(points.at(entry) - points.first(), direction);
        if (segmentDistance(points.at(entry), points.first(), apex) > tolerance || position < forward - 6) break;
        forward = qMax(forward, position);
    }
    if (entry >= points.size() - 8 || forward < length * 0.4) return false;
    QPointF tip;
    if (!triangularHead(points.mid(qMax(0, entry - 1)), apex, direction, length, true, tip)) return false;
    path = {points.first(), tip};
    return true;
}

bool fitElbowShaft(const Path &samples, Path &path)
{
    const int count = samples.size();
    if (count < 4) return false;
    const QPointF start = samples.first(), tip = samples.last();
    const qreal length = pathLength(samples);
    qreal bestError = std::numeric_limits<qreal>::infinity();
    const auto axis = [](QPointF delta) {
        if (qAbs(delta.y()) <= qAbs(delta.x()) * .45) return 1;
        if (qAbs(delta.x()) <= qAbs(delta.y()) * .45) return 2;
        return 0;
    };
    for (int corner = 1; corner < count - 1; ++corner) {
        const QPointF bend = samples.at(corner);
        const qreal firstLength = magnitude(bend - start), lastLength = magnitude(tip - bend);
        const int firstAxis = axis(bend - start), lastAxis = axis(tip - bend);
        if (firstLength < 19 || lastLength < 19 || !firstAxis || !lastAxis || firstAxis == lastAxis
            || length > (firstLength + lastLength) * 1.32) continue;
        qreal error = 0;
        bool withinTolerance = true;
        for (int i = 1; i < count - 1; ++i) {
            const qreal distance = i <= corner ? segmentDistance(samples.at(i), start, bend)
                                                : segmentDistance(samples.at(i), bend, tip);
            const qreal tolerance = qBound(6.0, (i <= corner ? firstLength : lastLength) * .16, 14.0);
            if (distance > tolerance) { withinTolerance = false; break; }
            error += distance * distance;
        }
        if (withinTolerance && error < bestError) {
            bestError = error;
            path = {start, bend, tip};
        }
    }
    return !path.isEmpty();
}

bool continuousArrow(const Stroke &stroke, Path &path, bool &orthogonal)
{
    if (continuousStraightArrow(stroke, path)) {
        orthogonal = false;
        return true;
    }
    if (stroke.points.size() < 6 || stroke.length < 55 || stroke.length > 900) return false;
    const Path points = resample(stroke.points);
    const Path vertices = simplified(points, 3.0);
    if (vertices.size() < 5 || vertices.size() > 14) return false;
    Path found;
    for (int v = 2; v < vertices.size() - 2; ++v) {
        const int split = points.indexOf(vertices.at(v));
        if (split < 25 || split > 119) continue;
        const Path shaft = points.first(split + 1);
        const Path tail = points.sliced(split);
        if (pathLength(tail) > 155) continue;
        Path candidate;
        if (!fitElbowShaft(resample(shaft, 32), candidate)) continue;
        const QPointF end = candidate.last();
        const QPointF segment = end - candidate.at(candidate.size() - 2);
        QPointF tip;
        const qreal length = pathLength(candidate);
        if (!triangularHead(tail, end, segment / magnitude(segment), length, true, tip)) {
            // The sampled tip can turn down at the end of a freehand elbow.
            // Use this last portion only for a compact head with three traced
            // sides, so a bend followed by a stray flick never gives direction.
            int previous = split - 1;
            while (previous > 0 && magnitude(end - points.at(previous)) < 16) --previous;
            const QPointF tangent = end - points.at(previous);
            if (magnitude(tangent) < 10
                || !triangularHead(tail, end, tangent / magnitude(tangent), length, true, tip, true))
                continue;
        }
        if (!found.isEmpty() && magnitude(tip - found.last()) > 7) return false;
        if (found.isEmpty()) {
            candidate.last() = tip;
            found = candidate;
        }
    }
    if (found.isEmpty()) return false;
    path = found;
    orthogonal = true;
    return true;
}

bool wing(QPointF tip, QPointF end, QPointF shaftDirection, qreal shaftLength, qreal &side)
{
    const QPointF delta = end - tip;
    const qreal distance = magnitude(delta);
    if (distance < 4 || distance > qMin(28.0, shaftLength * 0.45)) return false;
    const qreal behind = -QPointF::dotProduct(delta, shaftDirection);
    side = cross(shaftDirection, delta);
    return behind > distance * 0.32 && behind < distance * 0.94 && qAbs(side) >= 2.5;
}
struct ArrowHead { QList<int> indexes; QPointF tip; };

bool headWithStem(const Stroke &stroke, QPointF shaftEnd, QPointF direction,
                  qreal shaftLength, QPointF &tip)
{
    const Path points = resample(stroke.points, 64);
    const QPointF start = points.first();
    if (magnitude(start - shaftEnd) > 7) return false;
    const Path vertices = simplified(points, 3);
    if (vertices.size() < 5 || vertices.size() > 8) return false;
    const QPointF end = vertices.at(1);
    const QPointF stem = end - start;
    const qreal length = magnitude(stem);
    if (length < 10 || length > qMin(36.0, shaftLength * .65)
        || QPointF::dotProduct(stem, direction) < length * .85) return false;
    return triangularHead(points.sliced(points.indexOf(end)), end, direction,
                          shaftLength + length, false, tip);
}

ArrowHead arrowHead(const QList<Stroke> &strokes, const QSet<int> &used, int shaftIndex,
                    QPointF tip, QPointF direction, qreal shaftLength)
{
    struct Half { int index; qreal side; qreal distance; };
    QList<ArrowHead> complete;
    QList<Half> halves;
    for (int i = 0; i < strokes.size(); ++i) {
        const Stroke &stroke = strokes.at(i);
        if (i == shaftIndex || used.contains(i) || stroke.length < 6 || stroke.length > 180 || stroke.points.size() < 2
            || !stroke.bounds.adjusted(-16, -16, 16, 16).contains(tip)) continue;
        const Path &shape = stroke.headShape;
        QPointF triangleTip;
        if (triangularHead(stroke.points, tip, direction, shaftLength, false, triangleTip)) {
            complete.append(ArrowHead{{i}, tip});
        } else if (headWithStem(stroke, tip, direction, shaftLength, triangleTip)) {
            complete.append(ArrowHead{{i}, triangleTip});
        } else if (shape.size() == 3 && magnitude(shape.at(1) - tip) <= 6) {
            qreal first = 0, second = 0;
            if (wing(shape.at(1), shape.first(), direction, shaftLength, first)
                && wing(shape.at(1), shape.last(), direction, shaftLength, second) && first * second < 0)
                complete.append(ArrowHead{{i}, tip});
        } else if (shape.size() == 2) {
            const bool reverse = magnitude(shape.last() - tip) < magnitude(shape.first() - tip);
            const QPointF vertex = reverse ? shape.last() : shape.first();
            const QPointF end = reverse ? shape.first() : shape.last();
            qreal side = 0;
            if (magnitude(vertex - tip) <= 6 && wing(vertex, end, direction, shaftLength, side))
                halves.append({i, side, magnitude(end - vertex)});
        }
        // Several competing heads are ambiguous. Bound pair enumeration even
        // when thousands of tiny strokes accumulate around one endpoint.
        if (complete.size() > 1 || halves.size() > 8) return {};
    }
    for (int i = 0; i < halves.size(); ++i) for (int j = i + 1; j < halves.size(); ++j) {
        if (halves.at(i).side * halves.at(j).side < 0
            && qMax(halves.at(i).distance, halves.at(j).distance) < 1.8 * qMin(halves.at(i).distance, halves.at(j).distance))
            complete.append(ArrowHead{{halves.at(i).index, halves.at(j).index}, tip});
    }
    return complete.size() == 1 ? complete.first() : ArrowHead{{}, tip};
}
QList<Edge> findEdges(const QList<Stroke> &strokes, const QList<Node> &nodes, QSet<int> &used, bool &ambiguous,
                      bool &tooComplex, QSet<QPair<int, int>> pairs = {}, int edgeLimit = maximumEdges,
                      QSet<int> *duplicates = nullptr)
{
    QList<Edge> edges;
    const QSet<QPair<int, int>> existingPairs = pairs;
    for (int i = 0; i < strokes.size(); ++i) {
        if (used.contains(i)) continue;
        Path path;
        bool orthogonal = false;
        // Examine a proved one-stroke head before simplifying the shaft;
        // otherwise a small triangle can disappear inside its curve tolerance.
        const bool integratedHead = continuousArrow(strokes.at(i), path, orthogonal);
        if (!integratedHead && !connectorPath(strokes.at(i), path, orthogonal)) continue;
        const auto potentiallyAttached = [&nodes](QPointF point) {
            return std::any_of(nodes.cbegin(), nodes.cend(), [point](const Node &node) {
                return node.bounds.adjusted(-54, -54, 54, 54).contains(point);
            });
        };
        if (!potentiallyAttached(path.first()) || !potentiallyAttached(path.last())) continue;
        const qreal shaftLength = pathLength(path);
        const QPointF end = path.last() - path.at(path.size() - 2), start = path.first() - path.at(1);
        const ArrowHead lastHead = integratedHead ? ArrowHead{{i}, path.last()}
            : arrowHead(strokes, used, i, path.last(), end / magnitude(end), shaftLength);
        const ArrowHead firstHead = integratedHead ? ArrowHead{{}, path.first()}
            : arrowHead(strokes, used, i, path.first(), start / magnitude(start), shaftLength);
        QList<int> towardLast = lastHead.indexes, towardFirst = firstHead.indexes;
        if (!towardLast.isEmpty()) path.last() = lastHead.tip;
        if (!towardFirst.isEmpty()) path.first() = firstHead.tip;
        const int first = nearestNode(nodes, path.first()), last = nearestNode(nodes, path.last());
        if (first < 0 || last < 0 || first == last) continue;
        // A connector crossing through another node cannot be assigned safely.
        bool crosses = false;
        const Path sample = resample(path, 64);
        for (int n = 0; n < nodes.size(); ++n) if (n != first && n != last) {
            for (const QPointF &point : sample) if (insideNode(nodes.at(n), {point})) { crosses = true; break; }
        }
        if (crosses) { ambiguous = true; continue; }
        if (towardLast.isEmpty() && towardFirst.isEmpty()) { ambiguous = true; continue; }
        // A bidirectional drawing may add only the missing return connection.
        // Preserve the already represented head as ink; consume the shared
        // shaft and only the head that proves the newly accepted direction.
        const bool knownLast = !towardLast.isEmpty() && existingPairs.contains({first, last});
        const bool knownFirst = !towardFirst.isEmpty() && existingPairs.contains({last, first});
        if (knownLast || knownFirst) {
            ambiguous = true;
            if (duplicates) {
                if (knownLast) for (const int index : towardLast) duplicates->insert(index);
                if (knownFirst) for (const int index : towardFirst) duplicates->insert(index);
            }
            if (knownLast) towardLast.clear();
            if (knownFirst) towardFirst.clear();
            if (towardLast.isEmpty() && towardFirst.isEmpty()) {
                if (duplicates) duplicates->insert(i);
                continue;
            }
        }
        const bool duplicate = (!towardLast.isEmpty() && pairs.contains({first, last}))
            || (!towardFirst.isEmpty() && pairs.contains({last, first}));
        if (duplicate) {
            if (duplicates) {
                duplicates->insert(i);
                for (const int index : towardLast) duplicates->insert(index);
                for (const int index : towardFirst) duplicates->insert(index);
            }
            ambiguous = true;
            continue;
        }
        if (edges.size() + !towardLast.isEmpty() + !towardFirst.isEmpty() > edgeLimit) { tooComplex = true; return {}; }
        QList<int> indexes{i};
        indexes.append(towardLast); indexes.append(towardFirst);
        for (const int part : indexes) used.insert(part);
        if (!towardLast.isEmpty()) {
            pairs.insert({first, last});
            edges.append({QStringLiteral("edge-%1").arg(edges.size() + 1), first, last, orthogonal, path, indexes});
        }
        if (!towardFirst.isEmpty()) {
            pairs.insert({last, first});
            Path reversed = path; std::reverse(reversed.begin(), reversed.end());
            edges.append({QStringLiteral("edge-%1").arg(edges.size() + 1), last, first, orthogonal, reversed, indexes});
        }
    }
    return edges;
}
qreal edgeLabelDistance(const Edge &edge, const Stroke &stroke)
{
    if (stroke.points.isEmpty() || stroke.bounds.width() > 120 || stroke.bounds.height() > 45) return -1;
    const Path &path = edge.points;
    const QPointF center = stroke.bounds.center();
    if (magnitude(center - path.first()) < 20 || magnitude(center - path.last()) < 20) return -1;
    qreal distance = std::numeric_limits<qreal>::infinity();
    for (qsizetype i = 1; i < path.size(); ++i)
        distance = qMin(distance, segmentDistance(center, path.at(i - 1), path.at(i)));
    if (distance > 22 || distance < 3) return -1;
    return distance;
}

QRectF normalizedRect(const QVariantMap &data, const QRectF &parent)
{
    return {parent.x() + data.value(QStringLiteral("x")).toDouble() * parent.width(),
            parent.y() + data.value(QStringLiteral("y")).toDouble() * parent.height(),
            data.value(QStringLiteral("width")).toDouble() * parent.width(),
            data.value(QStringLiteral("height")).toDouble() * parent.height()};
}

// These are the contours used by diagramitem.cpp. In particular, the saved
// terminal is a rounded rectangle, not the freehand ellipse that produced it.
Path existingOutline(const Node &node)
{
    const QRectF &r = node.bounds;
    QPainterPath path;
    if (node.shape == QStringLiteral("terminal")) {
        const qreal radius = qMin(r.height() * .5, r.width() * .23);
        path.addRoundedRect(r, radius, radius);
    } else if (node.shape == QStringLiteral("decision")) {
        path.addPolygon(QPolygonF{{r.center().x(), r.top()}, {r.right(), r.center().y()},
            {r.center().x(), r.bottom()}, {r.left(), r.center().y()}});
    } else if (node.shape == QStringLiteral("data")) {
        const qreal slant = qMin(r.width() * .17, r.height() * .40);
        path.addPolygon(QPolygonF{{r.left() + slant, r.top()}, r.topRight(),
            {r.right() - slant, r.bottom()}, r.bottomLeft()});
    } else {
        path.addRect(r); // process and note (whose border is merely dashed)
    }
    path.closeSubpath();
    return path.toFillPolygon();
}

QTransform rotationAround(QPointF center, qreal angle)
{
    QTransform rotation;
    rotation.translate(center.x(), center.y());
    rotation.rotate(angle);
    rotation.translate(-center.x(), -center.y());
    return rotation;
}

QString unusedId(const QString &prefix, QSet<QString> &ids)
{
    int suffix = 1;
    QString id;
    do { id = prefix + QString::number(suffix++); } while (ids.contains(id));
    ids.insert(id);
    return id;
}

QVariantMap recognizeExtension(const QVariantList &input, const QVariantMap &existing)
{
    QVariantMap result{{QStringLiteral("diagram"), QVariantMap{}}, {QStringLiteral("candidates"), QVariantList{}},
        {QStringLiteral("labelGroups"), QVariantList{}}, {QStringLiteral("unassignedIndexes"), QVariantList{}},
        {QStringLiteral("warnings"), QStringList{}}, {QStringLiteral("error"), QString{}},
        {QStringLiteral("extendsExisting"), true}, {QStringLiteral("hasChanges"), false},
        {QStringLiteral("addedNodeCount"), 0}, {QStringLiteral("addedEdgeCount"), 0}};
    const auto fail = [&](const QString &message) {
        result.insert(QStringLiteral("error"), message);
        QVariantList remaining;
        for (int i = 0; i < input.size(); ++i) remaining.append(i);
        result.insert(QStringLiteral("unassignedIndexes"), remaining);
        return result;
    };
    if (existing.value(QStringLiteral("type")).toString() != QStringLiteral("diagram")
        || PageContent::validatedItem(existing).isEmpty())
        return fail(QObject::tr("Il diagramma esistente non è valido. Riapri il foglio e riprova."));
    QList<Stroke> strokes;
    const QString invalid = readStrokes(input, strokes);
    if (!invalid.isEmpty()) return fail(invalid);
    const QRectF page(0, 0, pageWidth, pageHeight);
    const QRectF originalBounds = normalizedRect(existing, page);
    const qreal rotation = existing.value(QStringLiteral("rotation")).toDouble();
    const QTransform toPage = rotationAround(originalBounds.center(), rotation);
    const QTransform fromPage = rotationAround(originalBounds.center(), -rotation);
    for (Stroke &stroke : strokes) {
        for (QPointF &point : stroke.points) point = fromPage.map(point);
        for (QPointF &point : stroke.headShape) point = fromPage.map(point);
        stroke.bounds = QPolygonF(stroke.points).boundingRect();
    }
    const QVariantList oldNodes = existing.value(QStringLiteral("nodes")).toList();
    const QVariantList oldEdges = existing.value(QStringLiteral("edges")).toList();
    QList<Node> nodes;
    QVariantList existingNodeIds, existingEdgeIds;
    QSet<QString> ids;
    QMap<QString, int> nodeIndexes;
    for (const QVariant &entry : oldNodes) {
        const QVariantMap data = entry.toMap();
        Node node{data.value(QStringLiteral("id")).toString(), data.value(QStringLiteral("shape")).toString(),
            normalizedRect(data, originalBounds), {}, {}, true};
        node.outline = existingOutline(node);
        nodeIndexes.insert(node.id, nodes.size());
        ids.insert(node.id); existingNodeIds.append(node.id);
        nodes.append(std::move(node));
    }
    QSet<QPair<int, int>> existingPairs;
    for (const QVariant &entry : oldEdges) {
        const QVariantMap edge = entry.toMap();
        const QString id = edge.value(QStringLiteral("id")).toString();
        ids.insert(id); existingEdgeIds.append(id);
        existingPairs.insert({nodeIndexes.value(edge.value(QStringLiteral("from")).toString()),
                              nodeIndexes.value(edge.value(QStringLiteral("to")).toString())});
    }
    result.insert(QStringLiteral("existingNodeIds"), existingNodeIds);
    result.insert(QStringLiteral("existingEdgeIds"), existingEdgeIds);

    // Retained originals are still real ink. Exclude outlines matching saved
    // nodes from recognition, but never put their indexes in removal candidates.
    QSet<int> excluded;
    QList<Stroke> geometry = strokes;
    for (int i = 0; i < strokes.size(); ++i) {
        const Stroke &stroke = strokes.at(i);
        for (const Node &node : nodes) {
            if (stroke.points.isEmpty() || stroke.length < 30
                || stroke.bounds.width() < node.bounds.width() * .5
                || stroke.bounds.height() < node.bounds.height() * .5
                || !node.bounds.adjusted(-12, -12, 12, 12).contains(stroke.bounds)) continue;
            const qsizetype requiredNearby = (stroke.points.size() * 9 + 9) / 10;
            const qsizetype allowedMisses = stroke.points.size() - requiredNearby;
            qsizetype nearby = 0, missed = 0;
            for (const QPointF &point : stroke.points) {
                if (distanceToOutline(node, point) <= 12) ++nearby;
                else if (++missed > allowedMisses) break;
            }
            if (nearby >= requiredNearby) {
                excluded.insert(i); geometry[i] = {}; break;
            }
        }
    }
    bool tooComplex = false;
    const QList<Node> found = findNodes(geometry, tooComplex, maximumNodes * 2);
    if (tooComplex) return fail(QObject::tr("Il disegno è troppo complesso. Seleziona un diagramma più piccolo."));
    QRectF bounds = originalBounds;
    for (Node node : found) {
        bool overlaps = false;
        for (int i = 0; i < oldNodes.size(); ++i) {
            const QRectF old = nodes.at(i).bounds;
            const QRectF overlap = old.intersected(node.bounds);
            if (overlap.width() * overlap.height() > .15 * qMin(old.width() * old.height(),
                    node.bounds.width() * node.bounds.height())) { overlaps = true; break; }
        }
        if (overlaps) {
            for (const int index : node.indexes) excluded.insert(index);
            continue;
        }
        node.id = unusedId(QStringLiteral("node-"), ids);
        bounds = bounds.united(node.bounds.adjusted(-3, -3, 3, 3));
        nodes.append(std::move(node));
    }
    if (nodes.size() > maximumNodes)
        return fail(QObject::tr("Il diagramma supera il limite di 64 forme. Seleziona una parte più piccola."));
    // A rotation changes the page-space center when the local container grows.
    // Rebase around that transformed center so every old node stays in place.
    if (qFuzzyIsNull(rotation)) bounds = bounds.intersected(page);
    const QPointF newCenter = toPage.map(bounds.center());
    const QRectF pageBounds(newCenter - QPointF(bounds.width() / 2, bounds.height() / 2), bounds.size());
    const QRectF rotatedBounds = rotationAround(newCenter, rotation).mapRect(pageBounds);
    const QRectF pageTolerance = page.adjusted(-.00001, -.00001, .00001, .00001);
    if (!pageTolerance.contains(pageBounds) || !pageTolerance.contains(rotatedBounds))
        return fail(QObject::tr("Il diagramma esteso supera il bordo della pagina. Sposta le nuove forme verso il centro e riprova."));

    QSet<int> used;
    for (int i = oldNodes.size(); i < nodes.size(); ++i)
        for (const int index : nodes.at(i).indexes) used.insert(index);
    QSet<int> edgeUsed = used;
    edgeUsed.unite(excluded);
    bool ambiguous = false;
    QList<Edge> edges = findEdges(strokes, nodes, edgeUsed, ambiguous, tooComplex, existingPairs,
        maximumEdges - oldEdges.size(), &excluded);
    if (tooComplex)
        return fail(QObject::tr("Il diagramma supera il limite di 128 collegamenti. Seleziona una parte più piccola."));
    QVariantList nodeData, edgeData = oldEdges, candidates;
    for (int i = 0; i < nodes.size(); ++i) {
        const Node &node = nodes.at(i);
        QVariantMap data = i < oldNodes.size() ? oldNodes.at(i).toMap()
            : QVariantMap{{QStringLiteral("id"), node.id}, {QStringLiteral("shape"), node.shape},
                          {QStringLiteral("text"), QString{}}};
        // Preserve the exact stored numbers for an arrow-only addition.
        if (i >= oldNodes.size() || bounds != originalBounds) {
            data.insert(QStringLiteral("x"), (node.bounds.x() - bounds.x()) / bounds.width());
            data.insert(QStringLiteral("y"), (node.bounds.y() - bounds.y()) / bounds.height());
            data.insert(QStringLiteral("width"), node.bounds.width() / bounds.width());
            data.insert(QStringLiteral("height"), node.bounds.height() / bounds.height());
        }
        nodeData.append(data);
        if (i >= oldNodes.size()) candidates.append(QVariantMap{{QStringLiteral("id"), node.id},
            {QStringLiteral("kind"), QStringLiteral("node")}, {QStringLiteral("indexes"), variantIndexes(node.indexes)}});
    }
    for (Edge &edge : edges) {
        edge.id = unusedId(QStringLiteral("edge-"), ids);
        QVariantMap data{{QStringLiteral("id"), edge.id}, {QStringLiteral("from"), nodes.at(edge.from).id},
            {QStringLiteral("to"), nodes.at(edge.to).id}, {QStringLiteral("text"), QString{}}};
        if (edge.orthogonal) data.insert(QStringLiteral("route"), QStringLiteral("orthogonal"));
        edgeData.append(data);
        candidates.append(QVariantMap{{QStringLiteral("id"), edge.id}, {QStringLiteral("kind"), QStringLiteral("edge")},
            {QStringLiteral("indexes"), variantIndexes(edge.indexes)}});
        for (const int index : edge.indexes) used.insert(index);
    }
    QMap<QString, QList<int>> groups;
    QMap<QString, QString> kinds;
    QSet<int> labels;
    for (int i = 0; i < strokes.size(); ++i) {
        if (used.contains(i) || excluded.contains(i) || strokes.at(i).points.isEmpty()
            || (strokes.at(i).tool != QStringLiteral("pen") && strokes.at(i).tool != QStringLiteral("pencil")
                && strokes.at(i).tool != QStringLiteral("marker"))) continue;
        QString id, kind;
        bool inExisting = false;
        for (const Node &node : nodes) if (insideNode(node, strokes.at(i).points)) {
            if (node.existing) { inExisting = true; break; }
            if (!id.isEmpty()) { id.clear(); break; }
            id = node.id; kind = QStringLiteral("node");
        }
        if (inExisting) continue; // Existing text is never re-read or overwritten.
        if (id.isEmpty()) {
            qreal best = std::numeric_limits<qreal>::infinity(), second = best;
            for (const Edge &edge : edges) {
                const qreal distance = edgeLabelDistance(edge, strokes.at(i));
                if (distance < 0) continue;
                if (distance < best) { second = best; best = distance; id = edge.id; }
                else second = qMin(second, distance);
            }
            if (second - best < 8) id.clear();
            if (!id.isEmpty()) kind = QStringLiteral("edge");
        }
        if (id.isEmpty()) continue;
        groups[id].append(i); kinds.insert(id, kind); labels.insert(i);
    }
    QVariantList labelGroups, unassigned;
    for (auto group = groups.cbegin(); group != groups.cend(); ++group) {
        QVariantList ink;
        for (const int index : *group) ink.append(input.at(index));
        labelGroups.append(QVariantMap{{QStringLiteral("id"), group.key()}, {QStringLiteral("kind"), kinds.value(group.key())},
            {QStringLiteral("indexes"), variantIndexes(*group)}, {QStringLiteral("strokes"), ink}});
    }
    for (int i = 0; i < input.size(); ++i) if (!used.contains(i) && !labels.contains(i)) unassigned.append(i);
    QVariantMap diagram = existing;
    if (bounds != originalBounds) {
        diagram.insert(QStringLiteral("x"), pageBounds.x() / pageWidth);
        diagram.insert(QStringLiteral("y"), pageBounds.y() / pageHeight);
        diagram.insert(QStringLiteral("width"), pageBounds.width() / pageWidth);
        diagram.insert(QStringLiteral("height"), pageBounds.height() / pageHeight);
        const qreal oldScale = qMax(.1, qMin(originalBounds.width(), originalBounds.height()) / 350);
        const qreal newScale = qMax(.1, qMin(bounds.width(), bounds.height()) / 350);
        const qreal styleScale = existing.value(QStringLiteral("styleScale"), 1.0).toDouble() * oldScale / newScale;
        if (!qFuzzyCompare(styleScale, 1.0) || existing.contains(QStringLiteral("styleScale")))
            diagram.insert(QStringLiteral("styleScale"), styleScale);
    }
    diagram.insert(QStringLiteral("nodes"), nodeData); diagram.insert(QStringLiteral("edges"), edgeData);
    if (PageContent::validatedItem(diagram).isEmpty())
        return fail(QObject::tr("Il diagramma esteso non può mantenere le proprietà attuali. Riduci l'estensione e riprova."));
    QStringList warnings;
    if (ambiguous) warnings.append(QObject::tr("I collegamenti incerti o già presenti restano tratti a mano libera."));
    if (!unassigned.isEmpty()) warnings.append(QObject::tr("I tratti non assegnati restano sulla pagina."));
    result.insert(QStringLiteral("diagram"), diagram);
    result.insert(QStringLiteral("candidates"), candidates); result.insert(QStringLiteral("labelGroups"), labelGroups);
    result.insert(QStringLiteral("unassignedIndexes"), unassigned); result.insert(QStringLiteral("warnings"), warnings);
    result.insert(QStringLiteral("addedNodeCount"), nodes.size() - oldNodes.size());
    result.insert(QStringLiteral("addedEdgeCount"), edges.size());
    result.insert(QStringLiteral("hasChanges"), !candidates.isEmpty());
    return result;
}
}

QVariantMap DiagramRecognition::recognize(const QVariantList &input, const QVariantMap &existingDiagram)
{
    if (!existingDiagram.isEmpty()) return recognizeExtension(input, existingDiagram);
    QVariantMap result{{QStringLiteral("diagram"), QVariantMap{}}, {QStringLiteral("candidates"), QVariantList{}},
        {QStringLiteral("labelGroups"), QVariantList{}}, {QStringLiteral("unassignedIndexes"), QVariantList{}},
        {QStringLiteral("warnings"), QStringList{}}, {QStringLiteral("error"), QString{}},
        {QStringLiteral("extendsExisting"), false}, {QStringLiteral("hasChanges"), false},
        {QStringLiteral("existingNodeIds"), QVariantList{}}, {QStringLiteral("existingEdgeIds"), QVariantList{}},
        {QStringLiteral("addedNodeCount"), 0}, {QStringLiteral("addedEdgeCount"), 0}};
    QList<Stroke> strokes;
    const QString invalid = readStrokes(input, strokes);
    if (!invalid.isEmpty()) { result.insert(QStringLiteral("error"), invalid); return result; }
    bool tooComplex = false;
    const QList<Node> nodes = findNodes(strokes, tooComplex);
    if (tooComplex || nodes.isEmpty()) {
        result.insert(QStringLiteral("error"), tooComplex
            ? QObject::tr("Il disegno è troppo complesso. Seleziona un diagramma più piccolo.")
            : QObject::tr("Nessuna forma riconosciuta. Disegna rettangoli, rombi o ovali con contorni chiusi."));
        QVariantList remaining;
        for (int i = 0; i < input.size(); ++i) remaining.append(i);
        result.insert(QStringLiteral("unassignedIndexes"), remaining);
        return result;
    }
    QSet<int> used;
    QRectF bounds;
    for (const Node &node : nodes) {
        bounds = bounds.united(node.bounds);
        for (const int index : node.indexes) used.insert(index);
    }
    bool ambiguous = false;
    const QList<Edge> edges = findEdges(strokes, nodes, used, ambiguous, tooComplex);
    if (tooComplex) {
        result.insert(QStringLiteral("error"), QObject::tr("Il diagramma supera il limite di 128 collegamenti. Seleziona una parte più piccola."));
        QVariantList remaining;
        for (int i = 0; i < input.size(); ++i) remaining.append(i);
        result.insert(QStringLiteral("unassignedIndexes"), remaining);
        return result;
    }
    // Preserve original page placement; only internal node coordinates become local.
    bounds = bounds.adjusted(-3, -3, 3, 3).intersected(QRectF(0, 0, pageWidth, pageHeight));
    QVariantList nodeData, edgeData, candidates;
    for (const Node &node : nodes) {
        nodeData.append(QVariantMap{{QStringLiteral("id"), node.id}, {QStringLiteral("shape"), node.shape},
            {QStringLiteral("text"), QString{}}, {QStringLiteral("x"), (node.bounds.x() - bounds.x()) / bounds.width()},
            {QStringLiteral("y"), (node.bounds.y() - bounds.y()) / bounds.height()},
            {QStringLiteral("width"), node.bounds.width() / bounds.width()}, {QStringLiteral("height"), node.bounds.height() / bounds.height()}});
        candidates.append(QVariantMap{{QStringLiteral("id"), node.id}, {QStringLiteral("kind"), QStringLiteral("node")},
            {QStringLiteral("indexes"), variantIndexes(node.indexes)}});
    }
    for (const Edge &edge : edges) {
        QVariantMap data{{QStringLiteral("id"), edge.id}, {QStringLiteral("from"), nodes.at(edge.from).id},
            {QStringLiteral("to"), nodes.at(edge.to).id}, {QStringLiteral("text"), QString{}}};
        if (edge.orthogonal) data.insert(QStringLiteral("route"), QStringLiteral("orthogonal"));
        edgeData.append(data);
        candidates.append(QVariantMap{{QStringLiteral("id"), edge.id}, {QStringLiteral("kind"), QStringLiteral("edge")},
            {QStringLiteral("indexes"), variantIndexes(edge.indexes)}});
    }
    QMap<QString, QList<int>> groups;
    QMap<QString, QString> groupKinds;
    QSet<int> labels;
    for (int i = 0; i < strokes.size(); ++i) {
        if (used.contains(i) || strokes.at(i).points.isEmpty()
            || (strokes.at(i).tool != QStringLiteral("pen") && strokes.at(i).tool != QStringLiteral("pencil")
                && strokes.at(i).tool != QStringLiteral("marker"))) continue;
        QString id, kind;
        for (const Node &node : nodes) if (insideNode(node, strokes.at(i).points)) {
            if (!id.isEmpty()) { id.clear(); break; }
            id = node.id; kind = QStringLiteral("node");
        }
        if (id.isEmpty()) {
            qreal best = std::numeric_limits<qreal>::infinity(), second = best;
            for (const Edge &edge : edges) {
                const qreal distance = edgeLabelDistance(edge, strokes.at(i));
                if (distance < 0) continue;
                if (distance < best) { second = best; best = distance; id = edge.id; }
                else second = qMin(second, distance);
            }
            if (second - best < 8) id.clear();
            if (!id.isEmpty()) kind = QStringLiteral("edge");
        }
        if (id.isEmpty()) continue;
        groups[id].append(i);
        groupKinds.insert(id, kind);
        labels.insert(i);
    }
    QVariantList labelGroups, unassigned;
    // Accumulate indexes first. Appending to QVariantLists extracted from a
    // QVariantMap repeatedly would detach/copy the whole group for every stroke.
    for (auto group = groups.cbegin(); group != groups.cend(); ++group) {
        QVariantList indexes, ink;
        indexes.reserve(group->size());
        ink.reserve(group->size());
        for (const int index : *group) { indexes.append(index); ink.append(input.at(index)); }
        labelGroups.append(QVariantMap{{QStringLiteral("id"), group.key()},
            {QStringLiteral("kind"), groupKinds.value(group.key())},
            {QStringLiteral("indexes"), indexes}, {QStringLiteral("strokes"), ink}});
    }
    for (int i = 0; i < input.size(); ++i) if (!used.contains(i) && !labels.contains(i)) unassigned.append(i);
    QStringList warnings;
    if (ambiguous) warnings.append(QObject::tr("Alcuni collegamenti non hanno una direzione chiara e restano tratti a mano libera."));
    if (!unassigned.isEmpty()) warnings.append(QObject::tr("I tratti non assegnati restano sulla pagina."));
    result.insert(QStringLiteral("diagram"), QVariantMap{{QStringLiteral("type"), QStringLiteral("diagram")},
        {QStringLiteral("x"), bounds.x() / pageWidth}, {QStringLiteral("y"), bounds.y() / pageHeight},
        {QStringLiteral("width"), bounds.width() / pageWidth}, {QStringLiteral("height"), bounds.height() / pageHeight},
        {QStringLiteral("nodes"), nodeData}, {QStringLiteral("edges"), edgeData}});
    result.insert(QStringLiteral("candidates"), candidates); result.insert(QStringLiteral("labelGroups"), labelGroups);
    result.insert(QStringLiteral("unassignedIndexes"), unassigned); result.insert(QStringLiteral("warnings"), warnings);
    result.insert(QStringLiteral("hasChanges"), true);
    result.insert(QStringLiteral("addedNodeCount"), nodes.size());
    result.insert(QStringLiteral("addedEdgeCount"), edges.size());
    return result;
}
