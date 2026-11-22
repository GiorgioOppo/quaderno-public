#include "diagramexport.h"

#include "pagecontent.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QXmlStreamWriter>

#include <cmath>

namespace {
constexpr qreal pageWidth = 800;
constexpr qreal pageHeight = 1067;
const QString compartmentBreak = QStringLiteral("\n\n");
const QString interfaceStereotype = QStringLiteral("«interface»");

QString drawioStyle(const QString &shape)
{
    static const QHash<QString, QString> shapes{
        {QStringLiteral("process"), QStringLiteral("shape=rectangle;")},
        {QStringLiteral("terminal"), QStringLiteral("shape=rectangle;rounded=1;arcSize=50;roundedPerimeter=1;")},
        {QStringLiteral("decision"), QStringLiteral("shape=rhombus;perimeter=rhombusPerimeter;")},
        {QStringLiteral("data"), QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;")},
        {QStringLiteral("note"), QStringLiteral("shape=rectangle;dashed=1;")},
        {QStringLiteral("database"), QStringLiteral("shape=mxgraph.flowchart.database;")},
        {QStringLiteral("subprocess"), QStringLiteral("shape=process;size=0.13;")},
        {QStringLiteral("display"), QStringLiteral("shape=mxgraph.flowchart.display;")},
        {QStringLiteral("manual-input"), QStringLiteral("shape=manualInput;boundedLbl=1;size=26;")},
        {QStringLiteral("preparation"), QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;size=0.22;")},
        {QStringLiteral("delay"), QStringLiteral("shape=mxgraph.flowchart.delay;")},
        {QStringLiteral("off-page"), QStringLiteral("shape=offPageConnector;size=0.31;")},
        {QStringLiteral("cloud"), QStringLiteral("shape=cloud;")},
        {QStringLiteral("uml-actor"), QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;")},
        {QStringLiteral("uml-use-case"), QStringLiteral("shape=ellipse;perimeter=ellipsePerimeter;quadernoUml=uml-use-case;")},
        {QStringLiteral("uml-component"), QStringLiteral("shape=module;jettyWidth=10;jettyHeight=4;")},
        {QStringLiteral("uml-package"), QStringLiteral("shape=folder;tabWidth=40;tabHeight=18;tabPosition=left;boundedLbl=1;")},
        {QStringLiteral("uml-deployment"), QStringLiteral("shape=cube;size=10;direction=south;boundedLbl=1;")},
        {QStringLiteral("uml-lifeline"), QStringLiteral("shape=umlLifeline;perimeter=lifelinePerimeter;size=30;")},
    };
    const QString verticalAlign = shape == QStringLiteral("uml-actor") ? QString{} : QStringLiteral("verticalAlign=middle;");
    return shapes.value(shape) + verticalAlign + QStringLiteral("whiteSpace=wrap;html=0;align=center;"
                                                               "fillColor=#ffffff;strokeColor=#202020;fontColor=#202020;fontSize=16;");
}

void geometry(QXmlStreamWriter &xml, const QRectF &rect)
{
    xml.writeStartElement(QStringLiteral("mxGeometry"));
    xml.writeAttribute(QStringLiteral("as"), QStringLiteral("geometry"));
    xml.writeAttribute(QStringLiteral("x"), QString::number(rect.x(), 'g', 15));
    xml.writeAttribute(QStringLiteral("y"), QString::number(rect.y(), 'g', 15));
    xml.writeAttribute(QStringLiteral("width"), QString::number(rect.width(), 'g', 15));
    xml.writeAttribute(QStringLiteral("height"), QString::number(rect.height(), 'g', 15));
    xml.writeEndElement();
}

void vertex(QXmlStreamWriter &xml, const QString &id, const QString &parent, const QString &label,
            const QString &style, const QRectF &rect)
{
    xml.writeStartElement(QStringLiteral("mxCell"));
    xml.writeAttribute(QStringLiteral("id"), id);
    xml.writeAttribute(QStringLiteral("vertex"), QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("parent"), parent);
    if (parent != QStringLiteral("1"))
        xml.writeAttribute(QStringLiteral("connectable"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("value"), label);
    xml.writeAttribute(QStringLiteral("style"), style);
    geometry(xml, rect);
    xml.writeEndElement();
}

void classifier(QXmlStreamWriter &xml, const QString &id, const QString &shape, const QString &label, const QRectF &rect)
{
    const bool isObject = shape == QStringLiteral("uml-object");
    const bool isInterface = shape == QStringLiteral("uml-interface");
    const qsizetype compartmentCount = isObject ? 2 : 3;
    auto parts = label.split(compartmentBreak);
    while (parts.size() > compartmentCount) {
        const QString tail = parts.takeLast();
        parts.last() += compartmentBreak + tail;
    }
    QString heading = parts.first();
    const qreal headerHeight = rect.height() * (isInterface ? 0.38 : 0.28);
    QString style = QStringLiteral("swimlane;quadernoUml=%1;horizontal=1;startSize=%2;collapsible=0;"
                                   "childLayout=stackLayout;horizontalStack=0;resizeParent=1;resizeLast=0;"
                                   "spacing=2;fontStyle=%3;").arg(shape, QString::number(headerHeight),
                                       isObject ? QStringLiteral("4") : QStringLiteral("1"));
    if (isInterface && !heading.startsWith(interfaceStereotype, Qt::CaseInsensitive)
        && !heading.startsWith(QStringLiteral("<<interface>>"), Qt::CaseInsensitive)) {
        heading.prepend(interfaceStereotype + u'\n');
        style += QStringLiteral("quadernoUmlStereotype=1;");
    }
    vertex(xml, id, QStringLiteral("1"), heading, style + drawioStyle(shape), rect);

    const qreal dividerHeight = isObject ? 0 : qMin(4.0, rect.height() * 0.05);
    const qreal sectionHeight = (rect.height() - headerHeight - dividerHeight) / (compartmentCount - 1);
    for (qsizetype part = 1; part < compartmentCount; ++part) {
        qreal y = headerHeight + (part - 1) * sectionHeight;
        if (part > 1) {
            vertex(xml, id + QStringLiteral("-divider"), id, {},
                   QStringLiteral("shape=line;strokeColor=#202020;resizeWidth=1;"),
                   {0, y, rect.width(), dividerHeight});
            y += dividerHeight;
        }
        QString sectionStyle = QStringLiteral("text;html=0;strokeColor=none;fillColor=none;align=left;spacingLeft=5;"
                                              "verticalAlign=top;resizeWidth=1;whiteSpace=wrap;overflow=hidden;");
        if (part >= parts.size())
            sectionStyle += QStringLiteral("quadernoEmptySection=1;");
        vertex(xml, id + QStringLiteral("-section-%1").arg(part), id, parts.value(part), sectionStyle,
               {0, y, rect.width(), sectionHeight});
    }
}
}

QUrl DiagramExport::suggestedFile(const QString &title) const
{
    QString name = title.left(80).trimmed();
    name.replace(QRegularExpression(QStringLiteral("[/\\\\:\\p{Cc}]")), QStringLiteral("_"));
    if (name.isEmpty() || name.startsWith(u'.'))
        name = tr("Diagramma");
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!QFileInfo(directory).isDir())
        directory = QDir::homePath();
    return QUrl::fromLocalFile(QDir(directory).filePath(name + QStringLiteral(".drawio")));
}

QString DiagramExport::save(const QUrl &file, const QVariantMap &diagram, const QString &title) const
{
    const QFileInfo destination(file.toLocalFile());
    if (!file.isLocalFile() || !destination.isAbsolute() || destination.isSymLink()
        || (!file.host().isEmpty() && file.host() != QStringLiteral("localhost"))
        || destination.suffix().compare(QStringLiteral("drawio"), Qt::CaseInsensitive) != 0)
        return tr("Scegli un file .drawio da salvare.");

    QSaveFile output(destination.absoluteFilePath());
    if (!output.open(QIODevice::WriteOnly))
        return tr("Impossibile salvare il diagramma. %1").arg(output.errorString());
    const QString error = write(output, diagram, title);
    if (!error.isEmpty()) { output.cancelWriting(); return error; }
    if (!output.commit())
        return tr("Impossibile salvare il diagramma. %1").arg(output.errorString());
    return {};
}

QString DiagramExport::write(QIODevice &output, const QVariantMap &diagram, const QString &title)
{
    if (!output.isWritable())
        return tr("Impossibile salvare il diagramma. %1").arg(output.errorString());
    auto candidate = diagram;
    if (candidate.value(QStringLiteral("id")).toString().isEmpty()) {
        candidate.insert(QStringLiteral("id"), QStringLiteral("diagram"));
        candidate.insert(QStringLiteral("x"), 0.0);
        candidate.insert(QStringLiteral("y"), 0.0);
        candidate.insert(QStringLiteral("width"), 0.84);
        candidate.insert(QStringLiteral("height"), PageContent::preferredItemHeight(candidate, 0.84));
    }
    const auto valid = PageContent::validatedItem(candidate);
    if (valid.value(QStringLiteral("type")).toString() != QStringLiteral("diagram"))
        return tr("Impossibile esportare il diagramma. Controlla le forme e i collegamenti.");

    const auto nodes = valid.value(QStringLiteral("nodes")).toList();
    const auto edges = valid.value(QStringLiteral("edges")).toList();
    const int width = qMax(1, qRound(valid.value(QStringLiteral("width")).toReal() * pageWidth));
    const int height = qMax(1, qRound(valid.value(QStringLiteral("height")).toReal() * pageHeight));
    QXmlStreamWriter xml(&output);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("mxfile"));
    xml.writeStartElement(QStringLiteral("diagram"));
    xml.writeAttribute(QStringLiteral("id"), QStringLiteral("quaderno"));
    xml.writeAttribute(QStringLiteral("name"), title.isEmpty() ? tr("Diagramma") : title);
    xml.writeStartElement(QStringLiteral("mxGraphModel"));
    xml.writeAttribute(QStringLiteral("pageWidth"), QString::number(width));
    xml.writeAttribute(QStringLiteral("pageHeight"), QString::number(height));
    xml.writeAttribute(QStringLiteral("page"), QStringLiteral("1"));
    xml.writeStartElement(QStringLiteral("root"));
    xml.writeEmptyElement(QStringLiteral("mxCell"));
    xml.writeAttribute(QStringLiteral("id"), QStringLiteral("0"));
    xml.writeEmptyElement(QStringLiteral("mxCell"));
    xml.writeAttribute(QStringLiteral("id"), QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("parent"), QStringLiteral("0"));

    QHash<QString, QString> identifiers;
    QHash<QString, QPointF> centers;
    for (const QVariant &entry : nodes) {
        const auto node = entry.toMap();
        const auto id = QStringLiteral("node-%1").arg(identifiers.size() + 1);
        const QString sourceId = node.value(QStringLiteral("id")).toString();
        identifiers.insert(sourceId, id);
        const QRectF rect(node.value(QStringLiteral("x")).toReal() * width, node.value(QStringLiteral("y")).toReal() * height,
                          node.value(QStringLiteral("width")).toReal() * width, node.value(QStringLiteral("height")).toReal() * height);
        centers.insert(sourceId, rect.center());
        const QString shape = node.value(QStringLiteral("shape")).toString();
        const QString label = node.value(QStringLiteral("text")).toString();
        if (shape == QStringLiteral("uml-class") || shape == QStringLiteral("uml-interface") || shape == QStringLiteral("uml-object"))
            classifier(xml, id, shape, label, rect);
        else
            vertex(xml, id, QStringLiteral("1"), label, drawioStyle(shape), rect);
    }
    QSet<QPair<QString, QString>> connections;
    for (const QVariant &entry : edges) {
        const auto edge = entry.toMap();
        connections.insert({edge.value(QStringLiteral("from")).toString(), edge.value(QStringLiteral("to")).toString()});
    }
    for (qsizetype index = 0; index < edges.size(); ++index) {
        const auto edge = edges.at(index).toMap();
        const auto from = edge.value(QStringLiteral("from")).toString();
        const auto to = edge.value(QStringLiteral("to")).toString();
        const bool reciprocal = connections.contains({to, from});
        const bool orthogonal = edge.value(QStringLiteral("route")).toString() == QStringLiteral("orthogonal");
        xml.writeStartElement(QStringLiteral("mxCell"));
        xml.writeAttribute(QStringLiteral("id"), QStringLiteral("edge-%1").arg(index + 1));
        xml.writeAttribute(QStringLiteral("edge"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("parent"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("source"), identifiers.value(from));
        xml.writeAttribute(QStringLiteral("target"), identifiers.value(to));
        xml.writeAttribute(QStringLiteral("value"), edge.value(QStringLiteral("text")).toString());
        QString style = QStringLiteral("html=0;endArrow=block;endFill=1;strokeColor=#202020;fontColor=#202020;");
        if (orthogonal)
            style += QStringLiteral("edgeStyle=orthogonalEdgeStyle;rounded=0;");
        else if (reciprocal)
            style += QStringLiteral("edgeStyle=none;curved=1;labelOnCurve=1;");
        xml.writeAttribute(QStringLiteral("style"), style);
        xml.writeStartElement(QStringLiteral("mxGeometry"));
        xml.writeAttribute(QStringLiteral("relative"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("as"), QStringLiteral("geometry"));
        if (reciprocal) {
            const QPointF start = centers.value(from);
            const QPointF end = centers.value(to);
            const QPointF delta = end - start;
            const qreal distance = std::hypot(delta.x(), delta.y());
            const QPointF normal = distance > 0 ? QPointF(-delta.y(), delta.x()) / distance
                                                 : QPointF(0, from < to ? 1 : -1);
            const QPointF waypoint = (start + end) / 2 + normal * qBound(16.0, distance * 0.2, 50.0);
            xml.writeStartElement(QStringLiteral("Array"));
            xml.writeAttribute(QStringLiteral("as"), QStringLiteral("points"));
            xml.writeEmptyElement(QStringLiteral("mxPoint"));
            xml.writeAttribute(QStringLiteral("x"), QString::number(waypoint.x()));
            xml.writeAttribute(QStringLiteral("y"), QString::number(waypoint.y()));
            xml.writeEndElement();
        }
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndDocument();
    if (xml.hasError())
        return tr("Il diagramma contiene testo che non può essere esportato.");
    return {};
}
