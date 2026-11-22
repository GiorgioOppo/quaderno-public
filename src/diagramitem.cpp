#include "diagramitem.h"

#include <QFontMetricsF>
#include <QHash>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QSet>
#include <QTextOption>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
const QColor outline(QStringLiteral("#252525"));
const QColor selection(QStringLiteral("#e1e1e1"));
constexpr qsizetype maximumLassoPoints = 4096;

struct Node {
    QString id;
    QString text;
    QRectF rect;
    QRectF label;
    QPainterPath outline;
    bool dashed = false;
};

struct Edge {
    QString id;
    QString text;
    QPainterPath route;
    QPointF head;
    QPointF approach;
};

QString singleLineLabel(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral(" "));
    text.replace(u'\r', u' ');
    text.replace(u'\n', u' ');
    return text;
}

qreal diagramScale(const QRectF &bounds, const QVariantMap &diagram)
{
    const qreal style = diagram.value(QStringLiteral("styleScale"), 1.0).toDouble();
    return qMax<qreal>(0.1, qMin(bounds.width(), bounds.height()) / 350.0)
        * (std::isfinite(style) && style >= 0.001 && style <= 1000.0 ? style : 1.0);
}

QFont labelFont(qreal scale, bool edge)
{
    QFont font;
    font.setPixelSize(qMax(1, qRound((edge ? 13 : 14) * scale)));
    return font;
}

Node layoutNode(const QVariantMap &data, const QRectF &bounds, qreal scale)
{
    Node node;
    node.id = data.value(QStringLiteral("id")).toString();
    node.text = data.value(QStringLiteral("text")).toString();
    const auto shape = data.value(QStringLiteral("shape")).toString();
    const qreal x = data.value(QStringLiteral("x")).toDouble();
    const qreal y = data.value(QStringLiteral("y")).toDouble();
    const qreal width = data.value(QStringLiteral("width")).toDouble();
    const qreal height = data.value(QStringLiteral("height")).toDouble();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)
        || width <= 0 || height <= 0)
        return node;

    node.rect = {bounds.x() + x * bounds.width(), bounds.y() + y * bounds.height(),
                 width * bounds.width(), height * bounds.height()};
    const auto &r = node.rect;
    const auto center = r.center();
    node.label = r.adjusted(5 * scale, 3 * scale, -5 * scale, -3 * scale);
    if (shape == QStringLiteral("terminal")) {
        const qreal radius = qMin(r.height() * 0.5, r.width() * 0.23);
        node.outline.addRoundedRect(r, radius, radius);
        node.label.adjust(radius * 0.32, 0, -radius * 0.32, 0);
    } else if (shape == QStringLiteral("decision")) {
        node.outline.addPolygon({QPointF(center.x(), r.top()), QPointF(r.right(), center.y()),
                                 QPointF(center.x(), r.bottom()), QPointF(r.left(), center.y())});
        node.outline.closeSubpath();
        node.label = r.adjusted(r.width() * 0.20, r.height() * 0.29, -r.width() * 0.20, -r.height() * 0.29);
    } else if (shape == QStringLiteral("data")) {
        const qreal slant = qMin(r.width() * 0.17, r.height() * 0.40);
        node.outline.addPolygon({QPointF(r.left() + slant, r.top()), r.topRight(),
                                 QPointF(r.right() - slant, r.bottom()), r.bottomLeft()});
        node.outline.closeSubpath();
        node.label.adjust(slant * 0.62, 0, -slant * 0.62, 0);
    } else {
        node.outline.addRect(r);
        node.dashed = shape == QStringLiteral("note");
    }
    return node;
}

QPointF unitVector(const QPointF &vector)
{
    const qreal length = std::hypot(vector.x(), vector.y());
    return length > 0 ? vector / length : QPointF(1, 0);
}

QPointF outwardNormal(const QPointF &vector)
{
    return {-vector.y(), vector.x()};
}

