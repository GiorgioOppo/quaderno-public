#include "diagramexport.h"

#include "pagecontent.h"

#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRectF>
#include <QSet>
#include <QStringConverter>
#include <QStringList>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <QtNumeric>

#include <zlib.h>

#include <cmath>
#include <utility>

namespace {
constexpr qsizetype maxFileBytes = 2 * 1024 * 1024;
constexpr qsizetype maxExpandedBytes = 4 * 1024 * 1024;
constexpr qsizetype maxCells = 4096;
constexpr qsizetype maxNodes = 64;
constexpr qsizetype maxEdges = 128;

QByteArray unpackPage(const QString &encoded)
{
    QByteArray base64;
    base64.reserve(encoded.size());
    for (const QChar c : encoded) {
        if (c == u' ' || c == u'\t' || c == u'\r' || c == u'\n')
            continue;
        if (c.unicode() > 127)
            return {};
        base64.append(c.toLatin1());
    }
    QByteArray compressed = QByteArray::fromBase64(base64, QByteArray::AbortOnBase64DecodingErrors);
    if (compressed.isEmpty())
        return {};

    z_stream stream{};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return {};
    QByteArray escaped(maxExpandedBytes, Qt::Uninitialized);
    stream.next_in = reinterpret_cast<Bytef *>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef *>(escaped.data());
    stream.avail_out = static_cast<uInt>(escaped.size());
    const int status = inflate(&stream, Z_FINISH);
    const bool complete = status == Z_STREAM_END && stream.avail_in == 0;
    const auto length = static_cast<qsizetype>(stream.total_out);
    inflateEnd(&stream);
    if (!complete || length == 0)
        return {};
    escaped.resize(length);

    // Draw.io deflates the UTF-8 bytes escaped by encodeURIComponent.
    const auto isHex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    for (qsizetype i = 0; i < escaped.size(); ++i) {
        if (escaped.at(i) != '%')
            continue;
        if (i + 2 >= escaped.size() || !isHex(escaped.at(i + 1)) || !isHex(escaped.at(i + 2)))
            return {};
        i += 2;
    }
    const QByteArray xml = QByteArray::fromPercentEncoding(escaped);
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    utf8(xml);
    return utf8.hasError() || xml.contains('\0') ? QByteArray{} : xml;
}

struct Cell {
    QString id;
    QString parent;
    QString source;
    QString target;
    QString style;
    QString value;
    QRectF rect;
    bool vertex = false;
    bool edge = false;
    bool hidden = false;
    bool customGeometry = false;
};

struct Page {
    QList<Cell> cells;
    QString name;
    QSizeF pageSize;
    bool extraPages = false;
    bool unsupported = false;
};

QString attribute(const QXmlStreamAttributes &attributes, const QString &key)
{
    return attributes.value(key).toString();
}

qreal number(const QXmlStreamAttributes &attributes, const QString &key, qreal fallback = 0)
{
    const auto value = attributes.value(key);
    bool ok = false;
    const qreal n = value.isNull() ? fallback : value.toDouble(&ok);
    return (value.isNull() || (ok && std::isfinite(n) && qAbs(n) <= 1e8)) ? n : qQNaN();
}

class DiagramReader
{
public:
    explicit DiagramReader(const QByteArray &bytes) : m_xml(bytes) {}

    bool read(Page &page)
    {
        while (next()) {
            if (!m_xml.isStartElement())
                continue;
            if (m_xml.name() == u"mxGraphModel")
                readGraph(page);
            else if (m_xml.name() == u"mxfile")
                readFile(page);
            else
                m_xml.raiseError(QStringLiteral("Unsupported draw.io root"));
            while (next()) {
                if (m_xml.isStartElement())
                    m_xml.raiseError(QStringLiteral("Unexpected XML content"));
            }
            return !m_xml.hasError();
        }
        return false;
    }

private:
    bool next()
    {
        if (m_xml.atEnd())
            return false;
        const auto token = m_xml.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference)
            m_xml.raiseError(QStringLiteral("XML declarations and custom entities are unsupported"));
        return !m_xml.atEnd();
    }

