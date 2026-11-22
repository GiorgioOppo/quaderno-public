#include "diagramitem.h"

#include <QFontMetricsF>
#include <QHash>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QSet>
#include <QTextBoundaryFinder>
#include <QTextOption>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace {
enum class Shape {
    Unknown, Process, Terminal, Decision, Data, Note,
    Database, Subprocess, Display, ManualInput, Preparation, Delay, OffPage, Cloud,
    UmlClass, UmlInterface, UmlObject, UmlActor, UmlUseCase, UmlComponent, UmlPackage, UmlDeployment, UmlLifeline
};

Shape shapeKind(const QString &shape)
{
    static const QHash<QString, Shape> kinds{
        {QStringLiteral("process"), Shape::Process}, {QStringLiteral("terminal"), Shape::Terminal},
        {QStringLiteral("decision"), Shape::Decision}, {QStringLiteral("data"), Shape::Data},
        {QStringLiteral("note"), Shape::Note}, {QStringLiteral("database"), Shape::Database},
        {QStringLiteral("subprocess"), Shape::Subprocess}, {QStringLiteral("display"), Shape::Display},
        {QStringLiteral("manual-input"), Shape::ManualInput}, {QStringLiteral("preparation"), Shape::Preparation},
        {QStringLiteral("delay"), Shape::Delay}, {QStringLiteral("off-page"), Shape::OffPage},
        {QStringLiteral("cloud"), Shape::Cloud},
        {QStringLiteral("uml-class"), Shape::UmlClass}, {QStringLiteral("uml-interface"), Shape::UmlInterface},
        {QStringLiteral("uml-object"), Shape::UmlObject}, {QStringLiteral("uml-actor"), Shape::UmlActor},
        {QStringLiteral("uml-use-case"), Shape::UmlUseCase}, {QStringLiteral("uml-component"), Shape::UmlComponent},
        {QStringLiteral("uml-package"), Shape::UmlPackage}, {QStringLiteral("uml-deployment"), Shape::UmlDeployment},
        {QStringLiteral("uml-lifeline"), Shape::UmlLifeline},
    };
    return kinds.value(shape, Shape::Unknown);
}

qreal terminalRadius(const QRectF &rect)
{
    return qMin(rect.height() * .5, rect.width() * .23);
}

qreal dataSlant(const QRectF &rect)
{
    return qMin(rect.width() * .17, rect.height() * .40);
}

qreal cylinderCap(const QRectF &rect)
{
    return qMin(rect.height() * .22, rect.width() * .24);
}

QRectF actorBody(const QRectF &rect)
{
    const qreal height = rect.height() * .75;
    const qreal width = qMin(rect.width() * .84, height * .72);
    return {rect.center().x() - width / 2, rect.top() + rect.height() * .025, width, height};
}

QRectF lifelineHead(const QRectF &rect)
{
    return {rect.topLeft(), QSizeF(rect.width(), qMin(rect.height() * .4, rect.width() * .52))};
}

qreal packageTabHeight(const QRectF &rect)
{
    return qMin(rect.height() * .2, rect.width() * .14);
}

qreal deploymentDepth(const QRectF &rect)
{
    return qMin(rect.width() * .19, rect.height() * .24);
}

