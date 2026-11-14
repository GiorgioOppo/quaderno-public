#include "pagecontent.h"

#include "diagramitem.h"
#include "notebook.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QImageReader>
#include <QImageWriter>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace {
constexpr qreal geometryTolerance = 1e-9;
constexpr qsizetype textLimit = 4000;
constexpr qsizetype diagramNodeLimit = 64;
constexpr qsizetype diagramEdgeLimit = 128;
constexpr qsizetype diagramNodeTextLimit = 200;
constexpr qsizetype diagramEdgeTextLimit = 120;
constexpr qsizetype maxImageFileBytes = 16 * 1024 * 1024;
constexpr qsizetype maxStoredImageBytes = 768 * 1024;
constexpr qsizetype maxEncodedImageLength = ((maxStoredImageBytes + 2) / 3) * 4;
constexpr qint64 maxSourceImagePixels = 24 * 1024 * 1024;
constexpr int maxStoredImageEdge = 1200;
constexpr qreal referenceWidth = 600;
constexpr qreal referenceHeight = 800;
constexpr int textSize = 16;
constexpr int cellTextSize = 14;
constexpr qreal textInset = 3;
constexpr qreal cellInset = 4;
constexpr int textFlags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere;

const QColor guideColor(QStringLiteral("#c5cbd3"));
const QColor headingColor(QStringLiteral("#485568"));
const QColor textColor(QStringLiteral("#202833"));
const QColor tableColor(QStringLiteral("#9ea9b6"));
const QString imageFormatKey = QStringLiteral("imageFormat");
const QString imageDataKey = QStringLiteral("imageData");

bool readNumber(const QVariant &value, double &result)
{
    switch (value.typeId()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Float:
    case QMetaType::Double:
        result = value.toDouble();
        return std::isfinite(result);
    default:
        return false;
    }
}

bool readCount(const QVariant &value, int limit, int &result)
{
    double number = 0;
    if (!readNumber(value, number) || number < 1 || number > limit || std::floor(number) != number)
        return false;
    result = int(number);
    return true;
}

bool readText(const QVariant &value, qsizetype limit, QString &result)
{
    if (value.typeId() != QMetaType::QString)
        return false;
    result = value.toString();
    return result.size() <= limit;
}

QByteArray imageFileFormat(QByteArrayView bytes)
{
    if (bytes.startsWith("\x89PNG\r\n\x1a\n"))
        return QByteArrayLiteral("png");
    if (bytes.startsWith("\xff\xd8\xff"))
        return QByteArrayLiteral("jpeg");
    return {};
}

QByteArray storedImageBytes(const QVariantMap &item)
{
    QString encoded;
    if (!readText(item.value(imageDataKey), maxEncodedImageLength, encoded) || encoded.isEmpty())
        return {};
    auto bytes = QByteArray::fromBase64Encoding(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!bytes || bytes.decoded.size() > maxStoredImageBytes)
        return {};
    return std::move(bytes.decoded);
}

QImage storedImage(const QVariantMap &item)
{
    QString storedFormat;
    if (!readText(item.value(imageFormatKey), 4, storedFormat))
        return {};
    const QByteArray bytes = storedImageBytes(item);
    const QByteArray format = imageFileFormat(bytes);
    if (format.isEmpty() || storedFormat != QString::fromLatin1(format))
        return {};
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, format);
    const QSize size = reader.size();
    if (size.isEmpty() || size.width() > maxStoredImageEdge || size.height() > maxStoredImageEdge)
        return {};
    return reader.read();
}

QString imageError(const char *message)
{
    return QCoreApplication::translate("PageContent", message);
}

std::optional<QRectF> readNormalizedRect(const QVariantMap &value)
{
    double x = 0, y = 0, width = 0, height = 0;
    if (!readNumber(value.value(QStringLiteral("x")), x) || !readNumber(value.value(QStringLiteral("y")), y)
        || !readNumber(value.value(QStringLiteral("width")), width) || !readNumber(value.value(QStringLiteral("height")), height)
        || x < 0 || y < 0 || x >= 1 || y >= 1 || width <= 0 || height <= 0
        || width > 1 - x + geometryTolerance || height > 1 - y + geometryTolerance)
        return {};
    return QRectF(x, y, qMin(width, 1 - x), qMin(height, 1 - y));
}

bool readDiagramId(const QVariant &value, QString &id)
{
    return readText(value, 128, id) && !id.isEmpty() && id.trimmed() == id && id.isValidUtf16()
        && !id.contains(QChar::Null);
}

bool readDiagramLabel(const QVariant &value, qsizetype limit, QString &label)
{
    return readText(value, limit, label) && label.isValidUtf16() && !label.contains(QChar::Null);
}