    void skip()
    {
        int depth = 1;
        while (depth > 0 && next()) {
            if (m_xml.isStartElement() && ++depth > 64)
                m_xml.raiseError(QStringLiteral("XML is too deeply nested"));
            else if (m_xml.isEndElement())
                --depth;
        }
    }

    void readFile(Page &page)
    {
        bool hasPage = false;
        while (next() && !m_xml.isEndElement()) {
            if (!m_xml.isStartElement())
                continue;
            if (m_xml.name() != u"diagram") {
                page.unsupported = true;
                skip();
            } else if (hasPage) {
                page.extraPages = true;
                skip();
            } else {
                hasPage = true;
                readPage(page);
            }
        }
        if (!hasPage && !m_xml.hasError())
            m_xml.raiseError(QStringLiteral("Missing diagram page"));
    }

    void readPage(Page &page)
    {
        page.name = attribute(m_xml.attributes(), QStringLiteral("name"));
        bool hasGraph = false;
        QString compressed;
        while (next() && !m_xml.isEndElement()) {
            if (m_xml.isCharacters()) {
                compressed += m_xml.text();
            } else if (m_xml.isStartElement()) {
                if (m_xml.name() != u"mxGraphModel" || hasGraph || !compressed.trimmed().isEmpty()) {
                    m_xml.raiseError(QStringLiteral("Invalid diagram page content"));
                    return;
                }
                hasGraph = true;
                readGraph(page);
            }
        }
        if (m_xml.hasError())
            return;
        if (hasGraph) {
            if (!compressed.trimmed().isEmpty())
                m_xml.raiseError(QStringLiteral("Mixed diagram page content"));
            return;
        }
        const QByteArray decoded = unpackPage(compressed);
        if (decoded.isEmpty()) {
            m_xml.raiseError(QStringLiteral("Invalid compressed page"));
            return;
        }
        // A compressed page contains only one graph, not another mxfile.
        DiagramReader inner(decoded);
        if (!inner.readGraphDocument(page))
            m_xml.raiseError(QStringLiteral("Invalid compressed graph"));
    }

    bool readGraphDocument(Page &page)
    {
        while (next()) {
            if (!m_xml.isStartElement())
                continue;
            if (m_xml.name() != u"mxGraphModel")
                return false;
            readGraph(page);
            while (next()) {
                if (m_xml.isStartElement())
                    return false;
            }
            return !m_xml.hasError();
        }
        return false;
    }

    void readGraph(Page &page)
    {
        const auto attributes = m_xml.attributes();
        page.pageSize = QSizeF(number(attributes, QStringLiteral("pageWidth")), number(attributes, QStringLiteral("pageHeight")));
        bool hasRoot = false;
        while (next() && !m_xml.isEndElement()) {
            if (!m_xml.isStartElement())
                continue;
            if (m_xml.name() == u"root" && !hasRoot) {
                hasRoot = true;
                readCells(page);
            } else {
                page.unsupported = true;
                skip();
            }
        }
        if (!hasRoot && !m_xml.hasError())
            m_xml.raiseError(QStringLiteral("Missing graph cells"));
    }

    void readCells(Page &page)
    {
        QSet<QString> ids;
        while (next() && !m_xml.isEndElement()) {
            if (!m_xml.isStartElement())
                continue;
            Cell cell;
            if (m_xml.name() == u"mxCell") {
                cell = readCell();
            } else if (m_xml.name().compare(u"object", Qt::CaseInsensitive) == 0 || m_xml.name() == u"UserObject") {
                cell = readObject(page);
            } else {
                page.unsupported = true;
                skip();
                continue;
            }
            if (m_xml.hasError())
                return;
            if (cell.id.isEmpty() || cell.id.size() > 256 || ids.contains(cell.id) || page.cells.size() >= maxCells) {
                m_xml.raiseError(QStringLiteral("Invalid or duplicate cell ID"));
                return;
            }
            ids.insert(cell.id);
            page.cells.append(std::move(cell));
        }
    }