QPointF outlineAnchor(const Node &node, const QPointF &origin, const QPointF &direction)
{
    // Supported outlines are convex, so a ray from an interior point meets
    // the actual outline once, including on a diamond or rounded corner.
    const QPointF ray = unitVector(direction) * std::hypot(node.rect.width(), node.rect.height());
    qreal near = 0;
    qreal far = 1;
    for (int step = 0; step < 18; ++step) {
        const qreal middle = (near + far) / 2;
        if (node.outline.contains(origin + middle * ray))
            near = middle;
        else
            far = middle;
    }
    return origin + far * ray;
}

QPointF nodeAnchor(const Node &node, const QPointF &direction)
{
    return outlineAnchor(node, node.rect.center(), direction);
}

void routeOrthogonally(Edge &edge, const Node &from, const Node &to, bool paired, const QRectF &bounds, qreal scale)
{
    const QPointF delta = to.rect.center() - from.rect.center();
    const qreal xGap = qAbs(delta.x()) - (from.rect.width() + to.rect.width()) / 2;
    const qreal yGap = qAbs(delta.y()) - (from.rect.height() + to.rect.height()) / 2;
    const bool horizontal = (xGap >= 0) == (yGap >= 0) ? qAbs(delta.x()) >= qAbs(delta.y()) : xGap >= 0;
    const QPointF axis = horizontal ? QPointF(1, 0) : QPointF(0, 1);
    const QPointF cross = horizontal ? QPointF(0, 1) : QPointF(1, 0);
    const qreal along = QPointF::dotProduct(delta, axis);
    const qreal sign = along > 0 || (along == 0 && from.id < to.id) ? 1 : -1;
    const auto point = [&](qreal primary, qreal secondary) { return primary * axis + secondary * cross; };
    const auto primary = [&](const QPointF &value) { return QPointF::dotProduct(value, axis); };
    const auto secondary = [&](const QPointF &value) { return QPointF::dotProduct(value, cross); };

    if (!paired && (horizontal ? xGap : yGap) > 0.01) {
        const auto start = nodeAnchor(from, sign * axis);
        edge.head = nodeAnchor(to, -sign * axis);
        const qreal middle = (primary(start) + primary(edge.head)) / 2;
        edge.approach = point(middle, secondary(edge.head));
        edge.route.moveTo(start);
        edge.route.lineTo(point(middle, secondary(start)));
        edge.route.lineTo(edge.approach);
        edge.route.lineTo(edge.head);
        if (QLineF(edge.approach, edge.head).length() < 0.01)
            edge.approach = edge.head - sign * axis;
        return;
    }

    // Parallel connectors use opposite exterior lanes. The same exterior
    // route keeps a connector visible when its end shapes overlap.
    const qreal lower = qMin(secondary(from.rect.topLeft()), secondary(to.rect.topLeft()));
    const qreal upper = qMax(secondary(from.rect.bottomRight()), secondary(to.rect.bottomRight()));
    const qreal availableLow = lower - secondary(bounds.topLeft());
    const qreal availableHigh = secondary(bounds.bottomRight()) - upper;
    const qreal desiredGap = 18 * scale;
    qreal side = horizontal ? sign : -sign;
    if (!paired
        && (side < 0 ? availableHigh : availableLow) > (side < 0 ? availableLow : availableHigh))
        side = -side;
    const qreal room = side < 0 ? availableLow : availableHigh;
    const qreal lane = (side < 0 ? lower : upper) + side * qMin(desiredGap, qMax(0.0, room * 0.78));
    const auto outward = side * cross;
    auto startOrigin = from.rect.center();
    auto endOrigin = to.rect.center();
    if (qAbs(along) < 4 * scale) {
        startOrigin -= sign * axis * (horizontal ? from.rect.width() : from.rect.height()) * 0.18;
        endOrigin += sign * axis * (horizontal ? to.rect.width() : to.rect.height()) * 0.18;
    }
    const auto start = outlineAnchor(from, startOrigin, outward);
    edge.head = outlineAnchor(to, endOrigin, outward);
    edge.approach = point(primary(edge.head), lane);
    edge.route.moveTo(start);
    edge.route.lineTo(point(primary(start), lane));
    edge.route.lineTo(edge.approach);
    edge.route.lineTo(edge.head);
    if (QLineF(edge.approach, edge.head).length() < 0.01)
        edge.approach = edge.head + outward;
}