QPainterPath outlineForShape(Shape shape, const QRectF &rect)
{
    QPainterPath path;
    if (!rect.isValid())
        return path;

    const qreal left = rect.left(), right = rect.right(), top = rect.top(), bottom = rect.bottom();
    const qreal width = rect.width(), height = rect.height();
    const auto center = rect.center();
    const auto polygon = [&path](std::initializer_list<QPointF> points) {
        path.addPolygon(QPolygonF(points));
        path.closeSubpath();
    };
    switch (shape) {
    case Shape::Terminal: {
        const qreal radius = terminalRadius(rect);
        path.addRoundedRect(rect, radius, radius);
        break;
    }
    case Shape::Decision:
        polygon({{center.x(), top}, {right, center.y()}, {center.x(), bottom}, {left, center.y()}});
        break;
    case Shape::Data: {
        const qreal slant = dataSlant(rect);
        polygon({{left + slant, top}, rect.topRight(), {right - slant, bottom}, rect.bottomLeft()});
        break;
    }
    case Shape::Database: {
        const qreal cap = cylinderCap(rect);
        path.moveTo(left, top + cap);
        path.arcTo(QRectF(left, top, width, 2 * cap), 180, -180);
        path.lineTo(right, bottom - cap);
        path.arcTo(QRectF(left, bottom - 2 * cap, width, 2 * cap), 0, -180);
        path.closeSubpath();
        break;
    }
    case Shape::Display:
    case Shape::Delay: {
        const bool display = shape == Shape::Display;
        const qreal cap = display ? qMin(width * .30, height * .50) : qMin(width * .58, height * .60);
        const qreal bevel = display ? qMin(width * .18, height * .40) : 0;
        path.moveTo(left + bevel, top);
        path.lineTo(right - cap, top);
        path.arcTo(QRectF(right - 2 * cap, top, 2 * cap, height), 90, -180);
        path.lineTo(left + bevel, bottom);
        if (display)
            path.lineTo(left, center.y());
        path.closeSubpath();
        break;
    }
    case Shape::ManualInput:
        polygon({{left, top + height * .25}, rect.topRight(), rect.bottomRight(), rect.bottomLeft()});
        break;
    case Shape::Preparation: {
        const qreal inset = qMin(width * .22, height * .45);
        polygon({{left + inset, top}, {right - inset, top}, {right, center.y()},
            {right - inset, bottom}, {left + inset, bottom}, {left, center.y()}});
        break;
    }
    case Shape::OffPage:
        polygon({rect.topLeft(), rect.topRight(), {right, bottom - height * .31},
            {center.x(), bottom}, {left, bottom - height * .31}});
        break;
    case Shape::Cloud: {
        const auto at = [&](qreal x, qreal y) { return QPointF(left + width * x, top + height * y); };
        path.moveTo(at(.19, .79));
        path.cubicTo(at(.06, .83), at(.005, .72), at(.026, .60));
        path.cubicTo(at(.005, .49), at(.07, .36), at(.18, .37));
        path.cubicTo(at(.15, .22), at(.29, .11), at(.41, .17));
        path.cubicTo(at(.48, .005), at(.65, .02), at(.72, .17));
        path.cubicTo(at(.84, .13), at(.95, .24), at(.92, .37));
        path.cubicTo(at(.99, .42), at(.995, .53), at(.966, .61));
        path.cubicTo(at(.998, .74), at(.88, .85), at(.76, .81));
        path.cubicTo(at(.70, .98), at(.53, .997), at(.44, .86));
        path.cubicTo(at(.35, .96), at(.21, .91), at(.19, .79));
        path.closeSubpath();
        break;
    }
    case Shape::UmlActor: {
        // Treat the stick figure as one interactive silhouette, including the
        // gaps between its strokes, without drawing a surrounding box.
        const QRectF body = actorBody(rect);
        path.addRoundedRect(body, body.width() * .2, body.width() * .2);
        break;
    }
    case Shape::UmlUseCase:
        path.addEllipse(rect);
        break;
    case Shape::UmlPackage: {
        const qreal tab = packageTabHeight(rect);
        polygon({rect.topLeft(), {left + width * .38, top}, {left + width * .47, top + tab},
            {right, top + tab}, rect.bottomRight(), rect.bottomLeft()});
        break;
    }
    case Shape::UmlDeployment: {
        const qreal depth = deploymentDepth(rect);
        polygon({{left, top + depth}, {left + depth, top}, rect.topRight(),
            {right, bottom - depth}, {right - depth, bottom}, rect.bottomLeft()});
        break;
    }
    case Shape::UmlLifeline: {
        const qreal stemTop = lifelineHead(rect).bottom();
        const qreal stem = qMin(width * .10, height * .04);
        polygon({rect.topLeft(), rect.topRight(), {right, stemTop}, {center.x() + stem, stemTop},
            {center.x() + stem, bottom}, {center.x() - stem, bottom}, {center.x() - stem, stemTop}, {left, stemTop}});
        break;
    }
    case Shape::Unknown:
    case Shape::Process:
    case Shape::Note:
    case Shape::Subprocess:
    case Shape::UmlClass:
    case Shape::UmlInterface:
    case Shape::UmlObject:
    case Shape::UmlComponent:
        path.addRect(rect);
        break;
    }
    return path;
}
}