    Cell readObject(Page &page)
    {
        const auto attributes = m_xml.attributes();
        Cell cell;
        bool hasCell = false;
        while (next() && !m_xml.isEndElement()) {
            if (!m_xml.isStartElement())
                continue;
            if (m_xml.name() == u"mxCell" && !hasCell) {
                hasCell = true;
                cell = readCell();
            } else {
                page.unsupported = true;
                skip();
            }
        }
        if (attributes.hasAttribute(u"id"))
            cell.id = attribute(attributes, QStringLiteral("id"));
        if (attributes.hasAttribute(u"label"))
            cell.value = attribute(attributes, QStringLiteral("label"));
        page.unsupported |= !hasCell || attributes.hasAttribute(u"link") || attributes.hasAttribute(u"placeholders");
        return cell;
    }

    Cell readCell()
    {
        const auto attributes = m_xml.attributes();
        Cell cell;
        cell.id = attribute(attributes, QStringLiteral("id"));
        cell.parent = attribute(attributes, QStringLiteral("parent"));
        cell.source = attribute(attributes, QStringLiteral("source"));
        cell.target = attribute(attributes, QStringLiteral("target"));
        cell.style = attribute(attributes, QStringLiteral("style"));
        cell.value = attribute(attributes, QStringLiteral("value"));
        cell.vertex = attributes.value(u"vertex") == u"1";
        cell.edge = attributes.value(u"edge") == u"1";
        cell.hidden = attributes.value(u"visible") == u"0";
        bool hasGeometry = false;
        while (next() && !m_xml.isEndElement()) {
            if (!m_xml.isStartElement())
                continue;
            if (m_xml.name() == u"mxGeometry" && !hasGeometry) {
                hasGeometry = true;
                const auto geometry = m_xml.attributes();
                if (geometry.value(u"relative") != u"1") {
                    cell.rect = QRectF(number(geometry, QStringLiteral("x")), number(geometry, QStringLiteral("y")),
                                       number(geometry, QStringLiteral("width")), number(geometry, QStringLiteral("height")));
                }
                while (next() && !m_xml.isEndElement()) {
                    if (m_xml.isStartElement()) {
                        cell.customGeometry = true;
                        skip();
                    }
                }
            } else {
                cell.customGeometry = true;
                skip();
            }
        }
        return cell;
    }

    QXmlStreamReader m_xml;
};

struct Style {
    QHash<QString, QString> options;
    QString name;

    explicit Style(const QString &source)
    {
        const auto entries = QStringView(source).split(u';', Qt::SkipEmptyParts);
        for (const QStringView entry : entries) {
            const qsizetype separator = entry.indexOf(u'=');
            if (separator >= 0)
                options.insert(entry.left(separator).trimmed().toString(), entry.sliced(separator + 1).trimmed().toString());
            else
                name = entry.trimmed().toString();
        }
    }

    bool enabled(const QString &key) const { return options.value(key) == QStringLiteral("1"); }
};

QString nodeShape(const Style &style)
{
    const QString shape = style.options.value(QStringLiteral("shape"), style.name);
    static const QHash<QString, QString> shapes{
        {QStringLiteral("ellipse"), QStringLiteral("terminal")},
        {QStringLiteral("oval"), QStringLiteral("terminal")},
        {QStringLiteral("terminator"), QStringLiteral("terminal")},
        {QStringLiteral("mxgraph.flowchart.terminator"), QStringLiteral("terminal")},
        {QStringLiteral("rhombus"), QStringLiteral("decision")},
        {QStringLiteral("diamond"), QStringLiteral("decision")},
        {QStringLiteral("mxgraph.flowchart.decision"), QStringLiteral("decision")},
        {QStringLiteral("parallelogram"), QStringLiteral("data")},
        {QStringLiteral("mxgraph.flowchart.data"), QStringLiteral("data")},
        {QStringLiteral("mxgraph.flowchart.manual_input"), QStringLiteral("data")},
        {QStringLiteral("note"), QStringLiteral("note")},
        {QStringLiteral("text"), QStringLiteral("note")},
        {QStringLiteral("document"), QStringLiteral("note")},
        {QStringLiteral("mxgraph.flowchart.document"), QStringLiteral("note")},
        {QStringLiteral("mxgraph.flowchart.process"), QStringLiteral("process")},
    };
    if (shape.isEmpty() || shape == QStringLiteral("rectangle")) {
        if (style.enabled(QStringLiteral("rounded")))
            return QStringLiteral("terminal");
        return style.enabled(QStringLiteral("dashed")) ? QStringLiteral("note") : QStringLiteral("process");
    }
    return shapes.value(shape);
}