Edge layoutEdge(const QVariantMap &data, const Node &from, const Node &to, bool paired, const QRectF &bounds, qreal scale)
{
    Edge edge;
    edge.id = data.value(QStringLiteral("id")).toString();
    edge.text = singleLineLabel(data.value(QStringLiteral("text")).toString());
    if (data.value(QStringLiteral("route")).toString() == QStringLiteral("orthogonal")) {
        routeOrthogonally(edge, from, to, paired, bounds, scale);
        return edge;
    }
    const auto fromCenter = from.rect.center();
    const auto toCenter = to.rect.center();
    const QPointF delta = toCenter - fromCenter;
    const qreal distance = std::hypot(delta.x(), delta.y());
    const bool overlapping = from.rect.intersects(to.rect);
    if (!paired && !overlapping) {
        const auto start = nodeAnchor(from, delta);
        edge.head = nodeAnchor(to, -delta);
        edge.approach = start;
        edge.route.moveTo(start);
        edge.route.lineTo(edge.head);
        return edge;
    }

    const QPointF direction = distance > 0 ? unitVector(delta) : QPointF(from.id < to.id ? 1 : -1, 0);
    const qreal bend = overlapping ? qMax(from.rect.height(), to.rect.height()) * 1.4
                                   : qBound(19 * scale, distance * 0.20, 55 * scale);
    const auto offset = outwardNormal(direction) * bend;
    const auto first = fromCenter + delta * 0.33 + offset;
    edge.approach = fromCenter + delta * 0.67 + offset;
    const auto start = nodeAnchor(from, first - fromCenter);
    edge.head = nodeAnchor(to, edge.approach - toCenter);
    edge.route.moveTo(start);
    edge.route.cubicTo(first, edge.approach, edge.head);
    return edge;
}

struct Layout {
    QList<Node> nodes;
    QList<Edge> edges;
};

Layout layoutDiagram(const QVariantMap &diagram, const QRectF &bounds, qreal scale)
{
    Layout layout;
    const auto nodes = diagram.value(QStringLiteral("nodes")).toList();
    QHash<QString, qsizetype> indices;
    layout.nodes.reserve(nodes.size());
    for (const QVariant &item : nodes) {
        Node node = layoutNode(item.toMap(), bounds, scale);
        if (!node.id.isEmpty() && !node.outline.isEmpty()) {
            indices.insert(node.id, layout.nodes.size());
            layout.nodes.append(std::move(node));
        }
    }

    const auto edges = diagram.value(QStringLiteral("edges")).toList();
    QSet<QPair<QString, QString>> directions;
    for (const QVariant &item : edges) {
        const auto data = item.toMap();
        directions.insert({data.value(QStringLiteral("from")).toString(), data.value(QStringLiteral("to")).toString()});
    }
    layout.edges.reserve(edges.size());
    for (const QVariant &item : edges) {
        const auto data = item.toMap();
        const auto from = data.value(QStringLiteral("from")).toString();
        const auto to = data.value(QStringLiteral("to")).toString();
        const auto a = indices.constFind(from);
        const auto b = indices.constFind(to);
        if (from != to && a != indices.cend() && b != indices.cend()) {
            const bool paired = directions.contains({to, from});
            layout.edges.append(layoutEdge(data, layout.nodes.at(a.value()), layout.nodes.at(b.value()), paired, bounds, scale));
        }
    }
    return layout;
}