QPainterPath diagramNodeOutline(const QString &shape, const QRectF &bounds)
{
    return outlineForShape(shapeKind(shape), bounds);
}

bool diagramShapeRequiresNewFormat(const QString &shape)
{
    const auto kind = shapeKind(shape);
    return kind >= Shape::Database && kind <= Shape::Cloud;
}

bool diagramShapeRequiresUmlFormat(const QString &shape)
{
    return shapeKind(shape) >= Shape::UmlClass;
}

namespace {
const QColor outline(QStringLiteral("#252525"));
const QColor selection(QStringLiteral("#e1e1e1"));
constexpr qsizetype maximumLassoPoints = 4096;

struct Node {
    QString id;
    QString text;
    Shape shape = Shape::Unknown;
    QRectF rect;
    QRectF label;
    QPainterPath outline;
    QPainterPath visibleOutline;
    QPainterPath details;
    QPainterPath dashedDetails;
    bool dashed = false;
    bool nonConvex = false;
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

QRectF paddedLabel(const QRectF &rect, qreal scale)
{
    const qreal horizontal = qMin(5 * scale, rect.width() * .12);
    const qreal vertical = qMin(3 * scale, rect.height() * .10);
    return rect.adjusted(horizontal, vertical, -horizontal, -vertical);
}

int umlCompartmentCount(Shape shape)
{
    if (shape == Shape::UmlClass || shape == Shape::UmlInterface)
        return 3;
    return shape == Shape::UmlObject ? 2 : 0;
}

QRectF umlCompartment(const QRectF &rect, Shape shape, int index)
{
    const qreal titleHeight = rect.height() * (shape == Shape::UmlClass ? .30 : .40);
    if (index == 0)
        return {rect.topLeft(), QSizeF(rect.width(), titleHeight)};
    const qreal rowHeight = (rect.height() - titleHeight) / (umlCompartmentCount(shape) - 1);
    return {rect.left(), rect.top() + titleHeight + (index - 1) * rowHeight, rect.width(), rowHeight};
}

Node layoutNode(const QVariantMap &data, const QRectF &bounds, qreal scale)
{
    Node node;
    node.id = data.value(QStringLiteral("id")).toString();
    node.text = data.value(QStringLiteral("text")).toString();
    const auto shape = shapeKind(data.value(QStringLiteral("shape")).toString());
    node.shape = shape;
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
    node.outline = outlineForShape(shape, r);
    node.visibleOutline = node.outline;
    node.dashed = shape == Shape::Note;
    node.nonConvex = shape == Shape::Cloud || shape == Shape::UmlPackage || shape == Shape::UmlLifeline;
    node.label = paddedLabel(r, scale);
    switch (shape) {
    case Shape::Terminal: {
        const qreal radius = terminalRadius(r);
        node.label.adjust(radius * 0.32, 0, -radius * 0.32, 0);
        break;
    }
    case Shape::Decision:
        node.label = r.adjusted(r.width() * 0.20, r.height() * 0.29, -r.width() * 0.20, -r.height() * 0.29);
        break;
    case Shape::Data: {
        const qreal slant = dataSlant(r);
        node.label.adjust(slant * 0.62, 0, -slant * 0.62, 0);
        break;
    }
    case Shape::Database: {
        const qreal cap = cylinderCap(r);
        node.details.moveTo(r.left(), r.top() + cap);
        node.details.arcTo(QRectF(r.left(), r.top(), r.width(), 2 * cap), 180, 180);
        node.label.adjust(0, cap * 1.7, 0, -cap * .26);
        break;
    }
    case Shape::Subprocess: {
        const qreal inset = qMin(r.width() * .13, r.height() * .25);
        node.details.moveTo(r.left() + inset, r.top());
        node.details.lineTo(r.left() + inset, r.bottom());
        node.details.moveTo(r.right() - inset, r.top());
        node.details.lineTo(r.right() - inset, r.bottom());
        node.label.adjust(inset, 0, -inset, 0);
        break;
    }
    case Shape::Display:
        node.label.adjust(r.width() * .12, 0, -r.width() * .12, 0);
        break;
    case Shape::ManualInput:
        node.label.adjust(0, r.height() * .15, 0, 0);
        break;
    case Shape::Preparation: {
        const qreal inset = qMin(r.width() * .22, r.height() * .45);
        node.label.adjust(inset * .7, 0, -inset * .7, 0);
        break;
    }
    case Shape::Delay:
        node.label.adjust(0, 0, -qMin(r.width() * .58, r.height() * .60) * .45, 0);
        break;
    case Shape::OffPage:
        node.label.adjust(0, 0, 0, -r.height() * .25);
        break;
    case Shape::Cloud:
        node.label = r.adjusted(r.width() * .17, r.height() * .26, -r.width() * .16, -r.height() * .24);
        break;
    case Shape::UmlClass:
    case Shape::UmlInterface:
    case Shape::UmlObject:
        for (int row = 1; row < umlCompartmentCount(shape); ++row) {
            const auto compartment = umlCompartment(r, shape, row);
            node.details.moveTo(compartment.topLeft());
            node.details.lineTo(compartment.topRight());
        }
        break;
    case Shape::UmlActor: {
        node.visibleOutline.clear();
        const QRectF body = actorBody(r);
        const auto at = [&](qreal x, qreal y) { return QPointF(body.x() + x * body.width(), body.y() + y * body.height()); };
        const qreal radius = qMin(body.width() * .23, body.height() * .125);
        const QPointF head = at(.5, .15);
        const QPointF hips = at(.5, .62);
        node.details.addEllipse(head, radius, radius);
        node.details.moveTo(head + QPointF(0, radius));
        node.details.lineTo(hips);
        node.details.moveTo(at(.08, .40));
        node.details.lineTo(at(.92, .40));
        node.details.moveTo(at(.16, .97));
        node.details.lineTo(hips);
        node.details.lineTo(at(.84, .97));
        node.label = paddedLabel(r.adjusted(0, body.bottom() - r.top(), 0, 0), scale);
        break;
    }
    case Shape::UmlUseCase:
        node.label = r.adjusted(r.width() * .14, r.height() * .18, -r.width() * .14, -r.height() * .18);
        break;
    case Shape::UmlComponent: {
        const qreal size = qMin(r.width(), r.height()) * .33;
        const QRectF icon(r.right() - size * 1.24, r.top() + size * .24, size, size);
        const qreal slotHeight = size * .18;
        node.details.moveTo(icon.left(), icon.top() + slotHeight);
        node.details.lineTo(icon.topLeft());
        node.details.lineTo(icon.topRight());
        node.details.lineTo(icon.bottomRight());
        node.details.lineTo(icon.bottomLeft());
        node.details.lineTo(icon.left(), icon.bottom() - slotHeight);
        node.details.moveTo(icon.left(), icon.top() + slotHeight * 2);
        node.details.lineTo(icon.left(), icon.bottom() - slotHeight * 2);
        const qreal slotX = icon.left() - size * .2;
        node.details.addRect(QRectF(slotX, icon.top() + slotHeight, size * .44, slotHeight));
        node.details.addRect(QRectF(slotX, icon.bottom() - slotHeight * 2, size * .44, slotHeight));
        node.label.adjust(0, size * .45, -size * .4, 0);
        break;
    }
    case Shape::UmlPackage:
        node.label = paddedLabel(r.adjusted(0, packageTabHeight(r), 0, 0), scale);
        break;
    case Shape::UmlDeployment: {
        const qreal depth = deploymentDepth(r);
        const QPointF frontCorner(r.right() - depth, r.top() + depth);
        node.details.moveTo(r.left(), r.top() + depth);
        node.details.lineTo(frontCorner);
        node.details.lineTo(r.topRight());
        node.details.moveTo(frontCorner);
        node.details.lineTo(r.right() - depth, r.bottom());
        node.label = paddedLabel(r.adjusted(0, depth, -depth, 0), scale);
        break;
    }
    case Shape::UmlLifeline: {
        const QRectF head = lifelineHead(r);
        node.visibleOutline.clear();
        node.visibleOutline.addRect(head);
        node.dashedDetails.moveTo(head.center().x(), head.bottom());
        node.dashedDetails.lineTo(head.center().x(), r.bottom());
        node.label = paddedLabel(head, scale);
        break;
    }
    case Shape::Unknown:
    case Shape::Process:
    case Shape::Note:
        break;
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
    const QPointF start = node.outline.contains(origin) ? origin : node.rect.center();
    const QPointF ray = unitVector(direction) * std::hypot(node.rect.width(), node.rect.height());
    if (node.nonConvex) {
        // Use the outermost crossing for outlines with lobes or tabs.
        const QLineF probe(start, start + ray);
        const auto polygon = node.outline.toFillPolygon();
        QPointF outer = start;
        for (qsizetype index = 1; index < polygon.size(); ++index) {
            QPointF crossing;
            if (probe.intersects(QLineF(polygon.at(index - 1), polygon.at(index)), &crossing) == QLineF::BoundedIntersection
                && QPointF::dotProduct(crossing - outer, ray) > 0)
                outer = crossing;
        }
        return outer;
    }
    // The remaining outlines are convex and meet a ray from the interior once.
    qreal near = 0;
    qreal far = 1;
    for (int step = 0; step < 18; ++step) {
        const qreal middle = (near + far) / 2;
        if (node.outline.contains(start + middle * ray))
            near = middle;
        else
            far = middle;
    }
    return start + far * ray;
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

void drawFittedLabel(QPainter &painter, const QRectF &rect, const QStringList &lines, QFont font,
                     Qt::Alignment alignment = Qt::AlignCenter)
{
    if (rect.isEmpty() || lines.isEmpty())
        return;
    const auto widthFor = [&lines](const QFontMetricsF &metrics) {
        qreal width = 0;
        for (const QString &line : lines)
            width = qMax(width, qMax(metrics.horizontalAdvance(line), metrics.boundingRect(line).width()));
        return width;
    };
    const auto heightFor = [&lines](const QFontMetricsF &metrics) {
        return metrics.height() + (lines.size() - 1) * metrics.lineSpacing();
    };
    const QFontMetricsF initial(font, painter.device());
    const qreal ratio = qMin(rect.width() / qMax(1., widthFor(initial)), rect.height() / heightFor(initial));
    if (ratio < 1.)
        font.setPixelSize(qMax(1, qFloor(font.pixelSize() * ratio)));
    QFontMetricsF metrics(font, painter.device());
    // Integer pixels and font hinting can make the scaled metrics round up.
    if (font.pixelSize() > 1 && (widthFor(metrics) > rect.width() || heightFor(metrics) > rect.height())) {
        font.setPixelSize(font.pixelSize() - 1);
        metrics = QFontMetricsF(font, painter.device());
    }

    painter.save();
    painter.setClipRect(rect, Qt::IntersectClip);
    painter.setFont(font);
    const qreal offset = (alignment & Qt::AlignVCenter) ? qMax(0., (rect.height() - heightFor(metrics)) / 2) : 0;
    qreal top = rect.top() + offset;
    for (const QString &line : lines) {
        const QRectF row(rect.left(), top, rect.width(), metrics.height());
        painter.drawText(row, int(alignment & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter | Qt::TextSingleLine, line);
        top += metrics.lineSpacing();
    }
    painter.restore();
}

QStringList stackedLabel(const QString &text)
{
    QTextBoundaryFinder graphemes(QTextBoundaryFinder::Grapheme, text);
    QStringList lines;
    qsizetype start = 0;
    for (qsizetype end = graphemes.toNextBoundary(); end >= 0; end = graphemes.toNextBoundary()) {
        lines.append(text.mid(start, end - start));
        start = end;
    }
    return lines;
}

void paintNodeLabel(QPainter &painter, const Node &node, qreal scale)
{
    if (node.text.isEmpty() || node.label.isEmpty())
        return;
    QString text = node.text;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(u'\r', u'\n');
    painter.setPen(outline);
    const auto font = labelFont(scale, false);
    if (const int count = umlCompartmentCount(node.shape); count > 0) {
        auto sections = text.split(QStringLiteral("\n\n"));
        while (sections.size() > count) {
            const QString last = sections.takeLast();
            sections.last().append(QStringLiteral("\n\n") + last);
        }
        if (node.shape == Shape::UmlInterface && !sections.first().startsWith(u'«')
            && !sections.first().startsWith(QStringLiteral("<<"))) {
            sections.first().prepend(QStringLiteral("«interface»\n"));
        }
        for (int index = 0; index < sections.size(); ++index) {
            QFont sectionFont = font;
            sectionFont.setBold(index == 0);
            sectionFont.setUnderline(index == 0 && node.shape == Shape::UmlObject);
            drawFittedLabel(painter, paddedLabel(umlCompartment(node.rect, node.shape, index), scale),
                sections.at(index).split(u'\n'), sectionFont, index == 0 ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignTop);
        }
        return;
    }

    const QStringList lines = text.split(u'\n');
    if (node.shape == Shape::Process && lines.size() == 1 && text.size() > 1 && text.size() <= 20
        && node.label.height() >= node.label.width() * 2.6) {
        drawFittedLabel(painter, node.label, stackedLabel(text.trimmed()), font);
        return;
    }
    if (text.size() <= 32 || (lines.size() <= 6 && std::all_of(lines.cbegin(), lines.cend(), [](const QString &line) {
            return line.size() <= 20;
        }))) {
        drawFittedLabel(painter, node.label, lines, font);
        return;
    }

    painter.save();
    painter.setClipRect(node.label, Qt::IntersectClip);
    painter.setFont(font);
    QTextOption labelOption(Qt::AlignCenter);
    labelOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    painter.drawText(node.label, text, labelOption);
    painter.restore();
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

    for (const Node &node : layout.nodes) {
        const bool selected = selectedNodes.contains(node.id);
        const qreal penWidth = (selected ? 2.8 : 1.7) * scale;
        painter.setPen(QPen(outline, penWidth,
                            node.dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(selected ? selection : QColor(Qt::white));
        painter.drawPath(node.visibleOutline);
        if (!node.details.isEmpty()) {
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(node.details);
        }
        if (!node.dashedDetails.isEmpty()) {
            painter.setPen(QPen(outline, penWidth, Qt::DashLine, Qt::FlatCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(node.dashedDetails);
        }
        paintNodeLabel(painter, node, scale);
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
        if (node.outline.contains(point)
            || (node.shape == Shape::UmlActor && !node.text.isEmpty() && node.label.contains(point)))
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