bool hasSimplifiedStyle(const Style &style, bool edge)
{
    const auto differentColor = [&](const QString &key, const QColor &defaultColor, bool isFill = false) {
        const QString value = style.options.value(key).toLower();
        if (value.isEmpty() || (isFill && value == QStringLiteral("none")))
            return false;
        const QColor color(value);
        return color != defaultColor && (isFill || color != QColor(QStringLiteral("#202020")));
    };
    if ((!edge && differentColor(QStringLiteral("fillColor"), Qt::white, true))
        || differentColor(QStringLiteral("strokeColor"), Qt::black)
        || differentColor(QStringLiteral("fontColor"), Qt::black))
        return true;
    const QString rotation = style.options.value(QStringLiteral("rotation"));
    const QString gradient = style.options.value(QStringLiteral("gradientColor"));
    const QString fontStyle = style.options.value(QStringLiteral("fontStyle"));
    const QString fontSize = style.options.value(QStringLiteral("fontSize"));
    if ((!rotation.isEmpty() && rotation.toDouble() != 0)
        || (!gradient.isEmpty() && gradient != QStringLiteral("none"))
        || (!fontStyle.isEmpty() && fontStyle != QStringLiteral("0"))
        || (!fontSize.isEmpty() && fontSize != QStringLiteral("12") && fontSize != QStringLiteral("16"))
        || style.options.contains(QStringLiteral("image")) || style.enabled(QStringLiteral("shadow"))
        || style.enabled(QStringLiteral("flipH")) || style.enabled(QStringLiteral("flipV"))
        || style.enabled(QStringLiteral("glass")) || style.enabled(QStringLiteral("sketch"))
        || (edge && style.enabled(QStringLiteral("dashed"))))
        return true;
    for (const auto &key : {QStringLiteral("opacity"), QStringLiteral("fillOpacity"),
                            QStringLiteral("strokeOpacity"), QStringLiteral("textOpacity")}) {
        const QString opacity = style.options.value(key);
        if (!opacity.isEmpty() && opacity.toDouble() != 100)
            return true;
    }
    return false;
}

class LabelDocument final : public QTextDocument
{
protected:
    QVariant loadResource(int, const QUrl &) override { return {}; }
};

bool hasEmbeddedImage(const QString &value, const Style &style)
{
    return style.enabled(QStringLiteral("html")) && (value.contains(QStringLiteral("<img"), Qt::CaseInsensitive)
        || value.contains(QStringLiteral("<svg"), Qt::CaseInsensitive));
}

QString plainLabel(const QString &value, bool html, qsizetype maximum, bool &shortened)
{
    constexpr qsizetype maximumMarkup = 16 * 1024;
    QString label = value;
    if (html) {
        shortened |= value.size() > maximumMarkup;
        LabelDocument document;
        document.setHtml(value.left(maximumMarkup));
        label = document.toPlainText();
    }
    label.remove(QChar::ObjectReplacementCharacter);
    label.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    label.replace(u'\r', u'\n');
    if (label.size() > maximum) {
        label.truncate(maximum);
        if (label.back().isHighSurrogate())
            label.chop(1);
        shortened = true;
    }
    return label;
}

struct Node {
    QString originalId;
    QString shape;
    QString label;
    QRectF rect;
};

struct ConvertedDiagram {
    QVariantMap item;
    bool omitted = false;
    bool labelsShortened = false;
    bool appearanceChanged = false;
};