QRectF edgeLabel(const Edge &edge, const QList<Node> &nodes, const QFontMetricsF &metrics,
                 const QRectF &bounds, qreal scale)
{
    if (edge.text.isEmpty() || edge.route.isEmpty())
        return {};
    const qreal width = qMin(bounds.width() * 0.40, qCeil(metrics.horizontalAdvance(edge.text)) + 9 * scale);
    const qreal height = qMin(bounds.height(), metrics.height() + 3 * scale);
    const QPointF center = edge.route.pointAtPercent(0.5);
    const auto labelAt = [&](const QPointF &point) -> QRectF {
        return {qBound(bounds.left(), point.x() - width / 2, bounds.right() - width),
                qBound(bounds.top(), point.y() - height / 2, bounds.bottom() - height), width, height};
    };
    const auto overlapsNode = [&](const QRectF &label) {
        return std::any_of(nodes.cbegin(), nodes.cend(), [&](const Node &node) { return node.outline.intersects(label); });
    };
    const auto centered = labelAt(center);
    if (!overlapsNode(centered))
        return centered;

    // Put labels beside short connections if their midpoints leave too
    // little space between the endpoints.
    const auto normal = outwardNormal(unitVector(edge.route.pointAtPercent(0.55) - edge.route.pointAtPercent(0.45)));
    for (const int step : {-1, 1, -2, 2, -3, 3}) {
        const auto candidate = labelAt(center + normal * step * height * 0.85);
        if (!overlapsNode(candidate))
            return candidate;
    }
    return centered;
}

