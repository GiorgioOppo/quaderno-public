#include "inkselection.h"

#include "inkrender.h"
#include "pagecontent.h"
#include <QLineF>

#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal pageAspect = 4.0 / 3.0;
constexpr qreal tolerance = 1e-9;
constexpr qsizetype maximumPoints = 160000;
constexpr qsizetype maximumStrokes = 4000;
const QString pointsKey = QStringLiteral("points");
const QString originalKey = QStringLiteral("original");
const QString toolKey = QStringLiteral("tool");
const QString widthKey = QStringLiteral("width");
const QString xKey = QStringLiteral("x");
const QString yKey = QStringLiteral("y");
const QString pressureKey = QStringLiteral("p");

struct Extents {
    bool empty = true;
    qreal left = 0;
    qreal right = 0;
    qreal top = 0;
    qreal bottom = 0;

    void add(QPointF point)
    {
        if (empty) {
            *this = {false, point.x(), point.x(), point.y(), point.y()};
            return;
        }
        left = qMin(left, point.x());
        right = qMax(right, point.x());
        top = qMin(top, point.y());
        bottom = qMax(bottom, point.y());
    }

    QRectF rect() const
    {
        return QRectF(QPointF(left, top), QPointF(right, bottom));
    }
};

bool finite(QPointF point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool inUnitRange(qreal value)
{
    return value >= 0 && value <= 1;
}

bool isQuarterTurn(qreal angle)
{
    return std::abs(std::remainder(angle, 90.0)) < tolerance;
}

bool needsQuarterTurns(const QVariantMap &stroke)
{
    const auto tool = inkToolFromName(stroke.value(toolKey).toString());
    return (tool == InkTool::Rectangle || tool == InkTool::Ellipse) && stroke.contains(originalKey);
}

struct Transform {
    QPointF pivot;
    qreal scale;
    qreal sine;
    qreal cosine;
    bool quarterTurn;

    QPointF map(QPointF position) const
    {
        if (scale == 1 && sine == 0 && cosine == 1)
            return position;
        const QPointF delta = position - pivot;
        const qreal x = delta.x();
        const qreal y = delta.y() * pageAspect;
        return pivot + QPointF((x * cosine - y * sine) * scale,
                               (x * sine + y * cosine) * scale / pageAspect);
    }
};

struct MappedStroke {
    QVariantMap style;
    QList<InkPoint> points;
    QVariantMap originalStyle;
    QList<InkPoint> originalPoints;
};

struct MappedSelection {
    QList<MappedStroke> strokes;
    Extents display;
    Extents full;
};

bool scaleWidth(QVariantMap &stroke, qreal scale)
{
    const qreal width = stroke.value(widthKey).toReal() * scale;
    if (!std::isfinite(width) || width <= 0)
        return false;
    stroke.insert(widthKey, width);
    return true;
}

QList<InkPoint> mapPoints(const QVariantMap &stroke, const Transform &transform, bool flatten, Extents &extents)
{
    QList<InkPoint> points;
    const QVariantList samples = stroke.value(pointsKey).toList();
    if (samples.isEmpty() || samples.size() > maximumPoints)
        return {};
    points.reserve(samples.size());
    for (const QVariant &value : samples) {
        const QVariantMap sample = value.toMap();
        bool xOk = false;
        bool yOk = false;
        bool pOk = false;
        const qreal x = sample.value(xKey).toDouble(&xOk);
        const qreal y = sample.value(yKey).toDouble(&yOk);
        const qreal p = sample.value(pressureKey).toDouble(&pOk);
        if (!xOk || !yOk || !pOk || !inUnitRange(x) || !inUnitRange(y) || !inUnitRange(p))
            return {};
        points.append({QPointF(x, y), p});
    }
    const auto tool = inkToolFromName(stroke.value(toolKey).toString());
    if (isInkShape(tool) && points.size() != 2)
        return {};
    if (flatten)
        points = shapeOutline(points, tool);
    for (InkPoint &point : points) {
        point.position = transform.map(point.position);
        extents.add(point.position);
    }
    return points;
}

MappedSelection mapSelection(const QVariantList &strokes, QPointF pivot, qreal scale, qreal angle)
{
    if (!finite(pivot) || !inUnitRange(pivot.x()) || !inUnitRange(pivot.y()) || !std::isfinite(scale)
        || scale < 0.15 || scale > 6 || !std::isfinite(angle) || strokes.size() > maximumStrokes)
        return {};

    angle = std::remainder(angle, 360.0);
    const bool quarterTurn = isQuarterTurn(angle);
    const qreal radians = qDegreesToRadians(angle);
    const qreal sine = std::sin(radians);
    const qreal cosine = std::cos(radians);
    const Transform transform{pivot, scale, quarterTurn ? std::round(sine) : sine,
                              quarterTurn ? std::round(cosine) : cosine, quarterTurn};
    qsizetype count = 0;
    MappedSelection selection;
    selection.strokes.reserve(strokes.size());
    for (const QVariant &entry : strokes) {
        QVariantMap stroke = entry.toMap();
        const auto tool = inkToolFromName(stroke.value(toolKey).toString());
        if ((!transform.quarterTurn && needsQuarterTurns(stroke)) || !scaleWidth(stroke, scale))
            return {};
        const bool flatten = !transform.quarterTurn && (tool == InkTool::Rectangle || tool == InkTool::Ellipse);
        QList<InkPoint> points = mapPoints(stroke, transform, flatten, selection.display);
        if (points.isEmpty())
            return {};
        count += points.size();

        QVariantMap originalStyle = stroke.take(originalKey).toMap();
        QList<InkPoint> originalPoints;
        if (!originalStyle.isEmpty()) {
            originalPoints = mapPoints(originalStyle, transform, false, selection.full);
            if (originalPoints.isEmpty() || !scaleWidth(originalStyle, scale))
                return {};
            count += originalPoints.size();
            originalStyle.remove(pointsKey);
        }
        if (count > maximumPoints)
            return {};
        stroke.remove(pointsKey);
        if (flatten)
            stroke.remove(toolKey);
        selection.strokes.append({std::move(stroke), std::move(points), std::move(originalStyle),
                                  std::move(originalPoints)});
    }
    if (!selection.display.empty) {
        selection.full.add(selection.display.rect().topLeft());
        selection.full.add(selection.display.rect().bottomRight());
    }
    return selection;
}

QPointF clampOffset(QRectF bounds, QPointF desired)
{
    return QPointF(qBound(-bounds.left(), desired.x(), qMax(-bounds.left(), 1 - bounds.right())),
                   qBound(-bounds.top(), desired.y(), qMax(-bounds.top(), 1 - bounds.bottom())));
}

QVariantList encodePoints(const QList<InkPoint> &points, QPointF offset)
{
    QVariantList result;
    result.reserve(points.size());
    for (const InkPoint &point : points) {
        const QPointF position = point.position + offset;
        result.append(QVariantMap{{xKey, qBound(0.0, position.x(), 1.0)},
                                  {yKey, qBound(0.0, position.y(), 1.0)},
                                  {pressureKey, point.pressure}});
    }
    return result;
}

QVariantList encode(const MappedSelection &mapped, QPointF offset, bool includeOriginal)
{
    const QRectF bounds = mapped.full.rect().translated(offset);
    if (!finite(offset) || bounds.left() < -tolerance || bounds.top() < -tolerance
        || bounds.right() > 1 + tolerance || bounds.bottom() > 1 + tolerance)
        return {};

    QVariantList result;
    result.reserve(mapped.strokes.size());
    for (const MappedStroke &stroke : mapped.strokes) {
        QVariantMap entry = stroke.style;
        entry.insert(pointsKey, encodePoints(stroke.points, offset));
        if (includeOriginal && !stroke.originalPoints.isEmpty()) {
            QVariantMap original = stroke.originalStyle;
            original.insert(pointsKey, encodePoints(stroke.originalPoints, offset));
            entry.insert(originalKey, original);
        }
        result.append(entry);
    }
    return result;
}
}