ConvertedDiagram convert(const Page &page)
{
    ConvertedDiagram result;
    result.omitted = page.unsupported;
    QSet<QString> layers;
    for (const auto &cell : page.cells) {
        if (!cell.vertex && !cell.edge && !cell.hidden && cell.parent == QStringLiteral("0"))
            layers.insert(cell.id);
    }

    QList<Node> nodes;
    QRectF bounds;
    for (const auto &cell : page.cells) {
        if (!cell.vertex)
            continue;
        const Style style(cell.style);
        const auto shape = nodeShape(style);
        if (cell.edge || cell.hidden || !layers.contains(cell.parent) || shape.isEmpty() || !cell.rect.isValid()
            || !std::isfinite(cell.rect.x()) || !std::isfinite(cell.rect.y()) || nodes.size() == maxNodes) {
            result.omitted = true;
            continue;
        }
        result.appearanceChanged |= cell.customGeometry || hasSimplifiedStyle(style, false) || hasEmbeddedImage(cell.value, style);
        nodes.append({cell.id, shape, plainLabel(cell.value, style.enabled(QStringLiteral("html")), 200, result.labelsShortened),
                      cell.rect});
        bounds = bounds.united(cell.rect);
    }
    if (nodes.isEmpty())
        return result;

    const QRectF declaredBounds(QPointF(), page.pageSize);
    if (declaredBounds.isValid() && declaredBounds.adjusted(-1e-7, -1e-7, 1e-7, 1e-7).contains(bounds)) {
        bounds = declaredBounds;
    } else {
        const qreal padX = bounds.width() * 0.06;
        const qreal padY = bounds.height() * 0.06;
        bounds.adjust(-padX, -padY, padX, padY);
    }

    QHash<QString, QString> ids;
    QVariantList nodeData;
    nodeData.reserve(nodes.size());
    for (const auto &node : nodes) {
        const QString id = QStringLiteral("node-%1").arg(nodeData.size() + 1);
        const qreal x = qBound(0.0, (node.rect.x() - bounds.x()) / bounds.width(), 1.0);
        const qreal y = qBound(0.0, (node.rect.y() - bounds.y()) / bounds.height(), 1.0);
        const qreal width = qMin(node.rect.width() / bounds.width(), 1 - x);
        const qreal height = qMin(node.rect.height() / bounds.height(), 1 - y);
        if (width <= 0 || height <= 0) {
            result.omitted = true;
            continue;
        }
        ids.insert(node.originalId, id);
        nodeData.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("shape"), node.shape},
                                    {QStringLiteral("text"), node.label}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y},
                                    {QStringLiteral("width"), width}, {QStringLiteral("height"), height}});
    }

    QVariantList edgeData;
    QSet<QPair<QString, QString>> endpoints;
    QSet<QPair<QString, QString>> geometryRequiringPair;
    for (const auto &cell : page.cells) {
        if (!cell.edge)
            continue;
        const Style style(cell.style);
        auto from = ids.value(cell.source);
        auto to = ids.value(cell.target);
        const QString startArrow = style.options.value(QStringLiteral("startArrow"), QStringLiteral("none"));
        const QString endArrow = style.options.value(QStringLiteral("endArrow"), QStringLiteral("block"));
        if (startArrow != QStringLiteral("none") && endArrow == QStringLiteral("none"))
            std::swap(from, to);
        if (cell.vertex || cell.hidden || !layers.contains(cell.parent) || from.isEmpty() || to.isEmpty() || from == to
            || endpoints.contains({from, to}) || edgeData.size() == maxEdges) {
            result.omitted = true;
            continue;
        }
        endpoints.insert({from, to});
        const QString router = style.options.value(QStringLiteral("edgeStyle"));
        const bool orthogonal = router == QStringLiteral("orthogonalEdgeStyle") || router == QStringLiteral("elbowEdgeStyle");
        const QString arrow = endArrow == QStringLiteral("none") ? startArrow : endArrow;
        result.appearanceChanged |= hasSimplifiedStyle(style, true) || hasEmbeddedImage(cell.value, style)
            || (!router.isEmpty() && router != QStringLiteral("none") && !orthogonal)
            || (startArrow == QStringLiteral("none")) == (endArrow == QStringLiteral("none"))
            || arrow != QStringLiteral("block") || style.options.value(QStringLiteral("endFill")) == QStringLiteral("0")
            || style.options.contains(QStringLiteral("entryX")) || style.options.contains(QStringLiteral("entryY"))
            || style.options.contains(QStringLiteral("exitX")) || style.options.contains(QStringLiteral("exitY"));
        if (cell.customGeometry) {
            if (orthogonal || (router == QStringLiteral("none") && style.enabled(QStringLiteral("curved"))
                && style.enabled(QStringLiteral("labelOnCurve"))))
                geometryRequiringPair.insert({from, to});
            else
                result.appearanceChanged = true;
        }
        QVariantMap edge{{QStringLiteral("id"), QStringLiteral("edge-%1").arg(edgeData.size() + 1)},
                         {QStringLiteral("from"), from}, {QStringLiteral("to"), to},
                         {QStringLiteral("text"), plainLabel(cell.value, style.enabled(QStringLiteral("html")), 120, result.labelsShortened)}};
        if (orthogonal)
            edge.insert(QStringLiteral("route"), QStringLiteral("orthogonal"));
        edgeData.append(edge);
    }
    for (const auto &[from, to] : std::as_const(geometryRequiringPair))
        result.appearanceChanged |= !endpoints.contains({to, from});

    result.item = {{QStringLiteral("type"), QStringLiteral("diagram")}, {QStringLiteral("nodes"), nodeData},
                   {QStringLiteral("edges"), edgeData}};
    return result;
}
}