void paintDiagram(QPainter &painter, const QRectF &bounds, const QVariantMap &diagram,
                  const QStringList &selectedNodes, const QString &selectedEdge)
{
    if (bounds.isEmpty())
        return;
    const qreal scale = diagramScale(bounds, diagram);
    const auto layout = layoutDiagram(diagram, bounds, scale);
    painter.save();
    painter.setClipRect(bounds, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const Edge &edge : layout.edges) {
        const bool selected = !selectedEdge.isEmpty() && selectedEdge == edge.id;
        painter.setPen(QPen(outline, (selected ? 2.7 : 1.5) * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(edge.route);
        const auto direction = unitVector(edge.head - edge.approach);
        const auto normal = outwardNormal(direction) * (3.5 * scale);
        const auto tail = edge.head - direction * (8 * scale);
        painter.setPen(Qt::NoPen);
        painter.setBrush(outline);
        painter.drawPolygon({edge.head, tail + normal, tail - normal});
    }

    const QFont edgeFont = labelFont(scale, true);
    const QFontMetricsF edgeMetrics(edgeFont, painter.device());
    painter.setFont(edgeFont);
    for (const Edge &edge : layout.edges) {
        const QRectF label = edgeLabel(edge, layout.nodes, edgeMetrics, bounds, scale);
        if (!label.isEmpty()) {
            painter.fillRect(label, edge.id == selectedEdge ? selection : QColor(Qt::white));
            painter.setPen(outline);
            painter.drawText(label, Qt::AlignCenter | Qt::TextSingleLine,
                             edgeMetrics.elidedText(edge.text, Qt::ElideRight, qMax<qreal>(0, label.width() - 8 * scale)));
        }
    }

    QTextOption labelOption(Qt::AlignCenter);
    labelOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    for (const Node &node : layout.nodes) {
        const bool selected = selectedNodes.contains(node.id);
        painter.setPen(QPen(outline, (selected ? 2.8 : 1.7) * scale,
                            node.dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(selected ? selection : QColor(Qt::white));
        painter.drawPath(node.outline);
        if (!node.text.isEmpty() && !node.label.isEmpty()) {
            painter.save();
            painter.setClipRect(node.label, Qt::IntersectClip);
            painter.setPen(outline);
            auto font = labelFont(scale, false);
            if (node.text.size() <= 20 && !node.text.contains(u' ') && !node.text.contains(u'\n')) {
                const qreal textWidth = QFontMetricsF(font, painter.device()).horizontalAdvance(node.text);
                if (textWidth > node.label.width())
                    font.setPixelSize(qMax(qRound(font.pixelSize() * 0.76),
                                           qFloor(font.pixelSize() * node.label.width() / textWidth)));
            }
            painter.setFont(font);
            painter.drawText(node.label, node.text, labelOption);
            painter.restore();
        }
    }
    painter.restore();
}
}

void drawDiagram(QPainter &painter, const QRectF &bounds, const QVariantMap &diagram)
{
    paintDiagram(painter, bounds, diagram, {}, {});
}

void DiagramItem::setDiagram(const QVariantMap &diagram)
{
    if (m_diagram == diagram)
        return;
    m_diagram = diagram;
    emit diagramChanged();
    update();
}

void DiagramItem::setSelectedNode(const QString &id)
{
    if (m_selectedNode == id)
        return;
    m_selectedNode = id;
    emit selectedNodeChanged();
    update();
}

void DiagramItem::setSelectedNodes(const QStringList &ids)
{
    if (m_selectedNodes == ids)
        return;
    m_selectedNodes = ids;
    emit selectedNodesChanged();
    update();
}

void DiagramItem::setSelectedEdge(const QString &id)
{
    if (m_selectedEdge == id)
        return;
    m_selectedEdge = id;
    emit selectedEdgeChanged();
    update();
}

void DiagramItem::setViewRect(const QRectF &rect)
{
    const bool finite = std::isfinite(rect.left()) && std::isfinite(rect.top())
        && std::isfinite(rect.right()) && std::isfinite(rect.bottom());
    const QRectF next = finite && !rect.isEmpty() ? rect : QRectF{};
    if (m_viewRect == next)
        return;
    m_viewRect = next;
    emit viewRectChanged();
    update();
}

QRectF DiagramItem::diagramBounds() const
{
    return m_viewRect.isEmpty() ? boundingRect() : m_viewRect;
}

void DiagramItem::paint(QPainter *painter)
{
    auto selected = m_selectedNodes;
    if (!m_selectedNode.isEmpty() && !selected.contains(m_selectedNode))
        selected.append(m_selectedNode);
    painter->save();
    painter->setClipRect(boundingRect(), Qt::IntersectClip);
    paintDiagram(*painter, diagramBounds(), m_diagram, selected, m_selectedEdge);
    painter->restore();
}

QString DiagramItem::nodeAt(qreal x, qreal y) const
{
    const QRectF bounds = diagramBounds();
    const QPointF point(x, y);
    if (!boundingRect().contains(point) || !bounds.contains(point))
        return {};
    const auto nodes = m_diagram.value(QStringLiteral("nodes")).toList();
    const qreal scale = diagramScale(bounds, m_diagram);
    for (auto it = nodes.crbegin(); it != nodes.crend(); ++it) {
        const Node node = layoutNode(it->toMap(), bounds, scale);
        if (node.outline.contains(point))
            return node.id;
    }
    return {};
}

QString DiagramItem::edgeAt(qreal x, qreal y) const
{
    const QRectF bounds = diagramBounds();
    const QPointF point(x, y);
    if (!boundingRect().contains(point) || !bounds.contains(point))
        return {};
    const qreal scale = diagramScale(bounds, m_diagram);
    const auto layout = layoutDiagram(m_diagram, bounds, scale);
    const QFontMetricsF metrics(labelFont(scale, true));
    QPainterPathStroker hit;
    hit.setWidth(qMax<qreal>(10, 12 * scale));
    for (auto it = layout.edges.crbegin(); it != layout.edges.crend(); ++it) {
        if (edgeLabel(*it, layout.nodes, metrics, bounds, scale).contains(point) || hit.createStroke(it->route).contains(point))
            return it->id;
    }
    return {};
}

QStringList DiagramItem::nodesInLasso(const QVariantList &points) const
{
    const QRectF bounds = diagramBounds();
    const QRectF viewport = boundingRect().intersected(bounds);
    if (viewport.isEmpty() || points.size() < 3 || points.size() > maximumLassoPoints)
        return {};

    QPolygonF polygon;
    polygon.reserve(points.size());
    for (const QVariant &value : points) {
        if (value.metaType() != QMetaType::fromType<QPointF>())
            return {};
        const auto point = value.toPointF();
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            return {};
        polygon.append({qBound(viewport.left(), point.x(), viewport.right()),
                        qBound(viewport.top(), point.y(), viewport.bottom())});
    }

    QStringList selected;
    const qreal scale = diagramScale(bounds, m_diagram);
    const auto nodes = m_diagram.value(QStringLiteral("nodes")).toList();
    for (const QVariant &entry : nodes) {
        const Node node = layoutNode(entry.toMap(), bounds, scale);
        if (!node.id.isEmpty() && !node.outline.isEmpty() && viewport.contains(node.rect.center())
            && polygon.containsPoint(node.rect.center(), Qt::OddEvenFill))
            selected.append(node.id);
    }
    return selected;
}
