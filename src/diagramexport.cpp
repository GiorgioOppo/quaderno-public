#include "diagramexport.h"

#include "pagecontent.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPointF>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QXmlStreamWriter>

#include <cmath>

namespace {
constexpr qreal pageWidth = 800;
constexpr qreal pageHeight = 1067;

QString drawioStyle(const QString &shape)
{
    static const QHash<QString, QString> shapes{
        {QStringLiteral("process"), QStringLiteral("shape=rectangle;")},
        {QStringLiteral("terminal"), QStringLiteral("shape=rectangle;rounded=1;arcSize=50;roundedPerimeter=1;")},
        {QStringLiteral("decision"), QStringLiteral("shape=rhombus;perimeter=rhombusPerimeter;")},
        {QStringLiteral("data"), QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;")},
        {QStringLiteral("note"), QStringLiteral("shape=rectangle;dashed=1;")},
    };
    return shapes.value(shape) + QStringLiteral("whiteSpace=wrap;html=0;align=center;verticalAlign=middle;"
                                                "fillColor=#ffffff;strokeColor=#202020;fontColor=#202020;fontSize=16;");
}

void geometry(QXmlStreamWriter &xml, const QVariantMap &node, qreal width, qreal height)
{
    xml.writeStartElement(QStringLiteral("mxGeometry"));
    xml.writeAttribute(QStringLiteral("as"), QStringLiteral("geometry"));
    xml.writeAttribute(QStringLiteral("x"), QString::number(node.value(QStringLiteral("x")).toReal() * width, 'g', 15));
    xml.writeAttribute(QStringLiteral("y"), QString::number(node.value(QStringLiteral("y")).toReal() * height, 'g', 15));
    xml.writeAttribute(QStringLiteral("width"), QString::number(node.value(QStringLiteral("width")).toReal() * width, 'g', 15));
    xml.writeAttribute(QStringLiteral("height"), QString::number(node.value(QStringLiteral("height")).toReal() * height, 'g', 15));
    xml.writeEndElement();
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
    QSaveFile output(destination.absoluteFilePath());
    if (!output.open(QIODevice::WriteOnly))
        return tr("Impossibile salvare il diagramma. %1").arg(output.errorString());

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
        centers.insert(sourceId, {(node.value(QStringLiteral("x")).toReal() + node.value(QStringLiteral("width")).toReal() / 2) * width,
                                  (node.value(QStringLiteral("y")).toReal() + node.value(QStringLiteral("height")).toReal() / 2) * height});
        xml.writeStartElement(QStringLiteral("mxCell"));
        xml.writeAttribute(QStringLiteral("id"), id);
        xml.writeAttribute(QStringLiteral("vertex"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("parent"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("value"), node.value(QStringLiteral("text")).toString());
        xml.writeAttribute(QStringLiteral("style"), drawioStyle(node.value(QStringLiteral("shape")).toString()));
        geometry(xml, node, width, height);
        xml.writeEndElement();
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
    if (xml.hasError()) {
        output.cancelWriting();
        return tr("Il diagramma contiene testo che non può essere esportato.");
    }
    if (!output.commit())
        return tr("Impossibile salvare il diagramma. %1").arg(output.errorString());
    return {};
}