QVariantMap validatedDiagram(const QVariantMap &item, QVariantMap validated)
{
    const auto nodesKey = QStringLiteral("nodes");
    const auto edgesKey = QStringLiteral("edges");
    const auto idKey = QStringLiteral("id");
    const auto shapeKey = QStringLiteral("shape");
    const auto textKey = QStringLiteral("text");
    const auto fromKey = QStringLiteral("from");
    const auto toKey = QStringLiteral("to");
    const auto routeKey = QStringLiteral("route");
    const QVariant nodeData = item.value(nodesKey);
    const QVariant edgeData = item.value(edgesKey);
    if (nodeData.typeId() != QMetaType::QVariantList || edgeData.typeId() != QMetaType::QVariantList)
        return {};
    const QVariantList nodes = nodeData.toList();
    const QVariantList edges = edgeData.toList();
    if (nodes.isEmpty() || nodes.size() > diagramNodeLimit || edges.size() > diagramEdgeLimit)
        return {};

    static const QSet<QString> shapes{QStringLiteral("process"), QStringLiteral("terminal"),
                                      QStringLiteral("decision"), QStringLiteral("data"), QStringLiteral("note")};
    QVariantList cleanNodes;
    cleanNodes.reserve(nodes.size());
    QSet<QString> nodeIds;
    for (const QVariant &value : nodes) {
        const QVariantMap node = value.toMap();
        QString id, shape, text;
        const auto rect = readNormalizedRect(node);
        if (!rect || !readDiagramId(node.value(idKey), id) || nodeIds.contains(id)
            || !readText(node.value(shapeKey), 8, shape) || !shapes.contains(shape)
            || !readDiagramLabel(node.value(textKey), diagramNodeTextLimit, text))
            return {};
        nodeIds.insert(id);
        cleanNodes.append(QVariantMap{{idKey, id}, {shapeKey, shape}, {textKey, text},
                                     {QStringLiteral("x"), rect->x()}, {QStringLiteral("y"), rect->y()},
                                     {QStringLiteral("width"), rect->width()}, {QStringLiteral("height"), rect->height()}});
    }

    QVariantList cleanEdges;
    cleanEdges.reserve(edges.size());
    QSet<QString> edgeIds;
    QSet<QPair<QString, QString>> endpoints;
    for (const QVariant &value : edges) {
        const QVariantMap edge = value.toMap();
        QString id, from, to, text, route;
        if (!readDiagramId(edge.value(idKey), id) || edgeIds.contains(id)
            || !readDiagramId(edge.value(fromKey), from) || !readDiagramId(edge.value(toKey), to)
            || from == to || !nodeIds.contains(from) || !nodeIds.contains(to) || endpoints.contains({from, to})
            || !readDiagramLabel(edge.value(textKey), diagramEdgeTextLimit, text))
            return {};
        if (edge.contains(routeKey) && (!readText(edge.value(routeKey), 10, route)
            || (route != QStringLiteral("direct") && route != QStringLiteral("orthogonal"))))
            return {};
        edgeIds.insert(id);
        endpoints.insert({from, to});
        QVariantMap cleanEdge{{idKey, id}, {fromKey, from}, {toKey, to}, {textKey, text}};
        if (route == QStringLiteral("orthogonal"))
            cleanEdge.insert(routeKey, route);
        cleanEdges.append(cleanEdge);
    }

    validated.insert(nodesKey, cleanNodes);
    validated.insert(edgesKey, cleanEdges);
    return validated;
}

bool readTableNumber(QString text, bool decimalComma, double &number)
{
    if (decimalComma)
        text.replace(u',', u'.');
    QLocale locale = QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    bool ok = false;
    number = locale.toDouble(text, &ok);
    return ok && std::isfinite(number);
}

enum class FormulaValue { Blank, Text, Number, Error };

struct CellResult {
    FormulaValue kind = FormulaValue::Blank;
    double number = 0;
    QString error;
};

CellResult formulaError(QLatin1StringView message)
{
    return {FormulaValue::Error, 0, QString(message)};
}

bool cellReference(QStringView address, QPoint &position)
{
    qsizetype letters = 0;
    while (letters < address.size()) {
        const QChar letter = address.at(letters).toUpper();
        if (letter < u'A' || letter > u'Z')
            break;
        ++letters;
    }
    if (letters == 0 || letters == address.size())
        return false;
    const QStringView digits = address.sliced(letters);
    for (const QChar digit : digits) {
        if (digit < u'0' || digit > u'9')
            return false;
    }

    // Every supported table column has a one-letter address.
    const int x = letters == 1 ? address.at(0).toUpper().unicode() - u'A' : -1;
    const int row = digits.toInt();
    position = QPoint(x, row - 1);
    return true;
}

class TableFormulaResolver
{
public:
    TableFormulaResolver(const QVariantList &cells, int rows, int columns)
        : m_cells(cells), m_rows(rows), m_columns(columns)
    {
    }

    QString displayCell(int index)
    {
        const QString text = m_cells.at(index).toString();
        if (!text.trimmed().startsWith(u'='))
            return text;
        const CellResult value = resolve(index);
        if (value.kind == FormulaValue::Number)
            return QString::number(value.number == 0 ? 0 : value.number, 'g', 15);
        if (value.kind == FormulaValue::Error)
            return value.error;
        return text;
    }

    CellResult resolve(int index)
    {
        if (m_states[index] == State::Done)
            return m_results[index];
        if (m_states[index] == State::Visiting)
            return formulaError(QLatin1String("#CYCLE!"));

        m_states[index] = State::Visiting;
        const QString text = m_cells.at(index).toString().trimmed();
        CellResult result;
        if (text.startsWith(u'=')) {
            result = evaluate(QStringView(text).sliced(1).trimmed());
        } else if (!text.isEmpty()) {
            double number = 0;
            result = readTableNumber(text, true, number) ? CellResult{FormulaValue::Number, number, {}}
                                                         : CellResult{FormulaValue::Text, 0, {}};
        }
        m_results[index] = result;
        m_states[index] = State::Done;
        return result;
    }

private:
    enum class State { Pending, Visiting, Done };

    struct Aggregate {
        double sum = 0;
        int count = 0;

        void add(double number)
        {
            sum += number;
            ++count;
        }
    };

    bool contains(QPoint position) const
    {
        return position.x() >= 0 && position.x() < m_columns && position.y() >= 0 && position.y() < m_rows;
    }