QVariantList InkSelectionGeometry::transformed(const QVariantList &strokes, QPointF pivot, QPointF offset, qreal scale, qreal angle)
{
    return encode(mapSelection(strokes, pivot, scale, angle), offset, true);
}

bool InkSelectionGeometry::intersectsStroke(const QPolygonF &polygon, const QList<QPointF> &points, bool contained)
{
    if (points.isEmpty() || polygon.size() < 3)
        return false;
    const auto inside = [&polygon](QPointF p) {
        if (polygon.containsPoint(p, Qt::OddEvenFill))
            return true;
        // Include the boundary so a point or an accent exactly on the outline is selectable.
        for (qsizetype i = 0; i < polygon.size(); ++i) {
            const QPointF a = polygon.at(i), d = polygon.at((i + 1) % polygon.size()) - a;
            const qreal length2 = QPointF::dotProduct(d, d);
            const qreal t = length2 > 0 ? qBound(0.0, QPointF::dotProduct(p - a, d) / length2, 1.0) : 0;
            if (QLineF(p, a + t * d).length() <= 1e-8)
                return true;
        }
        return false;
    };
    if (points.size() == 1)
        return inside(points.first());
    bool touched = false;
    for (qsizetype i = 1; i < points.size(); ++i) {
        const QLineF segment(points.at(i - 1), points.at(i));
        const bool aInside = inside(segment.p1()), bInside = inside(segment.p2());
        if (contained && (!aInside || !bInside))
            return false;
        if (!contained && (aInside || bInside))
            return true;
        QList<qreal> cuts{0, 1};
        for (qsizetype edge = 0; edge < polygon.size(); ++edge) {
            QPointF crossing;
            const QLineF border(polygon.at(edge), polygon.at((edge + 1) % polygon.size()));
            if (segment.intersects(border, &crossing) == QLineF::BoundedIntersection) {
                if (!contained)
                    return true;
                const QPointF d = segment.p2() - segment.p1();
                const qreal length2 = QPointF::dotProduct(d, d);
                if (length2 > 0)
                    cuts.append(qBound(0.0, QPointF::dotProduct(crossing - segment.p1(), d) / length2, 1.0));
            }
        }
        std::sort(cuts.begin(), cuts.end());
        for (qsizetype part = 1; part < cuts.size(); ++part) {
            const bool midpointInside = inside(segment.pointAt((cuts.at(part - 1) + cuts.at(part)) / 2));
            if (contained && !midpointInside)
                return false;
            touched |= midpointInside;
        }
    }
    return contained || touched;
}