QVariantMap DiagramExport::load(const QUrl &file) const
{
    const auto error = [](const QString &message) { return QVariantMap{{QStringLiteral("error"), message}}; };
    const QFileInfo info(file.toLocalFile());
    const QString extension = info.suffix().toLower();
    if (!file.isLocalFile() || !info.isAbsolute() || !info.isFile()
        || (!file.host().isEmpty() && file.host() != QStringLiteral("localhost"))
        || (extension != QStringLiteral("drawio") && extension != QStringLiteral("xml")))
        return error(tr("Scegli un file draw.io locale (.drawio o .xml)."));
    QFile input(info.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly))
        return error(tr("Impossibile aprire il file draw.io."));
    const QByteArray bytes = input.read(maxFileBytes + 1);
    if (input.error() != QFileDevice::NoError)
        return error(tr("Impossibile leggere il file draw.io."));
    if (bytes.size() > maxFileBytes)
        return error(tr("Il file draw.io supera il limite di 2 MiB."));
    Page page;
    DiagramReader reader(bytes);
    if (!reader.read(page))
        return error(tr("Il file draw.io contiene dati non validi o supera i limiti di importazione."));
    auto converted = convert(page);
    QVariantMap validation = converted.item;
    validation.insert(QStringLiteral("id"), QStringLiteral("import"));
    validation.insert(QStringLiteral("x"), 0.0);
    validation.insert(QStringLiteral("y"), 0.0);
    validation.insert(QStringLiteral("width"), 1.0);
    validation.insert(QStringLiteral("height"), 1.0);
    if (PageContent::validatedItem(validation).isEmpty())
        return error(tr("La prima pagina del file draw.io non contiene forme compatibili."));

    QStringList warnings;
    if (page.extraPages)
        warnings.append(tr("È stata importata la prima pagina del file."));
    if (converted.omitted)
        warnings.append(tr("Alcune forme, gruppi o collegamenti non supportati sono stati omessi."));
    if (converted.labelsShortened)
        warnings.append(tr("Alcune etichette troppo lunghe sono state accorciate."));
    if (converted.appearanceChanged)
        warnings.append(tr("Alcuni dettagli grafici sono stati semplificati."));
    const QString name = page.name.simplified();
    QString title = name.isEmpty() ? info.completeBaseName().trimmed().left(80) : name.left(80);
    if (!title.isEmpty() && title.back().isHighSurrogate())
        title.chop(1);
    return {{QStringLiteral("diagram"), converted.item}, {QStringLiteral("title"), title},
            {QStringLiteral("warnings"), warnings.join(u'\n')}};
}