    QString addRange(QStringView argument, qsizetype colon, Aggregate &result)
    {
        QPoint start, end;
        if (!cellReference(argument.first(colon).trimmed(), start) || !contains(start)
            || !cellReference(argument.sliced(colon + 1).trimmed(), end) || !contains(end))
            return QStringLiteral("#REF!");
        for (int row = qMin(start.y(), end.y()); row <= qMax(start.y(), end.y()); ++row) {
            for (int column = qMin(start.x(), end.x()); column <= qMax(start.x(), end.x()); ++column) {
                const CellResult cell = resolve(row * m_columns + column);
                if (cell.kind == FormulaValue::Error)
                    return cell.error;
                if (cell.kind == FormulaValue::Number)
                    result.add(cell.number);
            }
        }
        return {};
    }

    QString addArgument(QStringView argument, bool decimalComma, Aggregate &result)
    {
        const qsizetype colon = argument.indexOf(u':');
        if (colon >= 0)
            return addRange(argument, colon, result);

        QPoint position;
        if (cellReference(argument, position)) {
            if (!contains(position))
                return QStringLiteral("#REF!");
            const CellResult cell = resolve(position.y() * m_columns + position.x());
            if (cell.kind == FormulaValue::Error)
                return cell.error;
            if (cell.kind == FormulaValue::Text)
                return QStringLiteral("#VALUE!");
            if (cell.kind == FormulaValue::Number)
                result.add(cell.number);
            return {};
        }

        double number = 0;
        if (!readTableNumber(argument.toString(), decimalComma, number))
            return QStringLiteral("#VALUE!");
        result.add(number);
        return {};
    }

    CellResult evaluate(QStringView expression)
    {
        const qsizetype open = expression.indexOf(u'(');
        const QString function = (open >= 0 ? expression.first(open).trimmed() : expression).toString().toUpper();
        const bool sum = function == QStringLiteral("SUM") || function == QStringLiteral("SOMMA");
        const bool average = function == QStringLiteral("AVERAGE") || function == QStringLiteral("MEDIA");
        if (!sum && !average) {
            static const QRegularExpression name(QStringLiteral("\\A[A-Z][A-Z0-9_.]*\\z"));
            if (open >= 0 && name.match(function).hasMatch())
                return formulaError(QLatin1String("#NAME?"));
            return {FormulaValue::Text, 0, {}};
        }
        if (open < 0 || !expression.endsWith(u')'))
            return formulaError(QLatin1String("#VALUE!"));

        const QStringView arguments = expression.sliced(open + 1, expression.size() - open - 2).trimmed();
        if (arguments.contains(u'(') || arguments.contains(u')'))
            return formulaError(QLatin1String("#VALUE!"));
        Aggregate result;
        if (!arguments.isEmpty()) {
            const bool semicolons = arguments.contains(u';');
            const auto parts = arguments.split(semicolons ? u';' : u',');
            for (const QStringView part : parts) {
                const QString error = addArgument(part.trimmed(), semicolons, result);
                if (!error.isEmpty())
                    return {FormulaValue::Error, 0, error};
            }
        }
        if (average && result.count == 0)
            return formulaError(QLatin1String("#DIV/0!"));
        const double number = average ? result.sum / result.count : result.sum;
        if (!std::isfinite(number))
            return formulaError(QLatin1String("#VALUE!"));
        return {FormulaValue::Number, number, {}};
    }

    const QVariantList &m_cells;
    const int m_rows;
    const int m_columns;
    std::array<State, PageContent::maxTableRows * PageContent::maxTableColumns> m_states{};
    std::array<CellResult, PageContent::maxTableRows * PageContent::maxTableColumns> m_results{};
};

QVariantList checkedTableCells(const QVariantMap &table, int &rows, int &columns)
{
    if (!readCount(table.value(QStringLiteral("rows")), PageContent::maxTableRows, rows)
        || !readCount(table.value(QStringLiteral("cols")), PageContent::maxTableColumns, columns))
        return {};
    const QVariantList cells = table.value(QStringLiteral("cells")).toList();
    if (cells.size() != rows * columns)
        return {};
    for (const QVariant &cell : cells) {
        QString text;
        if (!readText(cell, PageContent::maxTableCellLength, text))
            return {};
    }
    return cells;
}

QRectF itemRect(const QVariantMap &item, QSizeF size)
{
    return {item.value(QStringLiteral("x")).toDouble() * size.width(),
            item.value(QStringLiteral("y")).toDouble() * size.height(),
            item.value(QStringLiteral("width")).toDouble() * size.width(),
            item.value(QStringLiteral("height")).toDouble() * size.height()};
}

QFont pageFont(QPainter &painter, qreal scale, int points, QFont::Weight weight = QFont::Normal)
{
    QFont font = painter.font();
    font.setPixelSize(qMax(1, qRound(points * scale)));
    font.setWeight(weight);
    return font;
}

void paintPlanner(QPainter &painter, QSizeF size, qreal scale)
{
    const qreal left = size.width() * 0.08;
    const qreal right = size.width() * 0.92;
    const qreal top = size.height() * 0.07;
    const qreal spacing = 26 * scale;
    const auto label = [&](const QString &text, qreal y) {
        const QPen previous = painter.pen();
        painter.setPen(headingColor);
        painter.drawText(QRectF(left, y, right - left, spacing),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
        painter.setPen(previous);
    };
    painter.setFont(pageFont(painter, scale, 15, QFont::DemiBold));
    label(QCoreApplication::translate("PageContent", "Giorno"), top);
    painter.drawLine(QPointF(left + 62 * scale, top + spacing - 4 * scale),
                     QPointF(right, top + spacing - 4 * scale));

    const qreal priorityTop = size.height() * 0.18;
    label(QCoreApplication::translate("PageContent", "Priorità"), priorityTop);
    for (int row = 1; row <= 3; ++row) {
        const qreal y = priorityTop + row * spacing;
        painter.drawRect(QRectF(left, y + 6 * scale, 11 * scale, 11 * scale));
        painter.drawLine(QPointF(left + 20 * scale, y + spacing - 3 * scale),
                         QPointF(right, y + spacing - 3 * scale));
    }

    const qreal notesTop = size.height() * 0.37;
    label(QCoreApplication::translate("PageContent", "Note"), notesTop);
    for (qreal y = notesTop + 2 * spacing; y < size.height() * 0.93; y += spacing)
        painter.drawLine(QPointF(left, y), QPointF(right, y));
}

void drawPageText(QPainter &painter, const QRectF &rect, const QString &text, qreal margin)
{
    const QRectF area = rect.adjusted(margin, margin, -margin, -margin);
    if (area.isEmpty() || text.isEmpty())
        return;
    painter.save();
    painter.setClipRect(area, Qt::IntersectClip);
    painter.setPen(textColor);
    painter.drawText(area, textFlags, text);
    painter.restore();
}
}