QPolygonF InkSelectionGeometry::itemOutline(const QVariantMap &item)
{
    const QRectF rect(item.value(xKey).toDouble(), item.value(yKey).toDouble(),
                      item.value(QStringLiteral("width")).toDouble(), item.value(QStringLiteral("height")).toDouble());
    const qreal radians = qDegreesToRadians(item.value(QStringLiteral("rotation")).toDouble());
    const Transform transform{rect.center(), 1, std::sin(radians), std::cos(radians), false};
    QPolygonF polygon;
    for (QPointF p : {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft(), rect.topLeft()})
        polygon.append(transform.map(p));
    return polygon;
}

QRectF InkSelectionGeometry::boundsOf(const QVariantList &strokes, const QVariantList &items)
{
    Extents extents;
    for (const QVariant &entry : strokes) {
        for (const auto &point : storedInkPoints(entry.toMap()))
            extents.add(point.position);
    }
    for (const QVariant &entry : items) {
        for (QPointF point : itemOutline(entry.toMap()))
            extents.add(point);
    }
    return extents.empty ? QRectF{} : extents.rect();
}

QVariantList InkSelectionGeometry::transformedItems(const QVariantList &items, QPointF pivot, QPointF offset,
                                                   qreal scale, qreal angle)
{
    if (!finite(pivot) || !finite(offset) || !std::isfinite(scale) || scale < .15 || scale > 6 || !std::isfinite(angle))
        return {};
    const qreal radians = qDegreesToRadians(std::remainder(angle, 360.0));
    const Transform transform{pivot, scale, std::sin(radians), std::cos(radians), false};
    QVariantList result;
    for (const QVariant &entry : items) {
        QVariantMap item = entry.toMap();
        const qreal width = item.value(QStringLiteral("width")).toDouble();
        const qreal height = item.value(QStringLiteral("height")).toDouble();
        const QPointF center = transform.map({item.value(xKey).toDouble() + width / 2,
                                               item.value(yKey).toDouble() + height / 2}) + offset;
        item.insert(xKey, center.x() - width * scale / 2);
        item.insert(yKey, center.y() - height * scale / 2);
        item.insert(QStringLiteral("width"), width * scale);
        item.insert(QStringLiteral("height"), height * scale);
        item.insert(QStringLiteral("rotation"), std::remainder(item.value(QStringLiteral("rotation")).toDouble() + angle, 360.0));
        item.insert(QStringLiteral("textScale"), item.value(QStringLiteral("textScale"), 1.0).toDouble() * scale);
        if (item.contains(QStringLiteral("handwritingSource"))) {
            QVariantMap source = item.value(QStringLiteral("handwritingSource")).toMap();
            const QVariantList original = source.value(QStringLiteral("strokes")).toList();
            const QVariantList moved = transformed(original, pivot, offset, scale, angle);
            if (moved.size() != original.size())
                return {};
            source.insert(QStringLiteral("strokes"), moved);
            if (source.contains(QStringLiteral("baseDiagram"))) {
                const QVariantMap base = source.value(QStringLiteral("baseDiagram")).toMap();
                if (base.value(QStringLiteral("handwritingSource")).toMap().contains(QStringLiteral("baseDiagram")))
                    return {};
                const QVariantList movedBase = transformedItems({base}, pivot, offset, scale, angle);
                if (movedBase.size() != 1) return {};
                source.insert(QStringLiteral("baseDiagram"), movedBase.first());
            }
            item.insert(QStringLiteral("handwritingSource"), source);
        }
        const QRectF bounds = itemOutline(item).boundingRect();
        if (bounds.left() < -tolerance || bounds.top() < -tolerance || bounds.right() > 1 + tolerance || bounds.bottom() > 1 + tolerance)
            return {};
        item = PageContent::validatedItem(item);
        if (item.isEmpty())
            return {};
        result.append(item);
    }
    return result;
}