namespace PageContent {
bool isTemplate(const QString &key)
{
    return key == QStringLiteral("blank") || key == QStringLiteral("ruled")
        || key == QStringLiteral("grid") || key == QStringLiteral("dotted")
        || key == QStringLiteral("planner");
}

QVariantMap imageFromFile(const QUrl &file, QString &error)
{
    const auto fail = [&](const QString &message) -> QVariantMap {
        error = message;
        return {};
    };
    error.clear();
    const QFileInfo info(file.toLocalFile());
    if (!file.isLocalFile() || (!file.host().isEmpty() && file.host() != QStringLiteral("localhost"))
        || !info.isAbsolute() || !info.isFile())
        return fail(imageError("Scegli un file PNG o JPEG salvato sul dispositivo."));
    QFile source(info.absoluteFilePath());
    if (!source.open(QIODevice::ReadOnly))
        return fail(imageError("Impossibile leggere l’immagine."));
    if (source.size() > maxImageFileBytes)
        return fail(imageError("L’immagine supera il limite di 16 MiB."));
    const QByteArray bytes = source.read(maxImageFileBytes + 1);
    if (source.error() != QFileDevice::NoError)
        return fail(imageError("Impossibile leggere l’immagine."));
    if (bytes.size() > maxImageFileBytes)
        return fail(imageError("L’immagine supera il limite di 16 MiB."));

    const QByteArray inputFormat = imageFileFormat(bytes);
    if (inputFormat.isEmpty())
        return fail(imageError("Scegli un file PNG o JPEG salvato sul dispositivo."));
    QBuffer input;
    input.setData(bytes);
    input.open(QIODevice::ReadOnly);
    QImageReader reader(&input, inputFormat);
    reader.setAutoTransform(true);
    const QSize original = reader.size();
    if (original.isEmpty())
        return fail(imageError("Impossibile leggere l’immagine."));
    if (qint64(original.width()) * original.height() > maxSourceImagePixels)
        return fail(imageError("Le dimensioni dell’immagine sono troppo grandi."));
    if (qMax(original.width(), original.height()) > maxStoredImageEdge)
        reader.setScaledSize(original.scaled(maxStoredImageEdge, maxStoredImageEdge, Qt::KeepAspectRatio).expandedTo(QSize(1, 1)));
    const QImage decoded = reader.read();
    if (decoded.isNull())
        return fail(imageError("Impossibile leggere l’immagine."));

    const bool alpha = decoded.hasAlphaChannel();
    const QByteArray format = alpha ? QByteArrayLiteral("png") : QByteArrayLiteral("jpeg");
    for (int edge = maxStoredImageEdge; edge >= 240; edge = edge * 4 / 5) {
        const QSize target = qMax(decoded.width(), decoded.height()) <= edge ? decoded.size()
            : decoded.size().scaled(edge, edge, Qt::KeepAspectRatio).expandedTo(QSize(1, 1));
        // Copy pixels only so source metadata and file paths do not enter the notebook.
        QImage image(target, alpha ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
        if (image.isNull())
            break;
        image.fill(alpha ? Qt::transparent : Qt::white);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(QRect(QPoint(), target), decoded);
        painter.end();

        QBuffer output;
        output.open(QIODevice::WriteOnly);
        QImageWriter writer(&output, format);
        if (!alpha)
            writer.setQuality(85);
        if (!writer.write(image))
            break;
        if (output.size() <= maxStoredImageBytes)
            return {{QStringLiteral("type"), QStringLiteral("image")}, {imageFormatKey, QString::fromLatin1(format)},
                    {imageDataKey, QString::fromLatin1(output.data().toBase64())}};
    }
    return fail(imageError("Impossibile preparare l’immagine per il quaderno."));
}

qreal preferredItemHeight(const QVariantMap &item, qreal width)
{
    const QString type = item.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("chart"))
        return 0.37;
    if (type == QStringLiteral("diagram"))
        return 0.56;
    if (type == QStringLiteral("image")) {
        const QImage image = storedImage(item);
        return image.isNull() ? 0.38
            : std::clamp(width * referenceWidth / referenceHeight * image.height() / image.width(), 0.06, 0.72);
    }
    const bool table = type == QStringLiteral("table");
    const int rows = table ? std::clamp(item.value(QStringLiteral("rows")).toInt(), 1, PageContent::maxTableRows) : 1;
    const int cols = table ? std::clamp(item.value(QStringLiteral("cols")).toInt(), 1, PageContent::maxTableColumns) : 1;
    const qreal inset = table ? cellInset : textInset;
    const QRectF area(0, 0, qMax(1.0, referenceWidth * width / cols - 2 * inset), referenceHeight);
    QFont font;
    font.setPixelSize(table ? cellTextSize : textSize);
    const QFontMetricsF metrics(font);
    qreal height = table ? 0.058 : 0.055;
    const QVariantList texts = table ? item.value(QStringLiteral("cells")).toList()
                                     : QVariantList{item.value(QStringLiteral("text"))};
    for (const QVariant &value : texts) {
        const QString text = value.toString().left(table ? PageContent::maxTableCellLength : textLimit);
        const qreal needed = metrics.boundingRect(area, textFlags, text).height() + 2 * inset + metrics.leading() + 4;
        height = qMax(height, needed / referenceHeight);
    }
    return qMin(0.82, height * rows);
}

QRectF textRectAtInk(const QString &text, const QRectF &inkBounds)
{
    QFont font;
    font.setPixelSize(textSize);
    const QFontMetricsF metrics(font);
    const qreal horizontalPadding = textInset / referenceWidth;
    const qreal verticalPadding = textInset / referenceHeight;
    constexpr qreal minimumWidth = 0.1;
    qreal x = std::clamp(inkBounds.left() - horizontalPadding, 0.0, 1.0 - minimumWidth);
    qreal desiredWidth = qMax(minimumWidth, inkBounds.width() + 2 * horizontalPadding);
    for (const QString &line : text.split(u'\n')) {
        const qreal lineWidth = (metrics.horizontalAdvance(line) + 2 * textInset + 4) / referenceWidth;
        desiredWidth = qMax(desiredWidth, lineWidth);
    }

    const auto heightAtWidth = [&](qreal width) {
        const QRectF area(0, 0, referenceWidth * width - 2 * textInset, referenceHeight);
        const qreal needed = metrics.boundingRect(area, textFlags, text).height() + 2 * textInset
            + metrics.leading() + 4;
        return qMax(0.055, needed / referenceHeight);
    };
    qreal width = qMin(desiredWidth, 1 - x);
    qreal height = heightAtWidth(width);
    if (height > 1) {
        // A block near the right edge may need more space for wrapped lines.
        width = qMin(desiredWidth, 1.0);
        x = qMin(x, 1 - width);
        height = heightAtWidth(width);
    }
    if (height > 1)
        return {};
    const qreal y = std::clamp(inkBounds.top() - verticalPadding, 0.0, 1 - height);
    return {x, y, width, height};
}

QVariantMap validatedItem(const QVariantMap &item)
{
    const auto idKey = QStringLiteral("id");
    const auto typeKey = QStringLiteral("type");
    const auto xKey = QStringLiteral("x");
    const auto yKey = QStringLiteral("y");
    const auto widthKey = QStringLiteral("width");
    const auto heightKey = QStringLiteral("height");
    const auto textKey = QStringLiteral("text");
    const auto rowsKey = QStringLiteral("rows");
    const auto colsKey = QStringLiteral("cols");
    const auto cellsKey = QStringLiteral("cells");

    QString id;
    QString type;
    if (!readText(item.value(idKey), 128, id) || id.trimmed().isEmpty()
        || !readText(item.value(typeKey), 7, type))
        return {};

    const auto rect = readNormalizedRect(item);
    if (!rect)
        return {};

    QVariantMap validated{{idKey, id}, {typeKey, type}, {xKey, rect->x()}, {yKey, rect->y()},
                          {widthKey, rect->width()}, {heightKey, rect->height()}};
    if (type == QStringLiteral("diagram"))
        return validatedDiagram(item, std::move(validated));
    if (type == QStringLiteral("image")) {
        if (storedImage(item).isNull())
            return {};
        validated.insert(imageFormatKey, item.value(imageFormatKey));
        validated.insert(imageDataKey, item.value(imageDataKey));
        return validated;
    }
    if (type == QStringLiteral("text")) {
        QString text;
        if (!readText(item.value(textKey), textLimit, text))
            return {};
        validated.insert(textKey, text);
        return validated;
    }
    if (type == QStringLiteral("chart")) {
        const auto kindKey = QStringLiteral("chartKind");
        const auto pointsKey = QStringLiteral("points");
        const auto xMaxKey = QStringLiteral("xMax");
        const auto yMaxKey = QStringLiteral("yMax");
        const QString kind = item.value(kindKey).toString();
        const QVariantList points = item.value(pointsKey).toList();
        if ((kind != QStringLiteral("line") && kind != QStringLiteral("bars"))
            || points.size() < (kind == QStringLiteral("bars") ? 1 : 2) || points.size() > 128)
            return {};
        validated.insert(kindKey, kind);
        for (const QString &key : {QStringLiteral("title"), QStringLiteral("xLabel"), QStringLiteral("yLabel")}) {
            QString label;
            if (!readText(item.value(key, QString{}), key == QStringLiteral("title") ? 80 : 40, label))
                return {};
            validated.insert(key, label);
        }
        QVariantList cleaned;
        qreal last = -1;
        for (const QVariant &value : points) {
            const QVariantMap point = value.toMap();
            double px = 0;
            double py = 0;
            if (!readNumber(point.value(xKey), px) || !readNumber(point.value(yKey), py) || px < 0 || px > 1
                || py < 0 || py > 1 || px < last || (kind == QStringLiteral("bars") && px == last))
                return {};
            cleaned.append(QVariantMap{{xKey, px}, {yKey, py}});
            last = px;
        }
        validated.insert(pointsKey, cleaned);
        if (item.contains(xMaxKey) || item.contains(yMaxKey)) {
            double maxX = 0;
            double maxY = 0;
            if (!readNumber(item.value(xMaxKey), maxX) || !readNumber(item.value(yMaxKey), maxY)
                || maxX <= 0 || maxY <= 0 || maxX > 1e12 || maxY > 1e12)
                return {};
            validated.insert(xMaxKey, maxX);
            validated.insert(yMaxKey, maxY);
        }
        if (item.contains(QStringLiteral("minimum"))) {
            const QVariantMap min = item.value(QStringLiteral("minimum")).toMap();
            double xMin = 0;
            double yMin = 0;
            if (!readNumber(min.value(xKey), xMin) || !readNumber(min.value(yKey), yMin)
                || xMin < -1e12 || yMin < -1e12 || xMin > 0 || yMin > 0 || !validated.contains(xMaxKey))
                return {};
            validated.insert(QStringLiteral("minimum"), QVariantMap{{xKey, xMin}, {yKey, yMin}});
        }
        if (item.contains(QStringLiteral("sourceTable"))) {
            QString tableId;
            int xColumn = 0;
            int yColumn = 0;
            const QVariant header = item.value(QStringLiteral("hasHeader"));
            if (!readText(item.value(QStringLiteral("sourceTable")), 128, tableId) || tableId.isEmpty()
                || !readCount(item.value(QStringLiteral("xColumn")), maxTableColumns, xColumn)
                || !readCount(item.value(QStringLiteral("yColumn")), maxTableColumns, yColumn) || xColumn == yColumn
                || header.typeId() != QMetaType::Bool || !validated.contains(QStringLiteral("minimum")))
                return {};
            validated.insert(QStringLiteral("sourceTable"), tableId);
            validated.insert(QStringLiteral("xColumn"), xColumn);
            validated.insert(QStringLiteral("yColumn"), yColumn);
            validated.insert(QStringLiteral("hasHeader"), header);
        }
        return validated;
    }
    if (type != QStringLiteral("table"))
        return {};

    int rows = 0;
    int cols = 0;
    if (!readCount(item.value(rowsKey), maxTableRows, rows) || !readCount(item.value(colsKey), maxTableColumns, cols)
        || item.value(cellsKey).typeId() != QMetaType::QVariantList)
        return {};
    const QVariantList cells = item.value(cellsKey).toList();
    if (cells.size() != rows * cols)
        return {};
    for (const QVariant &value : cells) {
        QString text;
        if (!readText(value, maxTableCellLength, text))
            return {};
    }
    validated.insert(rowsKey, rows);
    validated.insert(colsKey, cols);
    validated.insert(cellsKey, cells);
    return validated;
}

QVariantList resolvedTableCells(const QVariantMap &table)
{
    int rows = 0;
    int columns = 0;
    const QVariantList cells = checkedTableCells(table, rows, columns);
    if (cells.isEmpty())
        return {};
    TableFormulaResolver resolver(cells, rows, columns);
    QVariantList display;
    display.reserve(cells.size());
    for (int index = 0; index < cells.size(); ++index)
        display.append(resolver.displayCell(index));
    return display;
}

QVariantMap chartFromTable(const QVariantMap &table, const QVariantMap &options)
{
    int rows = 0;
    int columns = 0;
    const int xColumn = options.value(QStringLiteral("xColumn")).toInt() - 1;
    const int yColumn = options.value(QStringLiteral("yColumn")).toInt() - 1;
    const bool header = options.value(QStringLiteral("hasHeader")).toBool();
    const QString kind = options.value(QStringLiteral("chartKind")).toString();
    const QVariantList cells = checkedTableCells(table, rows, columns);
    if (cells.isEmpty() || columns < 2 || xColumn < 0 || xColumn >= columns
        || yColumn < 0 || yColumn >= columns || xColumn == yColumn || (kind != "line" && kind != "bars"))
        return {};

    TableFormulaResolver resolver(cells, rows, columns);
    QList<QPointF> series;
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    for (int row = header ? 1 : 0; row < rows; ++row) {
        const CellResult xCell = resolver.resolve(row * columns + xColumn);
        const CellResult yCell = resolver.resolve(row * columns + yColumn);
        if (xCell.kind == FormulaValue::Blank && yCell.kind == FormulaValue::Blank)
            continue;
        if (xCell.kind != FormulaValue::Number || yCell.kind != FormulaValue::Number
            || qAbs(xCell.number) > 1e12 || qAbs(yCell.number) > 1e12)
            return {};
        const double x = xCell.number;
        const double y = yCell.number;
        series.append(QPointF(x, y));
        minX = qMin(minX, x);
        maxX = qMax(maxX, x);
        minY = qMin(minY, y);
        maxY = qMax(maxY, y);
    }
    if (series.size() < (kind == "line" ? 2 : 1))
        return {};
    std::stable_sort(series.begin(), series.end(), [](QPointF a, QPointF b) { return a.x() < b.x(); });
    if (maxX == 0)
        maxX = 1;
    if (maxY == 0)
        maxY = 1;

    QVariantList points;
    for (qsizetype i = 0; i < series.size(); ++i) {
        const QPointF point = series.at(i);
        if (kind == "bars" && i > 0 && point.x() == series.at(i - 1).x())
            return {};
        points.append(QVariantMap{{QStringLiteral("x"), (point.x() - minX) / (maxX - minX)},
                                  {QStringLiteral("y"), (point.y() - minY) / (maxY - minY)}});
    }
    QVariantMap chart = options;
    chart.insert(QStringLiteral("sourceTable"), table.value(QStringLiteral("id")));
    chart.insert(QStringLiteral("points"), points);
    chart.insert(QStringLiteral("xMax"), maxX);
    chart.insert(QStringLiteral("yMax"), maxY);
    chart.insert(QStringLiteral("minimum"), QVariantMap{{QStringLiteral("x"), minX}, {QStringLiteral("y"), minY}});
    chart.insert(QStringLiteral("xLabel"), header ? resolver.displayCell(xColumn).left(40) : QString{});
    chart.insert(QStringLiteral("yLabel"), header ? resolver.displayCell(yColumn).left(40) : QString{});
    return chart;
}

QVariantList resolveTableCharts(const QVariantList &items)
{
    QHash<QString, QVariantMap> tables;
    for (const QVariant &entry : items) {
        const QVariantMap item = entry.toMap();
        if (item.value(QStringLiteral("type")) == QStringLiteral("table"))
            tables.insert(item.value(QStringLiteral("id")).toString(), item);
    }
    QVariantList resolved;
    resolved.reserve(items.size());
    for (const QVariant &entry : items) {
        QVariantMap chart = entry.toMap();
        const QString source = chart.value(QStringLiteral("sourceTable")).toString();
        if (!source.isEmpty()) {
            const auto updated = chartFromTable(tables.value(source), chart);
            if (updated.isEmpty()) {
                chart.insert(QStringLiteral("points"), QVariantList{});
                chart.insert(QStringLiteral("invalidTableData"), true);
            } else {
                chart = updated;
            }
        }
        resolved.append(chart);
    }
    return resolved;
}

void paintTemplate(QPainter &painter, QSizeF pageSize, const QString &key)
{
    if (!isTemplate(key) || key == QStringLiteral("blank") || pageSize.isEmpty())
        return;

    const qreal scale = pageSize.width() / 600;
    const qreal left = pageSize.width() * 0.08;
    const qreal right = pageSize.width() * 0.92;
    const qreal top = pageSize.height() * 0.07;
    const qreal bottom = pageSize.height() * 0.93;
    painter.save();
    painter.setClipRect(QRectF(QPointF(), pageSize), Qt::IntersectClip);
    painter.setPen(QPen(guideColor, 0.7 * scale));
    painter.setBrush(Qt::NoBrush);
    if (key == QStringLiteral("planner")) {
        paintPlanner(painter, pageSize, scale);
    } else if (key == QStringLiteral("dotted")) {
        const qreal spacing = 19 * scale;
        painter.setPen(Qt::NoPen);
        painter.setBrush(guideColor);
        painter.setRenderHint(QPainter::Antialiasing);
        for (qreal y = top; y <= bottom; y += spacing) {
            for (qreal x = left; x <= right; x += spacing)
                painter.drawEllipse(QPointF(x, y), 0.8 * scale, 0.8 * scale);
        }
    } else {
        const bool grid = key == QStringLiteral("grid");
        const qreal spacing = (grid ? 19 : 26) * scale;
        for (qreal y = top; y <= bottom; y += spacing)
            painter.drawLine(QPointF(left, y), QPointF(right, y));
        if (grid) {
            for (qreal x = left; x <= right; x += spacing)
                painter.drawLine(QPointF(x, top), QPointF(x, bottom));
        }
    }
    painter.restore();
}

void paintItems(QPainter &painter, QSizeF pageSize, const QVariantList &items)
{
    if (pageSize.isEmpty() || items.isEmpty())
        return;

    const qreal scale = pageSize.width() / 600;
    painter.save();
    painter.setClipRect(QRectF(QPointF(), pageSize), Qt::IntersectClip);
    for (const QVariant &entry : items) {
        const QVariantMap item = entry.toMap();
        const QRectF rect = itemRect(item, pageSize);
        const QString type = item.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("diagram")) {
            drawDiagram(painter, rect, item);
            continue;
        }
        if (type == QStringLiteral("chart")) {
            painter.save();
            painter.translate(rect.topLeft());
            paintChart(painter, rect.size(), item);
            painter.restore();
            continue;
        }
        if (type == QStringLiteral("text")) {
            painter.setFont(pageFont(painter, scale, textSize));
            drawPageText(painter, rect, item.value(QStringLiteral("text")).toString(), textInset * scale);
            continue;
        }
        if (type == QStringLiteral("image")) {
            const QImage image = storedImage(item);
            if (!image.isNull()) {
                const QSizeF fitted = QSizeF(image.size()).scaled(rect.size(), Qt::KeepAspectRatio);
                QRectF target(QPointF(), fitted);
                target.moveCenter(rect.center());
                painter.save();
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.drawImage(target, image);
                painter.restore();
            }
            continue;
        }
        if (type != QStringLiteral("table"))
            continue;

        const int rows = item.value(QStringLiteral("rows")).toInt();
        const int cols = item.value(QStringLiteral("cols")).toInt();
        const QVariantList cells = resolvedTableCells(item);
        if (cells.isEmpty())
            continue;

        const qreal cellWidth = rect.width() / cols;
        const qreal cellHeight = rect.height() / rows;
        painter.setPen(QPen(tableColor, 0.9 * scale));
        painter.setBrush(Qt::white);
        painter.drawRect(rect);
        for (int row = 1; row < rows; ++row) {
            const qreal y = rect.top() + row * cellHeight;
            painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
        for (int column = 1; column < cols; ++column) {
            const qreal x = rect.left() + column * cellWidth;
            painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }

        painter.setFont(pageFont(painter, scale, cellTextSize));
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < cols; ++column) {
                const QRectF cell(rect.left() + column * cellWidth, rect.top() + row * cellHeight,
                                  cellWidth, cellHeight);
                drawPageText(painter, cell, cells.at(row * cols + column).toString(), cellInset * scale);
            }
        }
    }
    painter.restore();
}