InkSelectionPreview::InkSelectionPreview(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setFillColor(Qt::transparent);
}

void InkSelectionPreview::setStrokes(const QVariantList &strokes)
{
    if (strokes == m_strokes)
        return;
    m_strokes = strokes;
    const auto initial = mapSelection(strokes, QPointF(), 1, 0);
    m_selectionSupported = !initial.strokes.isEmpty();
    m_pivot = initial.display.rect().center();
    m_quarterTurnsOnly = std::any_of(strokes.cbegin(), strokes.cend(), [](const QVariant &entry) {
        return needsQuarterTurns(entry.toMap());
    });
    rebuild();
    emit strokesChanged();
    emit transformChanged();
}

void InkSelectionPreview::setScaleFactor(qreal scale)
{
    if (scale == m_scale)
        return;
    m_scale = scale;
    rebuild();
    emit transformChanged();
}

void InkSelectionPreview::setAngle(qreal angle)
{
    if (angle == m_angle)
        return;
    m_angle = angle;
    rebuild();
    emit transformChanged();
}

void InkSelectionPreview::setOffset(QPointF offset)
{
    if (offset == m_offset || !finite(offset))
        return;
    m_offset = offset;
    place();
}

void InkSelectionPreview::rebuild()
{
    const auto mapped = mapSelection(m_strokes, m_pivot, m_scale, m_angle);
    m_displayBounds = mapped.display.rect();
    m_fullBounds = mapped.full.rect();
    m_baseOffset = clampOffset(m_fullBounds, QPointF());
    m_preview = encode(mapped, m_baseOffset, false);
    place();
    update();
}

void InkSelectionPreview::place()
{
    m_appliedOffset = valid() ? clampOffset(m_fullBounds, m_offset) : QPointF();
    emit placementChanged();
}

void InkSelectionPreview::paint(QPainter *painter)
{
    paintStoredStrokes(*painter, size(), m_preview);
}

SelectionContentItem::SelectionContentItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setFillColor(Qt::transparent);
    connect(this, &SelectionContentItem::contentChanged, this, [this] { update(); });
}

void SelectionContentItem::paint(QPainter *painter)
{
    PageContent::paintItems(*painter, size(), m_content.value(QStringLiteral("items")).toList());
    paintStoredStrokes(*painter, size(), m_content.value(QStringLiteral("strokes")).toList());
}