void paintChart(QPainter &painter, QSizeF size, const QVariantMap &chart)
{
    if (size.isEmpty())
        return;
    if (chart.value(QStringLiteral("invalidTableData")).toBool()) {
        const QRectF frame(QPointF(), size);
        painter.fillRect(frame, Qt::white);
        painter.save();
        painter.setPen(headingColor);
        painter.setFont(pageFont(painter, size.width() / 420, 14));
        painter.drawText(frame.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap,
                         QCoreApplication::translate("PageContent", "Controlla i dati della tabella collegata."));
        painter.restore();
        return;
    }
    const QVariantList points = chart.value(QStringLiteral("points")).toList();
    const QString kind = chart.value(QStringLiteral("chartKind")).toString();
    if (points.isEmpty() || (kind != QStringLiteral("line") && kind != QStringLiteral("bars")))
        return;

    const QString title = chart.value(QStringLiteral("title")).toString();
    const QString xLabel = chart.value(QStringLiteral("xLabel")).toString();
    const QString yLabel = chart.value(QStringLiteral("yLabel")).toString();
    const double maxX = chart.value(QStringLiteral("xMax")).toDouble();
    const double maxY = chart.value(QStringLiteral("yMax")).toDouble();
    const QVariantMap minimum = chart.value(QStringLiteral("minimum")).toMap();
    const double minX = minimum.value(QStringLiteral("x")).toDouble();
    const double minY = minimum.value(QStringLiteral("y")).toDouble();
    const bool calibrated = maxX > 0 && maxY > 0;
    const qreal scale = qMin(size.width() / 420, size.height() / 220);
    const QRectF frame(QPointF(), size);
    const QRectF plot = frame.adjusted((calibrated ? 58 : 29) * scale,
                                       (title.isEmpty() ? 18 : 39) * scale,
                                       -25 * scale, -(calibrated ? 35 : 20) * scale - (xLabel.isEmpty() ? 0 : 16 * scale));
    if (plot.isEmpty())
        return;

    painter.save();
    painter.setClipRect(frame, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(frame, Qt::white);
    painter.setPen(QPen(guideColor, scale));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(frame.adjusted(scale / 2, scale / 2, -scale / 2, -scale / 2));
    for (int tick = 1; tick <= 4; ++tick) {
        const qreal y = plot.bottom() - plot.height() * tick / 4;
        const qreal x = plot.left() + plot.width() * tick / 4;
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    painter.setPen(QPen(headingColor, qMax(1.0, 1.3 * scale)));
    painter.drawLine(plot.topLeft(), plot.bottomLeft());
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());

    const auto position = [&plot](const QVariant &value) {
        const QVariantMap point = value.toMap();
        return QPointF(plot.left() + plot.width() * point.value(QStringLiteral("x")).toDouble(),
                       plot.bottom() - plot.height() * point.value(QStringLiteral("y")).toDouble());
    };
    const QColor series(QStringLiteral("#155cc2"));
    if (kind == QStringLiteral("line")) {
        QPainterPath path(position(points.constFirst()));
        for (qsizetype i = 1; i < points.size(); ++i)
            path.lineTo(position(points.at(i)));
        painter.setPen(QPen(series, qMax(1.0, 2.5 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    } else {
        painter.save();
        painter.setClipRect(plot, Qt::IntersectClip);
        qreal spacing = plot.width() / qMax<qsizetype>(4, points.size());
        for (qsizetype i = 1; i < points.size(); ++i)
            spacing = qMin(spacing, position(points.at(i)).x() - position(points.at(i - 1)).x());
        const qreal barWidth = qMax(scale, spacing * 0.64);
        const qreal baseline = plot.bottom() - plot.height() * (0 - minY) / (maxY > 0 ? maxY - minY : 1);
        for (const QVariant &point : points) {
            const QPointF tip = position(point);
            painter.fillRect(QRectF(tip.x() - barWidth / 2, qMin(tip.y(), baseline), barWidth,
                                    qAbs(baseline - tip.y())), series);
        }
        painter.restore();
    }

    painter.setPen(textColor);
    painter.setFont(pageFont(painter, scale, 16, QFont::DemiBold));
    painter.drawText(QRectF(8 * scale, 5 * scale, size.width() - 16 * scale, 27 * scale), Qt::AlignCenter,
                     QFontMetricsF(painter.font()).elidedText(title, Qt::ElideRight, size.width() - 16 * scale));
    painter.setFont(pageFont(painter, scale, 11));
    if (calibrated) {
        for (int tick = 0; tick <= 4; ++tick) {
            const qreal x = plot.left() + plot.width() * tick / 4;
            const qreal y = plot.bottom() - plot.height() * tick / 4;
            painter.drawText(QRectF(x - 21 * scale, plot.bottom() + 3 * scale, 42 * scale, 14 * scale), Qt::AlignCenter,
                             QString::number(minX + (maxX - minX) * tick / 4, 'g', 3));
            if (tick > 0 || minY != 0) {
                painter.drawText(QRectF(17 * scale, y - 7 * scale, plot.left() - 21 * scale, 14 * scale),
                                 Qt::AlignRight | Qt::AlignVCenter, QString::number(minY + (maxY - minY) * tick / 4, 'g', 3));
            }
        }
    }
    const QFontMetricsF metrics(painter.font());
    painter.drawText(QRectF(plot.left(), size.height() - 19 * scale, plot.width(), 16 * scale), Qt::AlignCenter,
                     metrics.elidedText(xLabel, Qt::ElideRight, plot.width()));
    if (!yLabel.isEmpty()) {
        painter.translate(9 * scale, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-plot.height() / 2, -7 * scale, plot.height(), 14 * scale), Qt::AlignCenter,
                         metrics.elidedText(yLabel, Qt::ElideRight, plot.height()));
    }
    painter.restore();
}
}

bool Notebook::importPageImage(const QUrl &file, const QString &bookId, const QString &pageId)
{
    if (bookId.isEmpty() || pageId.isEmpty() || this->bookId() != bookId || this->pageId() != pageId)
        return fail(tr("La pagina selezionata è cambiata. Riprova."));
    QString error;
    const QVariantMap item = PageContent::imageFromFile(file, error);
    return item.isEmpty() ? fail(error) : savePageItem(item);
}
