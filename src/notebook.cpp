#include "notebook.h"
#include "notebookformat.h"

#include "chartrecognizer.h"
#include "epubimport.h"
#include "diagramitem.h"
#include "diagramexport.h"
#include "diagramrecognizer.h"
#include "inkrender.h"
#include "inkselection.h"
#include "pagecontent.h"
#include "pdfimport.h"
#include "pagestore.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QLineF>
#include <QMap>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPolygonF>
#include <QRegularExpression>
#include <QRectF>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStringConverter>
#include <QSysInfo>
#include <QTextBoundaryFinder>
#include <QTextLayout>
#include <QTransform>
#include <QTemporaryDir>
#include <QUrl>
#include <QtMath>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr int LegacyFileVersion = 1;
constexpr int LayoutFileVersion = 2;
constexpr int LibraryFileVersion = 3;
constexpr int RecognitionFileVersion = 4;
constexpr int ReadingFileVersion = 5;
constexpr int DiagramFileVersion = 6;
constexpr int OrthogonalDiagramFileVersion = 7;
constexpr int ImageFileVersion = 8;
constexpr int ExtendedFileVersion = 9;
constexpr int DiagramExtensionFileVersion = 10;
constexpr int PageNoteFileVersion = 11;
constexpr qsizetype maxPagePdfText = 20000;
constexpr qsizetype maxTranscripts = 32;
constexpr qsizetype maxImportBytes = 64 * 1024 * 1024;
constexpr qsizetype maxItemsPerPage = 120;
constexpr qsizetype maxTagsPerBook = 20;
constexpr qsizetype maxTagLength = 40;
constexpr qsizetype maxInkHistory = 100;
constexpr qsizetype maxLayers = 12;
const QString defaultLayerId = QStringLiteral("ink");
constexpr qsizetype maxEncodedBackground = 3 * 1024 * 1024;
constexpr qsizetype maxMarkdownPages = 200;
constexpr qsizetype markdownTextBlockLimit = 4000;
constexpr qreal markdownWidth = 0.88;
constexpr qreal markdownHeight = 0.88;

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool isMarkdownExtension(const QString &extension)
{
    return extension.compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0
        || extension.compare(QStringLiteral("markdown"), Qt::CaseInsensitive) == 0;
}

bool safeMarkdownDestination(const QUrl &file, const QFileInfo &output, const QString &libraryPath)
{
    return file.isLocalFile() && isMarkdownExtension(output.suffix()) && output.absoluteFilePath() != libraryPath
        && (!output.exists() || output.canonicalFilePath() != QFileInfo(libraryPath).canonicalFilePath());
}

QString documentFileName(QString name, const QString &suffix)
{
    name = name.left(80).trimmed();
    if (!name.isEmpty() && name.back().isHighSurrogate())
        name.chop(1);
    name.replace(QRegularExpression(QStringLiteral("[/\\\\:\\p{Cc}]")), QStringLiteral("_"));
    if (name.isEmpty() || name.startsWith(QLatin1Char('.')))
        name.prepend(QStringLiteral("Quaderno"));
    return name + suffix;
}

QList<QString> paginateMarkdown(const QString &text)
{
    // Fit the same reference page and text scale used by PageContent and the
    // editor. Use a margin for different platform fonts and line metrics.
    QFont font;
    font.setPixelSize(16);
    const QFontMetricsF metrics(font);
    const QRectF area(0, 0, 600 * markdownWidth - 12, 800);
    const auto fits = [&](qsizetype start, qsizetype length) {
        return metrics.boundingRect(area, Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere,
                                    text.mid(start, length)).height() <= 800 * markdownHeight - 46;
    };
    QTextBoundaryFinder graphemes(QTextBoundaryFinder::Grapheme, text);
    QList<QString> pages;
    qsizetype offset = 0;
    do {
        if (pages.size() >= maxMarkdownPages)
            return {};
        qsizetype count = qMin(markdownTextBlockLimit, text.size() - offset);
        if (!fits(offset, count)) {
            qsizetype low = 1;
            qsizetype high = count - 1;
            while (low < high) {
                const qsizetype midpoint = low + (high - low + 1) / 2;
                if (fits(offset, midpoint))
                    low = midpoint;
                else
                    high = midpoint - 1;
            }
            count = low;
        }
        if (offset + count < text.size()) {
            const QStringView window = QStringView(text).sliced(offset, count);
            const qsizetype newline = window.lastIndexOf(u'\n');
            // Prefer a source-line boundary, except when an early newline
            // would waste most of the page before a very long line.
            if (newline >= count / 2)
                count = newline + 1;
            graphemes.setPosition(offset + count);
            if (!graphemes.isAtBoundary()) {
                const qsizetype boundary = graphemes.toPreviousBoundary();
                if (boundary > offset)
                    count = boundary - offset;
                else if (text.at(offset + count - 1).isHighSurrogate())
                    --count;
            }
        }
        if (count <= 0 && offset < text.size())
            return {};
        pages.append(text.mid(offset, count));
        offset += count;
    } while (offset < text.size());
    return pages;
}

QString legacyPageId(const QString &bookId, qsizetype index)
{
    static const QUuid pageNamespace(QStringLiteral("2ed61d8f-aeeb-4b49-bd01-f1fb9decb1f1"));
    // A delimiter and decimal index disambiguate the legacy book ID.
    return QUuid::createUuidV5(pageNamespace, bookId.toUtf8() + ':' + QByteArray::number(index))
        .toString(QUuid::WithoutBraces);
}

bool validUuid(const QString &id)
{
    const QUuid value = QUuid::fromString(id);
    return !value.isNull() && value.toString(QUuid::WithoutBraces) == id;
}

bool validBackground(const QByteArray &data)
{
    if (data.isEmpty())
        return false;
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, QByteArrayLiteral("jpeg"));
    const QSize size = reader.size();
    // Validate dimensions without decoding every page during library startup.
    // The requested page is decoded on the reader worker and reports errors.
    return reader.canRead() && !size.isEmpty() && size.width() <= 1600 && size.height() <= 1600;
}

bool normalizedTags(const QVariantList &input, QStringList &tags)
{
    if (input.size() > maxTagsPerBook)
        return false;
    QSet<QString> seen;
    for (const QVariant &entry : input) {
        const QString tag = entry.toString().simplified();
        if (entry.typeId() != QMetaType::QString || tag.isEmpty() || tag.size() > maxTagLength)
            return false;
        const QString key = tag.toCaseFolded();
        if (!seen.contains(key)) {
            seen.insert(key);
            tags.append(tag);
        }
    }
    return true;
}

bool readCoordinate(const QVariant &value, double &number)
{
    bool ok = false;
    number = value.toDouble(&ok);
    return ok && std::isfinite(number) && number >= 0.0 && number <= 1.0;
}

QVariantMap validatedStroke(const QVariantMap &stroke, bool allowOriginal = true)
{
    const QString color = stroke.value(QStringLiteral("color")).toString();
    bool widthOk = false;
    const double width = stroke.value(QStringLiteral("width")).toDouble(&widthOk);
    const QVariantList points = stroke.value(QStringLiteral("points")).toList();
    if (color.trimmed().isEmpty() || !widthOk || !std::isfinite(width) || width <= 0.0
        || points.isEmpty()) {
        return {};
    }

    QVariantList validatedPoints;
    validatedPoints.reserve(points.size());
    for (const QVariant &pointValue : points) {
        const QVariantMap point = pointValue.toMap();
        double x, y, pressure;
        if (!readCoordinate(point.value(QStringLiteral("x")), x)
            || !readCoordinate(point.value(QStringLiteral("y")), y)
            || !readCoordinate(point.value(QStringLiteral("p")), pressure)) {
            return {};
        }
        validatedPoints.append(QVariantMap{
            {QStringLiteral("x"), x},
            {QStringLiteral("y"), y},
            {QStringLiteral("p"), pressure},
        });
    }

    QVariantMap result{
        {QStringLiteral("color"), color},
        {QStringLiteral("width"), width},
        {QStringLiteral("points"), validatedPoints},
    };
    bool shape = false;
    if (stroke.contains(QStringLiteral("tool"))) {
        const QString tool = stroke.value(QStringLiteral("tool")).toString();
        shape = isInkShape(inkToolFromName(tool));
        if (shape && points.size() != 2)
            return {};
        if (shape || tool == QStringLiteral("pencil") || tool == QStringLiteral("marker") || tool == QStringLiteral("highlighter"))
            result.insert(QStringLiteral("tool"), tool);
        else if (tool != QStringLiteral("pen"))
            return {};
    }
    if (stroke.contains(QStringLiteral("original"))) {
        if (!allowOriginal || !shape)
            return {};
        const QVariantMap original = validatedStroke(stroke.value(QStringLiteral("original")).toMap(), false);
        if (original.isEmpty() || isInkShape(inkToolFromName(original.value(QStringLiteral("tool")).toString())))
            return {};
        result.insert(QStringLiteral("original"), original);
    }
    return result;
}

QString matchExcerpt(const QString &text, const QString &query)
{
    const QString compact = text.simplified();
    const qsizetype found = compact.indexOf(query, 0, Qt::CaseInsensitive);
    if (found < 0)
        return {};
    const qsizetype start = qMax<qsizetype>(0, found - 26);
    const qsizetype end = qMin<qsizetype>(compact.size(), start + 100);
    return (start > 0 ? QString(QChar(0x2026)) : QString{}) + compact.mid(start, end - start)
        + (end < compact.size() ? QString(QChar(0x2026)) : QString{});
}

QString tableLibraryTitle(const QVariantMap &item)
{
    const QVariantList cells = PageContent::resolvedTableCells(item);
    for (const QVariant &cell : cells) {
        QString title = cell.toString().simplified();
        if (!title.isEmpty()) {
            title.truncate(80);
            if (title.back().isHighSurrogate())
                title.chop(1);
            return title;
        }
    }
    return {};
}

QString libraryItemTitle(const QVariantMap &item, const QString &bookTitle)
{
    const QVariant type = item.value(QStringLiteral("type"));
    if (type == QStringLiteral("chart"))
        return item.value(QStringLiteral("title")).toString();
    if (type == QStringLiteral("table"))
        return tableLibraryTitle(item);
    QString title;
    for (const QVariant &node : item.value(QStringLiteral("nodes")).toList()) {
        title = node.toMap().value(QStringLiteral("text")).toString().simplified();
        if (!title.isEmpty()) break;
    }
    if (title.isEmpty()) title = bookTitle;
    title.truncate(80);
    if (!title.isEmpty() && title.back().isHighSurrogate()) title.chop(1);
    return title;
}

bool isLibraryItem(const QVariant &type)
{
    return type == QStringLiteral("chart") || type == QStringLiteral("table") || type == QStringLiteral("diagram");
}

QString objectWorkspace(const QVariant &type)
{
    if (type == QStringLiteral("chart")) return QStringLiteral("charts");
    if (type == QStringLiteral("table")) return QStringLiteral("tables");
    if (type == QStringLiteral("diagram")) return QStringLiteral("diagrams");
    return {};
}

qsizetype imageCount(const QVariantList &items)
{
    return std::count_if(items.cbegin(), items.cend(), [](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("type")) == QStringLiteral("image");
    });
}

bool readStrokes(const QJsonArray &data, QVariantList &strokes)
{
    strokes.reserve(data.size());
    for (const QJsonValue &value : data) {
        const QVariantMap stroke = validatedStroke(value.toObject().toVariantMap());
        if (stroke.isEmpty())
            return false;
        strokes.append(stroke);
    }
    return true;
}

bool hasOriginalInk(const QVariantList &strokes)
{
    return std::any_of(strokes.cbegin(), strokes.cend(), [](const QVariant &stroke) {
        return stroke.toMap().contains(QStringLiteral("original"));
    });
}

QString readFile(const QString &path, QByteArray &contents)
{
    QFile file(path);
    if (!file.exists())
        return {};

    if (file.open(QIODevice::ReadOnly)) {
        contents = file.readAll();
        if (file.error() == QFileDevice::NoError) {
            if (contents.isNull())
                contents = QByteArrayLiteral("");
            return {};
        }
    }

    return QCoreApplication::translate("Notebook", "Impossibile leggere il file dei quaderni. %1").arg(file.errorString());
}

QByteArray fileRevision(const QByteArray &contents)
{
    return contents.isNull() ? QByteArray{} : QCryptographicHash::hash(contents, QCryptographicHash::Sha256);
}

QRectF itemRect(const QVariantMap &item)
{
    return QRectF(item.value(QStringLiteral("x")).toDouble(), item.value(QStringLiteral("y")).toDouble(),
                  item.value(QStringLiteral("width")).toDouble(), item.value(QStringLiteral("height")).toDouble());
}

int diagramShapesVersion(const QVariantMap &diagram)
{
    int version = 0;
    for (const auto &node : diagram.value(QStringLiteral("nodes")).toList()) {
        const auto shape = node.toMap().value(QStringLiteral("shape")).toString();
        if (diagramShapeRequiresUmlFormat(shape)) return NotebookFormat::umlDiagramShapesVersion;
        if (diagramShapeRequiresNewFormat(shape)) version = NotebookFormat::extendedDiagramShapesVersion;
    }
    return version;
}

int requiredItemVersion(const QVariantMap &item)
{
    const QVariant type = item.value(QStringLiteral("type"));
    if (type == QStringLiteral("link"))
        return NotebookFormat::pageLinksVersion;
    if (type == QStringLiteral("note"))
        return PageNoteFileVersion;
    const auto recovery = item.value(QStringLiteral("handwritingSource")).toMap();
    if (type == QStringLiteral("diagram")) {
        const auto version = qMax(diagramShapesVersion(item),
            diagramShapesVersion(recovery.value(QStringLiteral("baseDiagram")).toMap()));
        if (version) return version;
    }
    if (item.contains(QStringLiteral("styleScale")) || recovery.contains(QStringLiteral("baseDiagram")))
        return DiagramExtensionFileVersion;
    if (item.contains(QStringLiteral("handwritingSource")) || item.contains(QStringLiteral("rotation"))
        || item.contains(QStringLiteral("textScale")))
        return ExtendedFileVersion;
    if (type == QStringLiteral("image"))
        return ImageFileVersion;
    if (type != QStringLiteral("diagram"))
        return 0;
    const auto edges = item.value(QStringLiteral("edges")).toList();
    const bool orthogonal = std::any_of(edges.cbegin(), edges.cend(), [](const QVariant &edge) {
        return edge.toMap().value(QStringLiteral("route")) == QStringLiteral("orthogonal");
    });
    return orthogonal ? OrthogonalDiagramFileVersion : DiagramFileVersion;
}

QRectF placement(const QVariantList &items, const QVariantMap &item)
{
    const QString type = item.value(QStringLiteral("type")).toString();
    const double width = type == QStringLiteral("note") ? 0.05 : type == QStringLiteral("text") ? 0.76 : 0.84;
    const double height = PageContent::preferredItemHeight(item, width);
    const QRectF candidate(0.08, 0.10, width, height);
    for (double y = candidate.y(); y + height <= 0.95; y += 0.04) {
        const QRectF position(candidate.x(), y, width, height);
        const bool occupied = std::any_of(items.cbegin(), items.cend(), [&](const QVariant &entry) {
            return position.intersects(itemRect(entry.toMap()).adjusted(-0.015, -0.015, 0.015, 0.015));
        });
        if (!occupied)
            return position;
    }
    return candidate;
}

QVariantMap itemAt(QVariantMap item, const QRectF &rect)
{
    item.insert(QStringLiteral("x"), rect.x());
    item.insert(QStringLiteral("y"), rect.y());
    item.insert(QStringLiteral("width"), rect.width());
    item.insert(QStringLiteral("height"), rect.height());
    return PageContent::validatedItem(item);
}

QVariantMap newPlacedItem(QVariantMap item, const QVariantList &items)
{
    const QRectF rect = placement(items, item);
    item.insert(QStringLiteral("id"), newId());
    return itemAt(std::move(item), rect);
}

QVariantMap pageLinkItem(const QVariantMap &target, const QPointF &anchor)
{
    constexpr qreal width = .055;
    constexpr qreal height = .04125;
    const QRectF bounds(std::clamp(anchor.x() - width / 2, 0.0, 1.0 - width),
                        std::clamp(anchor.y() - height / 2, 0.0, 1.0 - height), width, height);
    return itemAt({
        {QStringLiteral("id"), newId()},
        {QStringLiteral("type"), QStringLiteral("link")},
        {QStringLiteral("targetBookId"), target.value(QStringLiteral("bookId"))},
        {QStringLiteral("targetPageId"), target.value(QStringLiteral("pageId"))},
        {QStringLiteral("targetItemId"), target.value(QStringLiteral("itemId"))},
        {QStringLiteral("targetType"), target.value(QStringLiteral("type"))},
    }, bounds);
}
}

Notebook::Notebook(QString filePath, QObject *parent)
    : QObject(parent), m_filePath(QFileInfo(filePath).absoluteFilePath())
{
    m_loadError = load();
    loadReadingPreferences();
    setError(m_loadError.isEmpty() && m_recoveredStorage
        ? tr("Recuperata l’ultima versione valida della libreria. Il salvataggio danneggiato verrà conservato.")
        : m_loadError);
}

Notebook::~Notebook()
{
    if (m_importCancellation)
        m_importCancellation->store(true);
}

namespace {
bool validReadingPreferences(const QVariantMap &values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.key() == QStringLiteral("swipeAction")) {
            if (!QStringList{QStringLiteral("pages"), QStringLiteral("scroll"), QStringLiteral("none")}.contains(it.value().toString())) return false;
            continue;
        }
        if (it.value().metaType().id() == QMetaType::Bool) return false;
        bool valid = false;
        const double number = it.value().toDouble(&valid);
        if (!valid || !std::isfinite(number) || number != std::floor(number)) return false;
        const auto allowed = [&](std::initializer_list<int> choices) {
            return std::find(choices.begin(), choices.end(), number) != choices.end();
        };
        if (it.key() == QStringLiteral("readingZoom")) { if (number < 100 || number > 300) return false; }
        else if (it.key() == QStringLiteral("readingMargins")) { if (!allowed({0, 8, 16, 24})) return false; }
        else if (it.key() == QStringLiteral("readingOrientation")) { if (!allowed({-1, 0, 90, 180, 270})) return false; }
        else return false;
    }
    return true;
}
}

QString Notebook::sourceCacheDirectory() const { return QFileInfo(m_filePath).absolutePath() + QStringLiteral("/document-sources"); }
QString Notebook::readingPreferencesPath() const { return m_filePath + QStringLiteral(".reader-settings.json"); }
QString Notebook::validateBackupData(const QByteArray &data) { State state; return readState(data, state); }
QVariantMap Notebook::localReadingPreferences() const { return readingPreferencesForBook(bookId()); }
QVariantMap Notebook::readingPreferencesForBook(const QString &id) const { return m_readingPreferences.value(id).toMap(); }

void Notebook::loadReadingPreferences()
{
    QFile file(readingPreferencesPath());
    if (!file.exists()) return;
    const auto invalid = tr("Le preferenze di lettura locali non sono leggibili. I quaderni restano disponibili.");
    if (QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        m_readingPreferencesLoadError = invalid;
        return;
    }
    m_readingPreferencesBytes = file.read(1024 * 1024 + 1);
    const auto root = QJsonDocument::fromJson(m_readingPreferencesBytes).object();
    if (file.error() != QFileDevice::NoError || m_readingPreferencesBytes.size() > 1024 * 1024
        || root.value(QStringLiteral("format")) != QStringLiteral("quaderno-reader-settings-v1")
        || !root.value(QStringLiteral("books")).isObject()) {
        m_readingPreferencesLoadError = invalid;
        return;
    }
    const auto books = root.value(QStringLiteral("books")).toObject();
    for (auto it = books.begin(); it != books.end(); ++it) {
        if (!it.value().isObject() || !validReadingPreferences(it.value().toObject().toVariantMap())) {
            m_readingPreferencesLoadError = invalid;
            return;
        }
    }
    m_readingPreferences = books.toVariantMap();
}

bool Notebook::saveReadingPreferences(const QVariantMap &values)
{
    if (!m_readingPreferencesLoadError.isEmpty()) return fail(m_readingPreferencesLoadError);
    const QString path = readingPreferencesPath();
    QLockFile lock(path + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) return fail(tr("Le preferenze di lettura sono in uso. Riprova."));
    QFile previous(path);
    QByteArray bytes;
    if (previous.exists()) {
        if (QFileInfo(previous).isSymLink() || !previous.open(QIODevice::ReadOnly) || previous.size() > 1024 * 1024)
            return fail(tr("Impossibile leggere le preferenze di lettura."));
        bytes = previous.read(1024 * 1024 + 1);
        if (previous.error() != QFileDevice::NoError || bytes.size() > 1024 * 1024)
            return fail(tr("Impossibile leggere le preferenze di lettura."));
    }
    if (bytes != m_readingPreferencesBytes) return fail(tr("Le preferenze di lettura sono cambiate. Riapri Quaderno."));
    const QByteArray next = QJsonDocument(QJsonObject{{QStringLiteral("format"), QStringLiteral("quaderno-reader-settings-v1")},
        {QStringLiteral("books"), QJsonObject::fromVariantMap(values)}}).toJson(QJsonDocument::Compact);
    if (next.size() > 1024 * 1024) return fail(tr("Le preferenze di lettura superano il limite locale."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(next) != next.size() || !file.commit()) return fail(tr("Impossibile salvare le preferenze di lettura."));
    m_readingPreferencesBytes = next;
    m_readingPreferences = values;
    setError({});
    emit localReadingPreferencesChanged();
    return true;
}

bool Notebook::setReadingPreferences(const QVariantMap &changes)
{
    if (!currentBook() || !validReadingPreferences(changes)) return fail(tr("Le preferenze di lettura non sono valide."));
    QVariantMap next = m_readingPreferences, book = localReadingPreferences();
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) book.insert(it.key(), it.value());
    if (book == localReadingPreferences()) return true;
    next.insert(bookId(), book);
    return saveReadingPreferences(next);
}

bool Notebook::clearReadingPreferences()
{
    if (!currentBook()) return false;
    if (!m_readingPreferences.contains(bookId())) return true;
    auto next = m_readingPreferences;
    next.remove(bookId());
    return saveReadingPreferences(next);
}

QVariantMap Notebook::sourceForPage(const Book &book, const Page &page) const
{
    if (page.source.isEmpty())
        return {};
    QVariantMap source = page.source;
    const auto stored = book.sourceDocuments.value(source.value(QStringLiteral("asset")).toString()).toMap();
    source.insert(QStringLiteral("data"), stored.value(QStringLiteral("data")));
    source.insert(QStringLiteral("cacheDirectory"), sourceCacheDirectory());
    return source;
}

QVariantMap Notebook::pageRenderContent() const
{
    const Book *book = currentBook();
    const Page *page = currentPage();
    if (!book || !page)
        return {};
    return {{QStringLiteral("background"), page->backgroundData()},
            {QStringLiteral("source"), sourceForPage(*book, *page)}};
}

QVariantList Notebook::books() const
{
    QVariantList result;
    result.reserve(m_state.books.size());
    for (const Book &book : m_state.books) {
        QStringList pageTags;
        QSet<QString> keys;
        bool document = false;
        bool markdown = false;
        bool hasCharts = false;
        bool hasTables = false;
        bool hasDiagrams = false;
        QSet<QString> documentFormats;
        for (const Page &page : book.pages) {
            document |= page.hasBackground();
            markdown |= !page.markdownBlockId.isEmpty();
            if (!page.source.isEmpty()) {
                const QString sourceId = page.source.value(QStringLiteral("origin"),
                    page.source.value(QStringLiteral("asset"))).toString();
                const QString kind = book.sourceDocuments.value(sourceId).toMap().value(QStringLiteral("kind")).toString();
                if (kind == QStringLiteral("pdf") || kind == QStringLiteral("epub"))
                    documentFormats.insert(kind);
            } else if (page.hasBackground()) {
                documentFormats.insert(QStringLiteral("legacy"));
            }
            if (!hasCharts || !hasTables || !hasDiagrams) {
                for (const QVariant &entry : page.items) {
                    const QVariant type = entry.toMap().value(QStringLiteral("type"));
                    hasCharts |= type == QStringLiteral("chart");
                    hasTables |= type == QStringLiteral("table");
                    hasDiagrams |= type == QStringLiteral("diagram");
                    if (hasCharts && hasTables && hasDiagrams)
                        break;
                }
            }
            for (const QString &tag : page.tags) {
                const QString key = tag.toCaseFolded();
                if (!keys.contains(key)) {
                    keys.insert(key);
                    pageTags.append(tag);
                }
            }
        }
        result.append(QVariantMap{
            {QStringLiteral("id"), book.id},
            {QStringLiteral("title"), book.title},
            {QStringLiteral("pageCount"), static_cast<int>(book.pages.size())},
            {QStringLiteral("folderId"), book.folderId},
            {QStringLiteral("favorite"), book.favorite},
            {QStringLiteral("trashed"), book.trashed},
            {QStringLiteral("tags"), book.tags},
            {QStringLiteral("pageTags"), pageTags},
            {QStringLiteral("document"), document},
            {QStringLiteral("format"), documentFormats.size() == 1 ? *documentFormats.cbegin()
                : documentFormats.size() > 1 ? QStringLiteral("mixed")
                : markdown ? QStringLiteral("markdown") : QStringLiteral("notebook")},
            {QStringLiteral("markdown"), markdown},
            {QStringLiteral("hasCharts"), hasCharts},
            {QStringLiteral("hasTables"), hasTables},
            {QStringLiteral("hasDiagrams"), hasDiagrams},
            {QStringLiteral("workspace"), book.workspace},
        });
    }
    return result;
}

QVariantList Notebook::folders() const
{
    QVariantList result;
    result.reserve(m_state.folders.size());
    for (const Folder &folder : m_state.folders) {
        result.append(QVariantMap{{QStringLiteral("id"), folder.id},
                                  {QStringLiteral("title"), folder.title},
                                  {QStringLiteral("parentId"), folder.parentId}});
    }
    return result;
}

QVariantList Notebook::libraryItems() const
{
    QVariantList result;
    for (const Book &book : m_state.books) {
        if (book.trashed)
            continue;
        bool hasDiagram = false;
        for (int index = 0; index < book.pages.size(); ++index) {
            const Page &page = book.pages.at(index);
            for (const QVariant &entry : page.items) {
                const QVariantMap item = entry.toMap();
                const QVariant type = item.value(QStringLiteral("type"));
                if (!isLibraryItem(type))
                    continue;
                hasDiagram |= type == QStringLiteral("diagram");
                result.append(QVariantMap{
                    {QStringLiteral("bookId"), book.id},
                    {QStringLiteral("bookTitle"), book.title},
                    {QStringLiteral("pageId"), page.id},
                    {QStringLiteral("pageNumber"), index + 1},
                    {QStringLiteral("itemId"), item.value(QStringLiteral("id"))},
                    {QStringLiteral("type"), type},
                    {QStringLiteral("title"), libraryItemTitle(item, book.title)},
                    {QStringLiteral("empty"), false},
                    {QStringLiteral("folderId"), book.folderId},
                    {QStringLiteral("favorite"), book.favorite},
                    {QStringLiteral("tags"), book.tags},
                    {QStringLiteral("pageTags"), page.tags},
                });
            }
        }
        if (!hasDiagram && book.workspace == QStringLiteral("diagrams") && !book.pages.isEmpty()) {
            const int index = resumePage(book);
            const Page &page = book.pages.at(index);
            result.append(QVariantMap{{QStringLiteral("bookId"), book.id}, {QStringLiteral("bookTitle"), book.title},
                {QStringLiteral("pageId"), page.id}, {QStringLiteral("pageNumber"), index + 1},
                {QStringLiteral("itemId"), QString{}}, {QStringLiteral("type"), QStringLiteral("diagram")},
                {QStringLiteral("empty"), true}, {QStringLiteral("title"), book.title},
                {QStringLiteral("folderId"), book.folderId}, {QStringLiteral("favorite"), book.favorite},
                {QStringLiteral("tags"), book.tags}, {QStringLiteral("pageTags"), page.tags}});
        }
    }
    return result;
}

QString Notebook::bookId() const
{
    const Book *book = currentBook();
    return book ? book->id : QString{};
}

QString Notebook::title() const
{
    const Book *book = currentBook();
    return book ? book->title : QString{};
}

int Notebook::page() const
{
    return m_state.page;
}

QString Notebook::pageId() const
{
    const Page *page = currentPage();
    return page ? page->id : QString{};
}

int Notebook::pageCount() const
{
    const Book *book = currentBook();
    return book ? static_cast<int>(book->pages.size()) : 0;
}

QVariantList Notebook::strokes() const
{
    const Page *page = currentPage();
    return page ? visibleStrokes(*page) : QVariantList{};
}

bool Notebook::canUndo() const
{
    const Page *page = currentPage();
    return page && canWriteLayer() && (!m_undo.isEmpty() || !activeLayerInk(*page).isEmpty());
}

bool Notebook::canRedo() const
{
    return canWriteLayer() && !m_redo.isEmpty();
}

QVariantList Notebook::layers() const
{
    const Page *page = currentPage();
    if (!page)
        return {};
    if (page->layers.isEmpty()) {
        return {QVariantMap{{QStringLiteral("id"), defaultLayerId}, {QStringLiteral("name"), tr("Inchiostro")},
                            {QStringLiteral("visible"), true}}};
    }
    QVariantList result;
    for (const InkLayer &layer : page->layers) {
        result.append(QVariantMap{{QStringLiteral("id"), layer.id}, {QStringLiteral("name"), layer.name},
                                  {QStringLiteral("visible"), layer.visible}});
    }
    return result;
}

QString Notebook::activeLayer() const
{
    const Page *page = currentPage();
    return page ? (page->layers.isEmpty() ? defaultLayerId : page->activeLayer) : QString{};
}

bool Notebook::canWriteLayer() const
{
    const Page *page = currentPage();
    return page && (page->layers.isEmpty() || page->layers.at(findLayer(*page, page->activeLayer)).visible);
}

int Notebook::erasableFrom() const
{
    const Page *page = currentPage();
    int offset = 0;
    if (page) {
        for (const InkLayer &layer : page->layers) {
            if (layer.id == page->activeLayer)
                break;
            if (layer.visible)
                offset += static_cast<int>(layer.strokes.size());
        }
    }
    return offset;
}

int Notebook::erasableCount() const
{
    return canWriteLayer() ? static_cast<int>(activeLayerInk(*currentPage()).size()) : 0;
}

QString Notebook::pageTemplate() const
{
    const Page *page = currentPage();
    return page ? page->paper : QStringLiteral("blank");
}

QVariantList Notebook::pageItems() const
{
    const Page *page = currentPage();
    return page ? PageContent::resolveTableCharts(page->items) : QVariantList{};
}

QStringList Notebook::pageTags() const
{
    const Page *page = currentPage();
    return page ? page->tags : QStringList{};
}

QStringList Notebook::knownTags() const
{
    QMap<QString, QString> tags;
    for (const QVariant &value : books()) {
        const QVariantMap book = value.toMap();
        if (book.value(QStringLiteral("trashed")).toBool())
            continue;
        const QStringList labels = book.value(QStringLiteral("tags")).toStringList()
            + book.value(QStringLiteral("pageTags")).toStringList();
        for (const QString &tag : labels)
            tags.insert(tag.toCaseFolded(), tag);
    }
    return tags.values();
}

QByteArray Notebook::pageBackground() const
{
    const Page *page = currentPage();
    return page ? page->backgroundData() : QByteArray{};
}

bool Notebook::hasPageBackground() const
{
    const Page *page = currentPage();
    return page && page->hasBackground();
}

bool Notebook::diagramBook() const
{
    const Book *book = currentBook();
    return book && book->workspace == QStringLiteral("diagrams");
}

QString Notebook::workspaceForPage(const Page &page, const Book &book)
{
    if (page.hasBackground())
        return QStringLiteral("books");
    if (!page.markdownBlockId.isEmpty())
        return QStringLiteral("writing");

    QString workspace;
    for (const QVariant &entry : page.items) {
        const QVariant type = entry.toMap().value(QStringLiteral("type"));
        if (type == QStringLiteral("text"))
            return QStringLiteral("writing");
        const QString candidate = objectWorkspace(type);
        if (candidate.isEmpty())
            continue;
        if (!workspace.isEmpty() && candidate != workspace)
            return QStringLiteral("writing");
        workspace = candidate;
    }
    if (!workspace.isEmpty())
        return workspace;
    return book.workspace == QStringLiteral("diagrams") ? book.workspace : QStringLiteral("writing");
}

QString Notebook::pageWorkspace() const
{
    const Page *page = currentPage();
    return page ? workspaceForPage(*page, *currentBook()) : QString{};
}

bool Notebook::markdownBook() const
{
    const Book *book = currentBook();
    return book && hasMarkdownSource(*book);
}

bool Notebook::pageBookmarked() const
{
    const Page *page = currentPage();
    return page && page->bookmarked;
}

QString Notebook::error() const
{
    return m_error;
}

QString Notebook::pdfFileName() const
{
    return documentFileName(title(), QStringLiteral(".pdf"));
}

QUrl Notebook::suggestedPdfName() const
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!QFileInfo(directory).isDir())
        directory = QDir::homePath();
    return QUrl::fromLocalFile(QDir(directory).filePath(pdfFileName()));
}

bool Notebook::exportDocument(const QUrl &file)
{
    if (!file.isLocalFile())
        return fail(tr("Scegli una cartella sul dispositivo per salvare il PDF."));
    if (isMarkdownExtension(QFileInfo(file.toLocalFile()).suffix()))
        return exportMarkdown(file);
    return exportPdf(file.toLocalFile());
}

QVariantList Notebook::shareOptions() const
{
    const Book *book = currentBook();
    const Page *page = currentPage();
    if (!book || !page || book->trashed || m_importing)
        return {};

    QVariantList options;
    const auto add = [&](const QString &format, const QString &extension, const QString &title,
                         const QString &itemId = {}, const QVariantMap &target = {}) {
        QVariantMap option = target;
        option.insert(QStringLiteral("format"), format);
        option.insert(QStringLiteral("extension"), extension);
        option.insert(QStringLiteral("bookId"), book->id);
        option.insert(QStringLiteral("pageId"), page->id);
        option.insert(QStringLiteral("itemId"), itemId);
        option.insert(QStringLiteral("name"), documentFileName(title, u'.' + extension));
        option.insert(QStringLiteral("title"), title);
        options.append(option);
    };

    const QString pageTitle = tr("%1 - pagina %2").arg(book->title.left(60)).arg(m_state.page + 1);
    add(QStringLiteral("pdf"), QStringLiteral("pdf"), book->title);
    add(QStringLiteral("pdf-page"), QStringLiteral("pdf"), pageTitle);
    add(QStringLiteral("markdown"), QStringLiteral("md"), book->title);
    add(QStringLiteral("markdown-assets"), QStringLiteral("md"), book->title);
    add(QStringLiteral("png"), QStringLiteral("png"), pageTitle);
    add(QStringLiteral("quaderno"), QStringLiteral("quaderno"), book->title);

    const QString sourceId = page->source.value(QStringLiteral("origin"),
        page->source.value(QStringLiteral("asset"))).toString();
    const auto source = book->sourceDocuments.value(sourceId).toMap();
    const QString sourceKind = source.value(QStringLiteral("kind")).toString();
    if (sourceKind == QStringLiteral("pdf") || sourceKind == QStringLiteral("epub")) {
        add(QStringLiteral("source"), sourceKind, tr("%1 - originale").arg(book->title.left(66)), {},
            {{QStringLiteral("sourceId"), sourceId}});
    }

    QSet<QPair<QString, QPair<QString, QString>>> seen;
    const auto addItem = [&](const QVariantMap &item, const QString &itemId, const QString &targetBook,
                             const QString &targetPage, const QString &targetTitle) {
        const QString type = item.value(QStringLiteral("type")).toString();
        if (!isLibraryItem(type) || itemId.isEmpty()) return;
        const QString targetItem = item.value(QStringLiteral("id")).toString();
        const auto identity = qMakePair(targetBook, qMakePair(targetPage, targetItem));
        if (seen.contains(identity)) return;
        seen.insert(identity);
        const QString format = type == QStringLiteral("diagram") ? QStringLiteral("drawio") : QStringLiteral("csv");
        const QString name = libraryItemTitle(item, targetTitle);
        add(format, format, name.isEmpty() ? targetTitle : name, itemId,
            {{QStringLiteral("type"), type}, {QStringLiteral("targetBookId"), targetBook},
             {QStringLiteral("targetPageId"), targetPage}, {QStringLiteral("targetItemId"), targetItem}});
    };
    for (const auto &entry : page->items) {
        const auto item = entry.toMap();
        addItem(item, item.value(QStringLiteral("id")).toString(), book->id, page->id, book->title);
    }
    for (const auto &entry : page->items) {
        const auto item = entry.toMap();
        if (item.value(QStringLiteral("type")) != QStringLiteral("link")) continue;
        const QString id = item.value(QStringLiteral("id")).toString();
        const auto target = pageLinkTarget(item);
        if (!target) continue;
        const auto &targetBook = m_state.books.at(target->book);
        const auto &targetPage = targetBook.pages.at(target->page);
        addItem(targetPage.items.at(target->item).toMap(), id, targetBook.id, targetPage.id, targetBook.title);
    }
    return options;
}

QUrl Notebook::suggestedShareFile(const QVariantMap &option) const
{
    const QString name = option.value(QStringLiteral("name")).toString();
    if (name.isEmpty() || name.startsWith(u'.') || name.contains(u'\\') || QFileInfo(name).fileName() != name)
        return {};
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!QFileInfo(directory).isDir())
        directory = QDir::homePath();
    return QUrl::fromLocalFile(QDir(directory).filePath(name));
}

bool Notebook::exportShareFile(const QVariantMap &option, const QUrl &file)
{
    const Book *book = currentBook();
    const Page *page = currentPage();
    if (!book || !page || book->trashed || m_importing
        || option.value(QStringLiteral("bookId")) != book->id || option.value(QStringLiteral("pageId")) != page->id)
        return fail(tr("La pagina selezionata è cambiata. Riapri il menu di condivisione."));

    const auto keys = {QStringLiteral("format"), QStringLiteral("itemId"), QStringLiteral("sourceId"),
        QStringLiteral("targetBookId"), QStringLiteral("targetPageId"), QStringLiteral("targetItemId")};
    const auto choices = shareOptions();
    const auto match = std::find_if(choices.cbegin(), choices.cend(), [&](const QVariant &entry) {
        const auto candidate = entry.toMap();
        return std::all_of(keys.begin(), keys.end(), [&](const QString &key) {
            return candidate.value(key) == option.value(key);
        });
    });
    if (match == choices.cend())
        return fail(tr("L’elemento selezionato non è più disponibile per la condivisione."));

    const auto choice = match->toMap();
    const QString format = choice.value(QStringLiteral("format")).toString();
    const QString extension = choice.value(QStringLiteral("extension")).toString();
    const QFileInfo output(file.toLocalFile());
    const bool suffixMatches = output.suffix().compare(extension, Qt::CaseInsensitive) == 0
        || (extension == QStringLiteral("md") && isMarkdownExtension(output.suffix()));
    if (!file.isLocalFile() || (!file.host().isEmpty() && file.host() != QStringLiteral("localhost"))
        || !output.isAbsolute() || output.isSymLink() || output.isDir() || !suffixMatches
        || output.absoluteFilePath() == m_filePath
        || (output.exists() && output.canonicalFilePath() == QFileInfo(m_filePath).canonicalFilePath()))
        return fail(tr("Scegli un file locale con estensione .%1.").arg(extension));

    if (format == QStringLiteral("pdf") || format == QStringLiteral("pdf-page"))
        return exportPdf(output.absoluteFilePath(), format == QStringLiteral("pdf-page"));
    if (format == QStringLiteral("markdown")) return exportMarkdown(file);
    if (format == QStringLiteral("markdown-assets")) return exportMarkdownWithAssets(file);
    if (format == QStringLiteral("csv") || format == QStringLiteral("drawio")) {
        const QString targetBookId = choice.value(QStringLiteral("targetBookId")).toString();
        const QString targetPageId = choice.value(QStringLiteral("targetPageId")).toString();
        const QString targetItemId = choice.value(QStringLiteral("targetItemId")).toString();
        const auto &targetBook = m_state.books.at(findBook(m_state, targetBookId));
        const auto &targetPage = targetBook.pages.at(findPage(targetBook, targetPageId));
        const auto item = PageContent::resolveTableCharts(targetPage.items).at(findPageItem(targetPage, targetItemId)).toMap();
        if (format == QStringLiteral("csv")) {
            return item.value(QStringLiteral("type")) == QStringLiteral("table")
                ? exportTableCsv(file, item) : exportChartCsv(file, ChartRecognizer().csv(item));
        }
        const QString exportError = DiagramExport().save(file, item, choice.value(QStringLiteral("title")).toString());
        if (!exportError.isEmpty()) return fail(exportError);
        setError({});
        return true;
    }

    QSaveFile destination(output.absoluteFilePath());
    if (!destination.open(QIODevice::WriteOnly))
        return fail(tr("Impossibile salvare il file. %1").arg(destination.errorString()));
    if (format == QStringLiteral("png")) {
        QImage image(QSize(1200, 1600), QImage::Format_RGB32);
        QPainter painter(&image);
        if (!painter.isActive()) return fail(tr("Impossibile preparare l’immagine della pagina."));
        if (!paintExportPage(painter, image.size(), *book, *page)) return false;
        painter.end();
        QImageWriter writer(&destination, "png");
        if (!writer.write(image))
            return fail(tr("Impossibile salvare il file. %1").arg(writer.errorString()));
    } else {
        QByteArray bytes;
        if (format == QStringLiteral("quaderno")) {
            bytes = exportBookForTransfer(book->id);
            if (bytes.isEmpty()) return false;
        } else {
            const QString sourceId = choice.value(QStringLiteral("sourceId")).toString();
            const auto source = book->sourceDocuments.value(sourceId).toMap();
            bytes = QByteArray::fromBase64(source.value(QStringLiteral("data")).toString().toLatin1(),
                QByteArray::AbortOnBase64DecodingErrors);
            if (bytes.isEmpty() || QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() != sourceId.toLatin1())
                return fail(tr("La sorgente della pagina è mancante o danneggiata."));
        }
        if (destination.write(bytes) != bytes.size())
            return fail(tr("Impossibile salvare il file. %1").arg(destination.errorString()));
    }
    if (!destination.commit())
        return fail(tr("Impossibile salvare il file. %1").arg(destination.errorString()));
    setError({});
    return true;
}

QUrl Notebook::suggestedMarkdownName() const
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!QFileInfo(directory).isDir())
        directory = QDir::homePath();
    return QUrl::fromLocalFile(QDir(directory).filePath(documentFileName(title(), QStringLiteral(".md"))));
}

QString Notebook::markdownText() const
{
    const Book *book = currentBook();
    return book ? markdownTextOf(*book) : QString{};
}

QString Notebook::markdownSource() const
{
    const Book *book = currentBook();
    return book ? markdownSourceOf(*book) : QString{};
}

QString Notebook::markdownTextForBook(const QString &id) const
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return {};
    return markdownTextOf(m_state.books.at(index));
}

bool Notebook::hasBookId(const QString &id) const
{
    return findBook(m_state, id) >= 0;
}

bool Notebook::canRefreshMarkdownBook(const QString &id) const
{
    const int index = findBook(m_state, id);
    return index >= 0 && !m_state.books.at(index).trashed && isPureMarkdown(m_state.books.at(index));
}

bool Notebook::isPureMarkdown(const Book &book)
{
    return std::all_of(book.pages.cbegin(), book.pages.cend(), [](const Page &page) {
        return !page.markdownBlockId.isEmpty() && !hasMarkdownPageAnnotations(page);
    });
}

bool Notebook::hasMarkdownPageAnnotations(const Page &page)
{
    return page.items.size() != 1 || !page.strokes.isEmpty() || !page.layers.isEmpty() || page.hasBackground()
        || !page.pdfText.isEmpty() || !page.tags.isEmpty() || !page.transcripts.isEmpty() || page.bookmarked
        || page.paper != QStringLiteral("blank");
}

bool Notebook::hasMarkdownSource(const Book &book)
{
    return std::any_of(book.pages.cbegin(), book.pages.cend(), [](const Page &page) {
        return !page.markdownBlockId.isEmpty();
    });
}

QString Notebook::markdownSourceOf(const Book &book)
{
    QString text = QStringLiteral("");
    for (const Page &page : book.pages) {
        if (page.markdownBlockId.isEmpty())
            continue;
        const int index = findPageItem(page, page.markdownBlockId);
        if (index >= 0)
            text += page.items.at(index).toMap().value(QStringLiteral("text")).toString();
    }
    return text;
}

QString Notebook::markdownTextOf(const Book &book)
{
    QString text = QStringLiteral("");
    bool previousWasSource = false;
    int pageNumber = 0;
    for (const Page &page : book.pages) {
        ++pageNumber;
        int noteNumber = 0;
        for (const QVariant &entry : page.items) {
            const QVariantMap item = entry.toMap();
            const QString type = item.value(QStringLiteral("type")).toString();
            if (type != QStringLiteral("text") && type != QStringLiteral("note"))
                continue;
            QString fragment = item.value(QStringLiteral("text")).toString();
            if (type == QStringLiteral("note")) {
                ++noteNumber;
                if (!fragment.isEmpty())
                    fragment = QStringLiteral("### %1\n\n%2").arg(tr("Nota %1 · Pagina %2").arg(noteNumber).arg(pageNumber), fragment);
            }
            const bool source = item.value(QStringLiteral("id")).toString() == page.markdownBlockId;
            if (fragment.isEmpty() && !source)
                continue;
            if (!text.isEmpty() && !fragment.isEmpty() && (!previousWasSource || !source)) {
                if (!text.endsWith(u'\n'))
                    text.append(u'\n');
                if (!text.endsWith(QStringLiteral("\n\n")) && !fragment.startsWith(u'\n'))
                    text.append(u'\n');
            }
            text.append(fragment);
            previousWasSource = source;
        }
    }
    return text;
}

bool Notebook::exportMarkdown(const QUrl &file)
{
    if (!currentBook())
        return fail(tr("Apri un quaderno da esportare."));
    const QFileInfo output(file.toLocalFile());
    if (!safeMarkdownDestination(file, output, m_filePath))
        return fail(tr("Scegli un file .md o .markdown per salvare il testo."));

    const QByteArray bytes = markdownText().toUtf8();
    QSaveFile destination(output.absoluteFilePath());
    if (!destination.open(QIODevice::WriteOnly) || destination.write(bytes) != bytes.size() || !destination.commit())
        return fail(tr("Impossibile salvare il Markdown. %1").arg(destination.errorString()));
    setError({});
    return true;
}

bool Notebook::exportMarkdownWithAssets(const QUrl &file)
{
    const Book *book = currentBook();
    if (!book)
        return fail(tr("Apri un quaderno da esportare."));
    const QFileInfo output(file.toLocalFile());
    if (!safeMarkdownDestination(file, output, m_filePath))
        return fail(tr("Scegli un file .md o .markdown per salvare il testo."));

    const auto assetError = [this](const QString &detail) {
        return fail(tr("Impossibile salvare gli allegati del Markdown. %1").arg(detail));
    };
    std::unique_ptr<QTemporaryDir> assets;
    const auto addAsset = [&](const QString &name, auto write) {
        if (!assets) {
            assets = std::make_unique<QTemporaryDir>(output.dir().filePath(QStringLiteral("quaderno-assets-XXXXXX")));
            if (!assets->isValid())
                return assetError(assets->errorString());
        }
        QSaveFile target(assets->filePath(name));
        if (!target.open(QIODevice::WriteOnly) || !write(target) || !target.commit())
            return assetError(target.errorString());
        return true;
    };
    const auto imageLink = [&](const QString &name, const QString &label) {
        return QStringLiteral("![%1](%2/%3)\n").arg(label, QFileInfo(assets->path()).fileName(), name);
    };

    QString markdown = markdownTextOf(*book);
    for (int index = 0; index < book->pages.size(); ++index) {
        const Page &page = book->pages.at(index);
        const auto pagePrefix = QStringLiteral("page-%1-").arg(index + 1);
        QString links;
        int itemNumber = 0;
        int noteNumber = 0;
        for (const QVariant &entry : PageContent::resolveTableCharts(page.items)) {
            const QVariantMap item = entry.toMap();
            const QVariant type = item.value(QStringLiteral("type"));
            if (type == QStringLiteral("image")) {
                const bool png = item.value(QStringLiteral("imageFormat")) == QStringLiteral("png");
                const QString name = pagePrefix + QStringLiteral("image-%1.%2").arg(++itemNumber)
                    .arg(png ? QStringLiteral("png") : QStringLiteral("jpg"));
                const QByteArray bytes = QByteArray::fromBase64(item.value(QStringLiteral("imageData")).toString().toLatin1());
                if (!addAsset(name, [&](QIODevice &device) { return !bytes.isEmpty() && device.write(bytes) == bytes.size(); }))
                    return false;
                links += imageLink(name, tr("Immagine"));
            } else if (type == QStringLiteral("chart")) {
                const QString name = pagePrefix + QStringLiteral("chart-%1.png").arg(++itemNumber);
                if (!addAsset(name, [&](QIODevice &device) {
                        QImage image(QSize(840, 480), QImage::Format_ARGB32_Premultiplied);
                        if (image.isNull())
                            return false;
                        image.fill(Qt::white);
                        QPainter painter(&image);
                        PageContent::paintChart(painter, image.size(), item);
                        return painter.end() && QImageWriter(&device, "png").write(image);
                    }))
                    return false;
                links += imageLink(name, tr("Grafico"));
            } else if (type == QStringLiteral("table")) {
                const QString name = pagePrefix + QStringLiteral("table-%1.csv").arg(++itemNumber);
                const auto bytes = tableCsvData(item);
                if (!addAsset(name, [&](QIODevice &device) { return !bytes.isEmpty() && device.write(bytes) == bytes.size(); }))
                    return false;
                links += QStringLiteral("[%1](%2/%3)\n").arg(tr("Tabella (.csv)"), QFileInfo(assets->path()).fileName(), name);
            } else if (type == QStringLiteral("note")) {
                ++noteNumber;
                links += QStringLiteral("%1\n").arg(tr("Nota %1 · Pagina %2").arg(noteNumber).arg(index + 1));
                const QVariantList noteInk = item.value(QStringLiteral("noteInk")).toList();
                if (!noteInk.isEmpty()) {
                    const QString name = pagePrefix + QStringLiteral("note-%1.png").arg(noteNumber);
                    if (!addAsset(name, [&](QIODevice &device) {
                            QImage image(QSize(900, 600), QImage::Format_ARGB32_Premultiplied);
                            if (image.isNull()) return false;
                            image.fill(Qt::white);
                            QPainter painter(&image);
                            paintStoredStrokes(painter, image.size(), noteInk);
                            return painter.end() && QImageWriter(&device, "png").write(image);
                        })) return false;
                    links += imageLink(name, tr("Scrittura della nota"));
                }
            } else if (type == QStringLiteral("diagram")) {
                const QString base = pagePrefix + QStringLiteral("diagram-%1").arg(++itemNumber);
                const QString imageName = base + QStringLiteral(".png");
                if (!addAsset(imageName, [&](QIODevice &device) {
                        const qreal aspect = item.value(QStringLiteral("height")).toDouble() * 800
                            / (item.value(QStringLiteral("width")).toDouble() * 600);
                        const QSize size = QSizeF(1000, 1000 * aspect).scaled(QSizeF(1200, 1200), Qt::KeepAspectRatio).toSize();
                        QImage image(size.expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
                        if (image.isNull()) return false;
                        image.fill(Qt::white);
                        QPainter painter(&image);
                        drawDiagram(painter, QRectF(QPointF{}, image.size()), item);
                        return painter.end() && QImageWriter(&device, "png").write(image);
                    })) return false;
                const QString diagramName = base + QStringLiteral(".drawio");
                QString exportError;
                if (!addAsset(diagramName, [&](QIODevice &device) {
                        exportError = DiagramExport::write(device, item, book->title);
                        return exportError.isEmpty();
                    })) {
                    if (!exportError.isEmpty()) return assetError(exportError);
                    return false;
                }
                links += imageLink(imageName, tr("Diagramma"));
                links += QStringLiteral("[%1](%2/%3)\n").arg(tr("Modifica il diagramma (.drawio)"),
                    QFileInfo(assets->path()).fileName(), diagramName);
            }
        }
        const QVariantList strokes = visibleStrokes(page);
        if (!strokes.isEmpty()) {
            const QString name = pagePrefix + QStringLiteral("ink.png");
            if (!addAsset(name, [&](QIODevice &device) {
                    QImage image(QSize(900, 1200), QImage::Format_ARGB32_Premultiplied);
                    if (image.isNull())
                        return false;
                    image.fill(Qt::transparent);
                    QPainter painter(&image);
                    paintStoredStrokes(painter, image.size(), strokes);
                    return painter.end() && QImageWriter(&device, "png").write(image);
                }))
                return false;
            links += imageLink(name, tr("Scrittura a mano"));
        }
        if (!links.isEmpty()) {
            if (!markdown.isEmpty())
                markdown += QStringLiteral("\n\n");
            markdown += QStringLiteral("## %1\n\n").arg(tr("Allegati della pagina %1").arg(index + 1));
            markdown += links;
        }
    }

    const QByteArray bytes = markdown.toUtf8();
    QSaveFile destination(output.absoluteFilePath());
    if (!destination.open(QIODevice::WriteOnly) || destination.write(bytes) != bytes.size() || !destination.commit())
        return fail(tr("Impossibile salvare il Markdown. %1").arg(destination.errorString()));
    if (assets)
        assets->setAutoRemove(false);
    setError({});
    return true;
}

bool Notebook::exportChartCsv(const QUrl &file, const QString &data)
{
    const QFileInfo output(file.toLocalFile());
    if (!file.isLocalFile() || output.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) != 0
        || (output.exists() && output.canonicalFilePath() == QFileInfo(m_filePath).canonicalFilePath())
        || data.isEmpty() || data.size() > 32768)
        return fail(tr("Scegli un file .csv per salvare i dati del grafico."));

    QSaveFile destination(output.absoluteFilePath());
    const QByteArray bytes = data.toUtf8();
    if (!destination.open(QIODevice::WriteOnly) || destination.write(bytes) != bytes.size() || !destination.commit())
        return fail(tr("Impossibile salvare il CSV. %1").arg(destination.errorString()));
    setError({});
    return true;
}

bool Notebook::paintExportPage(QPainter &painter, const QSize &size, const Book &book, const Page &page)
{
    painter.fillRect(QRect(QPoint(), size), Qt::white);
    if (page.hasBackground()) {
        QString error;
        const auto background = page.source.isEmpty() ? QImage::fromData(page.backgroundData(), "JPEG")
            : renderDocumentPage(sourceForPage(book, page), 2400, error);
        if (background.isNull())
            return fail(error.isEmpty() ? tr("Impossibile leggere lo sfondo della pagina.") : error);
        paintPdfBackground(painter, size, background);
    } else {
        PageContent::paintTemplate(painter, size, page.paper);
    }
    PageContent::paintItems(painter, size, PageContent::resolveTableCharts(page.items));
    paintStoredStrokes(painter, size, visibleStrokes(page));
    return true;
}

bool Notebook::exportPdf(const QString &path, bool currentPageOnly)
{
    const Book *book = currentBook();
    if (!book)
        return fail(tr("Apri un quaderno da esportare."));
    if (currentPageOnly && !currentPage())
        return fail(tr("Apri una pagina da esportare."));
    const QFileInfo output(path);
    if (path.isEmpty() || output.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0
        || output.absoluteFilePath() == m_filePath
        || (output.exists() && output.canonicalFilePath() == QFileInfo(m_filePath).canonicalFilePath()))
        return fail(tr("Scegli un percorso per il PDF."));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(tr("Impossibile creare il PDF. %1").arg(file.errorString()));

    {
        QPdfWriter writer(&file);
        writer.setTitle(book->title);
        writer.setCreator(QStringLiteral("Quaderno"));
        writer.setResolution(144);
        writer.setPageLayout(QPageLayout(
            QPageSize(QSizeF(486, 648), QPageSize::Point, QStringLiteral("Quaderno"),
                      QPageSize::ExactMatch),
            QPageLayout::Portrait, QMarginsF(), QPageLayout::Point));
        QPainter painter;
        if (!painter.begin(&writer))
            return fail(tr("Impossibile iniziare a scrivere il PDF."));

        const QSize size = writer.pageLayout().paintRectPixels(writer.resolution()).size();
        const qsizetype first = currentPageOnly ? page() : 0;
        const qsizetype end = currentPageOnly ? first + 1 : book->pages.size();
        for (qsizetype index = first; index < end; ++index) {
            if (index > first && !writer.newPage())
                return fail(tr("Impossibile aggiungere una pagina al PDF."));
            if (!paintExportPage(painter, size, *book, book->pages.at(index)))
                return false;
        }
        // Keep the book's original pages intact. Note markers refer to these
        // appendices, where all typed text is paginated and ink has its own
        // fixed-aspect page instead of being clipped into the tiny marker.
        const qreal pageScale = qreal(size.width()) / 600;
        for (qsizetype index = first; index < end; ++index) {
            int noteNumber = 0;
            for (const QVariant &entry : book->pages.at(index).items) {
                const QVariantMap note = entry.toMap();
                if (note.value(QStringLiteral("type")) != QStringLiteral("note")) continue;
                ++noteNumber;
                const auto beginNotePage = [&](const QString &section) {
                    if (!writer.newPage()) return false;
                    painter.fillRect(QRect(QPoint(), size), Qt::white);
                    painter.save();
                    painter.scale(pageScale, pageScale);
                    QFont font = painter.font();
                    font.setPixelSize(18); font.setBold(true);
                    painter.setFont(font); painter.setPen(QColor("#242723"));
                    painter.drawText(QRectF(36, 26, 480, 30), Qt::AlignLeft | Qt::AlignVCenter,
                        tr("Nota %1 · Pagina %2").arg(noteNumber).arg(index + 1));
                    PageContent::paintNoteMarker(painter, QRectF(534, 27, 30, 30), note, noteNumber);
                    font.setPixelSize(12); font.setBold(false); painter.setFont(font);
                    painter.drawText(QRectF(36, 58, 528, 24), Qt::AlignLeft | Qt::AlignVCenter, section);
                    painter.restore();
                    return true;
                };
                const QString text = note.value(QStringLiteral("text")).toString();
                if (!text.trimmed().isEmpty()) {
                    QFont font;
                    font.setPixelSize(14);
                    QTextOption option;
                    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
                    const QSizeF bodySize(528, 660);
                    struct NoteLine { QTextLine line; int page; qreal y; };
                    QList<NoteLine> lines;
                    std::vector<std::unique_ptr<QTextLayout>> layouts;
                    QString plain = text;
                    plain.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
                    plain.replace(u'\r', u'\n');
                    int lastPage = 0;
                    qreal y = 0;
                    // Paginate complete visual lines. Drawing a clipped slice
                    // of one long document can split boundary glyphs and leave
                    // duplicate lines in a PDF reader's extracted text.
                    for (const QString &paragraph : plain.split(u'\n')) {
                        auto layout = std::make_unique<QTextLayout>(paragraph.isEmpty() ? QStringLiteral(" ") : paragraph,
                            font, painter.device());
                        layout->setTextOption(option);
                        layout->beginLayout();
                        for (;;) {
                            QTextLine line = layout->createLine();
                            if (!line.isValid()) break;
                            line.setLineWidth(bodySize.width());
                            line.setPosition(QPointF());
                            const qreal height = qMax<qreal>(1, line.height());
                            if (y > 0 && y + height > bodySize.height()) { ++lastPage; y = 0; }
                            lines.append({line, lastPage, y});
                            y += height;
                        }
                        layout->endLayout();
                        layouts.push_back(std::move(layout));
                    }
                    const int pages = lastPage + 1;
                    for (int part = 0; part < pages; ++part) {
                        if (!beginNotePage(tr("Testo · %1 / %2").arg(part + 1).arg(pages)))
                            return fail(tr("Impossibile aggiungere una pagina al PDF."));
                        painter.save();
                        painter.scale(pageScale, pageScale);
                        painter.setPen(QColor("#242723"));
                        painter.setClipRect(QRectF(QPointF(36, 96), bodySize));
                        for (const NoteLine &line : lines) {
                            if (line.page == part) line.line.draw(&painter, QPointF(36, 96 + line.y));
                        }
                        painter.restore();
                    }
                }
                const QVariantList noteInk = note.value(QStringLiteral("noteInk")).toList();
                if (!noteInk.isEmpty()) {
                    if (!beginNotePage(tr("Scrittura a mano")))
                        return fail(tr("Impossibile aggiungere una pagina al PDF."));
                    painter.save();
                    painter.scale(pageScale, pageScale);
                    painter.translate(36, 108);
                    const QSizeF canvas(528, 352);
                    painter.setPen(QPen(QColor("#9ea9b6"), .7));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(QRectF(QPointF(), canvas));
                    painter.setClipRect(QRectF(QPointF(), canvas));
                    paintStoredStrokes(painter, canvas, noteInk);
                    painter.restore();
                }
            }
        }
        if (!painter.end())
            return fail(tr("Impossibile completare il PDF."));
    }

    if (!file.commit())
        return fail(tr("Impossibile salvare il PDF. %1").arg(file.errorString()));
    setError({});
    return true;
}

bool Notebook::importFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(tr("Impossibile aprire il file dei quaderni."));
    if (file.size() > PageStore::maximumLibraryBytes)
        return fail(tr("Il file locale dei quaderni supera 256 MB."));
    const QByteArray contents = file.read(PageStore::maximumLibraryBytes + 1);
    if (file.error() != QFileDevice::NoError)
        return fail(tr("Impossibile leggere il file dei quaderni."));
    QByteArray payload = contents;
    if (PageStore::isManifest(contents)) {
        const auto snapshot = PageStore::read(path);
        if (!snapshot.error.isEmpty()) return fail(snapshot.error);
        payload = snapshot.payload;
    }
    const QVariantMap result = mergeImport(payload, false, {}, true);
    return !result.contains(QStringLiteral("error")) || fail(result.value(QStringLiteral("error")).toString());
}

QByteArray Notebook::exportLibraryForTransfer()
{
    if (!m_loadError.isEmpty()) { fail(m_loadError); return {}; }
    const QByteArray payload = serialize(m_state);
    if (payload.size() > maxImportBytes) {
        fail(tr("La libreria supera 64 MB. Trasferisci un quaderno alla volta."));
        return {};
    }
    return payload;
}

bool Notebook::importFromData(const QByteArray &contents)
{
    const QVariantMap result = mergeImport(contents, false);
    return !result.contains(QStringLiteral("error")) || fail(result.value(QStringLiteral("error")).toString());
}

QVariantMap Notebook::previewImportFromData(const QByteArray &contents)
{
    return mergeImport(contents, true);
}

QVariantMap Notebook::applyImportFromData(const QByteArray &contents, const QString &token)
{
    if (token.isEmpty())
        return {{QStringLiteral("error"), tr("Controlla l’anteprima prima di importare.")}};
    return mergeImport(contents, false, token);
}

QVariantMap Notebook::mergeImport(const QByteArray &contents, bool preview, const QString &expectedToken, bool localFile)
{
    const auto problem = [](const QString &message) {
        return QVariantMap{{QStringLiteral("error"), message}};
    };
    if (!m_loadError.isEmpty())
        return problem(m_loadError);
    if (contents.size() > (localFile ? PageStore::maximumLibraryBytes : maxImportBytes))
        return problem(localFile ? tr("Il file locale dei quaderni supera 256 MB.") : tr("Il file dei quaderni supera 64 MB."));

    QString token;
    if (preview || !expectedToken.isEmpty()) {
        QByteArray currentContents;
        const QString error = readFile(m_filePath, currentContents);
        if (!error.isEmpty())
            return problem(error);
        if (fileRevision(currentContents) != m_fileRevision)
            return problem(tr("Il file dei quaderni è cambiato. Riapri l’app per caricarlo."));
        token = QString::fromLatin1(fileRevision(m_fileRevision.toHex() + ':' + fileRevision(contents).toHex()).toHex());
        if (!expectedToken.isEmpty() && token != expectedToken)
            return problem(tr("La libreria del Mac è cambiata. Controlla una nuova anteprima."));
    }

    State incoming;
    const QString readError = readState(contents, incoming);
    if (!readError.isEmpty())
        return problem(readError);
    if (incoming.books.isEmpty())
        return problem(tr("Il file non contiene quaderni."));

    State next = m_state;
    int created = 0;
    int updated = 0;
    int unchanged = 0;
    int conflicts = 0;
    bool baselineChanged = false;
    for (const Folder &folder : std::as_const(incoming.folders)) {
        if (findFolder(next, folder.id) < 0)
            next.folders.append(folder);
    }
    for (Book &book : incoming.books) {
        const QByteArray revision = revisionOf(book);
        const int index = findBook(next, book.id);
        if (index >= 0) {
            const Book &local = next.books.at(index);
            if (local.title == book.title && local.pages == book.pages) {
                baselineChanged |= local.importedRevision != revision;
                next.books[index].importedRevision = revision;
                if (local.workspace.isEmpty() && !book.workspace.isEmpty()) {
                    next.books[index].workspace = book.workspace;
                    baselineChanged = true;
                }
                ++unchanged;
                continue;
            }

            if (!local.importedRevision.isEmpty() && local.importedRevision == revisionOf(local)) {
                preserveReadingPosition(book, local);
                preserveMarkdownBlocks(book, local);
                auto &replacement = next.books[index];
                replacement.title = std::move(book.title);
                replacement.pages = std::move(book.pages);
                replacement.lastViewedPageId = std::move(book.lastViewedPageId);
                replacement.workspace = std::move(book.workspace);
                replacement.importedRevision = revision;
                if (next.activeBook == index)
                    next.page = resumePage(replacement);
                ++updated;
                continue;
            }

            // A conflict becomes a stable copy, leaving the locally edited book available.
            static const QUuid copyNamespace(QStringLiteral("7badb84d-fccc-4699-8dd6-0209147969ad"));
            const QString copyId = QUuid::createUuidV5(copyNamespace, book.id.toUtf8() + revision)
                                       .toString(QUuid::WithoutBraces);
            if (findBook(next, copyId) >= 0) {
                ++unchanged;
                continue;
            }
            resetBookIds(book, copyId);
            book.title += tr(" (importato)");
            book.trashed = false;
            ++conflicts;
        } else {
            ++created;
        }
        book.lastViewedPageId.clear();
        book.importedRevision = revisionOf(book);
        next.books.append(std::move(book));
    }

    const QVariantMap result{{QStringLiteral("token"), token}, {QStringLiteral("created"), created},
                             {QStringLiteral("updated"), updated}, {QStringLiteral("unchanged"), unchanged},
                             {QStringLiteral("conflicts"), conflicts},
                             {QStringLiteral("foldersCreated"), next.folders.size() - m_state.folders.size()}};
    if (preview)
        return result;
    if (next.folders.size() == m_state.folders.size() && created == 0 && updated == 0 && conflicts == 0 && !baselineChanged) {
        setError({});
        return result;
    }
    if (next.activeBook < 0)
        selectActiveBook(next);
    if (!commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory))
        return problem(m_error);
    return result;
}

QByteArray Notebook::exportBookForTransfer(const QString &id)
{
    const int index = findBook(m_state, id);
    if (!m_loadError.isEmpty() || index < 0 || m_state.books.at(index).trashed) {
        fail(tr("Apri un quaderno disponibile da trasferire."));
        return {};
    }

    QByteArray currentContents;
    const QString readError = readFile(m_filePath, currentContents);
    if (!readError.isEmpty()) {
        fail(readError);
        return {};
    }
    if (fileRevision(currentContents) != m_fileRevision) {
        fail(tr("La libreria è stata modificata da un'altra sessione. Chiudi e riapri Quaderno."));
        return {};
    }

    State outbound;
    outbound.books.append(m_state.books.at(index));
    outbound.books.first().folderId.clear();
    outbound.books.first().lastViewedPageId.clear();
    outbound.activeBook = 0;
    const QByteArray payload = serialize(outbound);
    if (payload.size() > maxImportBytes) {
        fail(tr("Il quaderno supera il limite di trasferimento di 64 MiB."));
        return {};
    }
    setError({});
    return payload;
}

bool Notebook::recordBookTransfer(const QString &id, const QString &revision)
{
    const int index = findBook(m_state, id);
    const QByteArray hex = revision.toLatin1();
    const QByteArray decoded = QByteArray::fromHex(hex);
    if (index < 0 || hex.size() != 64 || decoded.toHex() != hex)
        return false;

    State next = m_state;
    next.books[index].importedRevision = decoded;
    return commit(std::move(next), 0);
}

QString Notebook::bookTransferRevision(const QString &id) const
{
    const int index = findBook(m_state, id);
    return index < 0 || m_state.books.at(index).trashed ? QString{}
        : QString::fromLatin1(revisionOf(m_state.books.at(index)).toHex());
}

QString Notebook::bookDisplayTitle(const QString &id) const
{
    const int index = findBook(m_state, id);
    return index < 0 || m_state.books.at(index).trashed ? QString{} : m_state.books.at(index).title;
}

int Notebook::bookPageNumber(const QString &id, const QString &pageId) const
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return 0;
    return findPage(m_state.books.at(index), pageId) + 1;
}

bool Notebook::canResumeBook(const QString &id, const QString &revision, const QString &pageId, int pageNumber) const
{
    const int index = findBook(m_state, id);
    return index >= 0 && pageAtRevision(m_state.books.at(index), revision, pageId, pageNumber) >= 0;
}

bool Notebook::openBookAtRevision(const QString &id, const QString &revision, const QString &pageId, int pageNumber)
{
    const int index = findBook(m_state, id);
    const int page = index < 0 ? -1 : pageAtRevision(m_state.books.at(index), revision, pageId, pageNumber);
    if (page < 0)
        return fail(tr("Questa versione della pagina non è disponibile sul dispositivo."));
    if (index != m_state.activeBook || page != m_state.page)
        return openBookPage(index, page);

    // A handoff can target the already open page. Check disk even when
    // there is no navigation to commit through the usual revision guard.
    QByteArray currentContents;
    const QString readError = readFile(m_filePath, currentContents);
    if (!readError.isEmpty())
        return fail(readError);
    if (fileRevision(currentContents) != m_fileRevision)
        return fail(tr("Il file dei quaderni è cambiato. Riapri l’app per caricarlo."));
    setError({});
    return true;
}

QVariantMap Notebook::receiveBookTransfer(const QByteArray &payload, const QByteArray &expectedToken,
                                          std::optional<QByteArray> baseOverride)
{
    const auto problem = [](const QString &message) {
        return QVariantMap{{QStringLiteral("error"), message}};
    };
    if (!m_loadError.isEmpty())
        return problem(m_loadError);
    if (baseOverride) {
        QByteArray currentContents;
        const QString error = readFile(m_filePath, currentContents);
        if (!error.isEmpty())
            return problem(error);
        if (fileRevision(currentContents) != m_fileRevision)
            return problem(tr("Il file dei quaderni è cambiato. Riapri l’app per caricarlo."));
    }
    if (payload.isEmpty() || payload.size() > maxImportBytes)
        return problem(tr("Il trasferimento richiede un singolo quaderno entro 64 MiB."));
    State incoming;
    const QString readError = readState(payload, incoming);
    if (!readError.isEmpty())
        return problem(readError);
    if (incoming.books.size() != 1 || !incoming.folders.isEmpty() || incoming.books.first().trashed)
        return problem(tr("Il trasferimento richiede un singolo quaderno disponibile."));

    Book book = std::move(incoming.books.first());
    const QByteArray sourceRevision = revisionOf(book);
    const int index = findBook(m_state, book.id);
    const Book *target = index < 0 ? nullptr : &m_state.books.at(index);
    const QByteArray targetRevision = target ? revisionOf(*target) : QByteArray{};
    const QByteArray base = baseOverride.value_or(book.importedRevision);
    const QString action = !target ? QStringLiteral("create") : target->trashed ? QStringLiteral("conflict")
        : targetRevision == sourceRevision ? QStringLiteral("unchanged")
        : targetRevision == base ? QStringLiteral("replace")
        : QStringLiteral("conflict");
    QByteArray tokenInput = QSysInfo::machineUniqueId().toHex() + ':' + m_fileRevision.toHex()
        + ':' + fileRevision(payload).toHex();
    if (baseOverride)
        tokenInput += ":wireless:" + base.toHex();
    const QByteArray token = fileRevision(tokenInput).toHex();
    qsizetype changedPages = 0;
    const qsizetype commonPages = target ? qMin(target->pages.size(), book.pages.size()) : 0;
    for (qsizetype page = 0; page < commonPages; ++page)
        changedPages += !(target->pages.at(page) == book.pages.at(page));
    changedPages += qMax(target ? target->pages.size() : 0, book.pages.size()) - commonPages;
    QVariantMap reply{{QStringLiteral("action"), action}, {QStringLiteral("token"), QString::fromLatin1(token)},
                      {QStringLiteral("bookId"), book.id}, {QStringLiteral("revision"), QString::fromLatin1(sourceRevision.toHex())},
                      {QStringLiteral("sourceTitle"), book.title}, {QStringLiteral("sourcePages"), book.pages.size()},
                      {QStringLiteral("targetTitle"), target ? target->title : QString{}},
                      {QStringLiteral("targetPages"), target ? target->pages.size() : 0},
                      {QStringLiteral("changedPages"), changedPages}};
    if (expectedToken.isEmpty())
        return reply;
    if (expectedToken != token) {
        if (baseOverride)
            return {{QStringLiteral("error"), tr("La libreria di destinazione è cambiata. Controlla una nuova anteprima.")},
                    {QStringLiteral("errorCode"), QStringLiteral("stale")}};
        return problem(tr("La libreria sul Paper Pro è cambiata. Controlla una nuova anteprima."));
    }
    if (action == QStringLiteral("unchanged")) {
        if (target->workspace.isEmpty() && !book.workspace.isEmpty()) {
            State next = m_state;
            next.books[index].workspace = book.workspace;
            if (!commit(std::move(next), BooksChanged | DocumentChanged)) return problem(m_error);
        }
        return reply;
    }

    State next = m_state;
    book.importedRevision = baseOverride ? QByteArray{} : sourceRevision;
    if (action == QStringLiteral("replace")) {
        preserveReadingPosition(book, *target);
        preserveMarkdownBlocks(book, *target);
        book.folderId = target->folderId;
        book.favorite = target->favorite;
        book.tags = target->tags;
        if (baseOverride)
            book.importedRevision = target->importedRevision;
        next.books[index] = std::move(book);
        if (next.activeBook == index)
            next.page = resumePage(next.books.at(index));
    } else {
        if (action == QStringLiteral("conflict")) {
            static const QUuid usbNamespace(QStringLiteral("560adadf-8fbc-4c94-9b9c-96f15f1fd6e9"));
            static const QUuid wirelessNamespace(QStringLiteral("9e855c8c-4fa1-4a41-8a3a-719a3dfffe21"));
            const auto &transferNamespace = baseOverride ? wirelessNamespace : usbNamespace;
            const QString copyId = QUuid::createUuidV5(transferNamespace, book.id.toUtf8() + sourceRevision)
                .toString(QUuid::WithoutBraces);
            const int copyIndex = findBook(next, copyId);
            resetBookIds(book, copyId);
            if (copyIndex >= 0) {
                const Book &existing = next.books.at(copyIndex);
                if (!existing.trashed && existing.pages == book.pages)
                    return reply;
                resetBookIds(book, newId());
            }
            book.title += baseOverride ? tr(" (copia wireless)") : tr(" (dal Mac)");
            book.importedRevision.clear();
        }
        book.lastViewedPageId.clear();
        next.books.append(std::move(book));
        if (next.activeBook < 0)
            selectActiveBook(next);
    }
    if (!commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory))
        return problem(m_error);
    return reply;
}

bool Notebook::createBook(const QString &title)
{
    return createBookInFolder(title, {});
}

bool Notebook::createBookInFolder(const QString &title, const QString &folderId)
{
    return createBookWithFirstPage(title, folderId, {});
}

bool Notebook::createDiagramBook(const QString &title, const QString &folderId)
{
    return createBookWithFirstPage(title, folderId, {}, QStringLiteral("diagrams"));
}

bool Notebook::createBookWithItem(const QString &title, const QString &folderId, const QVariantMap &item)
{
    if (!isLibraryItem(item.value(QStringLiteral("type"))) || item.contains(QStringLiteral("id"))
        || item.contains(QStringLiteral("sourceTable")))
        return fail(tr("Controlla il testo e le dimensioni dell’elemento."));

    const QVariantMap valid = newPlacedItem(item, {});
    if (valid.isEmpty())
        return fail(tr("Controlla il testo e le dimensioni dell’elemento."));

    Page page;
    page.items.append(valid);
    return createBookWithFirstPage(title, folderId, std::move(page),
        item.value(QStringLiteral("type")) == QStringLiteral("diagram") ? QStringLiteral("diagrams") : QString{});
}

bool Notebook::createBookWithFirstPage(const QString &title, const QString &folderId, Page page, const QString &workspace)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty())
        return fail(tr("Inserisci il nome del quaderno."));
    if (!folderId.isEmpty() && findFolder(m_state, folderId) < 0)
        return fail(tr("Impossibile trovare la cartella."));

    State next = m_state;
    Book book;
    book.id = newId();
    book.title = trimmedTitle;
    book.folderId = folderId;
    book.workspace = workspace;
    page.id = newId();
    book.pages.append(std::move(page));
    next.books.append(std::move(book));
    next.activeBook = static_cast<int>(next.books.size()) - 1;
    next.page = 0;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory);
}

QVariantMap Notebook::mixedPageSource() const
{
    if (!currentBook() || !currentPage() || m_fileRevision.isEmpty() || m_importing)
        return {};
    return {{QStringLiteral("bookId"), bookId()}, {QStringLiteral("pageId"), pageId()},
        {QStringLiteral("revision"), QString::fromLatin1(m_fileRevision.toHex())},
        {QStringLiteral("title"), title()}, {QStringLiteral("content"), pagePreview(page())}};
}

bool Notebook::createMixedDocument(const QString &sourceBookId, const QString &sourcePageId,
                                   const QString &sourceRevision, const QString &title, const QString &markdown,
                                   const QVariantMap &diagram, const QVariantMap &table)
{
    const Book *source = currentBook();
    if (m_importing || !source || !currentPage() || sourceBookId != source->id || sourcePageId != pageId()
        || sourceRevision.isEmpty() || sourceRevision != QString::fromLatin1(m_fileRevision.toHex()))
        return fail(tr("La pagina sorgente è cambiata. Riapri l’estrazione."));
    const QString baseTitle = title.simplified();
    if (baseTitle.isEmpty() || !baseTitle.isValidUtf16() || baseTitle.contains(QChar::Null))
        return fail(tr("Inserisci il nome del quaderno."));
    if (markdown.trimmed().isEmpty())
        return fail(tr("Inserisci il testo della trascrizione."));
    const auto fragments = validatedMarkdownFragments(markdown);
    if (fragments.isEmpty())
        return false;

    const auto outputItem = [](QVariantMap draft, const QString &type) {
        const QString givenType = draft.value(QStringLiteral("type")).toString();
        if (draft.isEmpty() || (!givenType.isEmpty() && givenType != type))
            return QVariantMap{};
        draft.insert(QStringLiteral("type"), type);
        draft.remove(QStringLiteral("handwritingSource"));
        return newPlacedItem(std::move(draft), {});
    };
    const QVariantMap diagramItem = outputItem(diagram, QStringLiteral("diagram"));
    if (diagramItem.isEmpty())
        return fail(tr("Controlla le forme, le etichette e i collegamenti del diagramma."));
    const QVariantMap tableItem = outputItem(table, QStringLiteral("table"));
    if (tableItem.isEmpty())
        return fail(tr("Controlla il testo e le dimensioni dell’elemento."));

    Book writingBook;
    writingBook.id = newId();
    writingBook.title = baseTitle.left(80).trimmed();
    if (writingBook.title.back().isHighSurrogate())
        writingBook.title.chop(1);
    writingBook.folderId = source->folderId;
    writingBook.handwritingLanguage = source->handwritingLanguage;
    for (const QString &fragment : fragments)
        writingBook.pages.append(newMarkdownPage(fragment));
    const auto addObjectPage = [](Book &book, const QVariantMap &item) {
        Page page;
        page.id = newId();
        page.items.append(item);
        book.pages.append(std::move(page));
        return QVariantMap{{QStringLiteral("bookId"), book.id}, {QStringLiteral("pageId"), book.pages.last().id},
            {QStringLiteral("itemId"), item.value(QStringLiteral("id"))}, {QStringLiteral("type"), item.value(QStringLiteral("type"))}};
    };
    const QVariantMap diagramTarget = addObjectPage(writingBook, diagramItem);
    const QVariantMap tableTarget = addObjectPage(writingBook, tableItem);
    // The first page has room below the Markdown frame for both internal links.
    const QVariantMap diagramLink = pageLinkItem(diagramTarget, {.09, .966});
    const QVariantMap tableLink = pageLinkItem(tableTarget, {.17, .966});
    if (diagramLink.isEmpty() || tableLink.isEmpty())
        return fail(tr("La destinazione del collegamento non è disponibile in questa libreria."));
    writingBook.pages.first().items.append(diagramLink);
    writingBook.pages.first().items.append(tableLink);

    State next = m_state;
    next.books.append(std::move(writingBook));
    next.activeBook = static_cast<int>(next.books.size()) - 1;
    next.page = 0;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory);
}

bool Notebook::importDocument(const QUrl &file, const QString &folderId)
{
    if (!file.isLocalFile())
        return fail(tr("Scegli un PDF, un EPUB o un Markdown salvato sul dispositivo."));
    if (!folderId.isEmpty() && findFolder(m_state, folderId) < 0)
        return fail(tr("Impossibile trovare la cartella."));

    const QString path = file.toLocalFile();
    const QFileInfo info(path);
    const QString extension = info.suffix().toLower();
    if (isMarkdownExtension(extension))
        return importMarkdownFile(file, folderId);

    return importPreparedDocument(prepareDocument(path), info.completeBaseName(), folderId);
}

bool Notebook::importMarkdownFile(const QUrl &file, const QString &folderId, bool activate, QString *importedId)
{
    const QFileInfo info(file.toLocalFile());
    QFile input(info.absoluteFilePath());
    if (!info.isFile() || !input.open(QIODevice::ReadOnly))
        return fail(tr("Impossibile aprire il file Markdown."));
    const QByteArray bytes = input.read(maximumMarkdownBytes + 1);
    if (input.error() != QFileDevice::NoError)
        return fail(tr("Impossibile leggere il file Markdown."));
    if (bytes.size() > maximumMarkdownBytes)
        return fail(tr("Il Markdown supera il limite di 256 KiB."));
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless | QStringDecoder::Flag::ConvertInitialBom);
    const QString decoded = decoder(bytes);
    if (decoder.hasError())
        return fail(tr("Il Markdown deve essere un file di testo UTF-8."));
    const QString id = newId();
    if (!importLinkedMarkdown(decoded, info.completeBaseName(), folderId, id, activate)) return false;
    if (importedId) *importedId = id;
    return true;
}

bool Notebook::importPreparedDocument(PreparedDocument prepared, const QString &title, const QString &folderId, bool activate, QString *importedId)
{
    if (!prepared.error.isEmpty() || prepared.pages.isEmpty())
        return fail(prepared.error.isEmpty() ? tr("Impossibile importare il documento.") : prepared.error);
    if (!folderId.isEmpty() && findFolder(m_state, folderId) < 0)
        return fail(tr("La cartella di destinazione non esiste più. Ripeti l’importazione."));
    Book book;
    book.id = newId();
    book.title = title.left(80).trimmed();
    if (book.title.isEmpty())
        book.title = tr("Documento importato");
    book.folderId = folderId;
    book.sourceDocuments = std::move(prepared.sources);
    for (const QVariant &entry : std::as_const(prepared.pages)) {
        const auto descriptor = entry.toMap();
        Page page;
        page.id = newId();
        page.source = descriptor.value(QStringLiteral("source")).toMap();
        page.pdfText = descriptor.value(QStringLiteral("text")).toString().left(maxPagePdfText);
        book.pages.append(std::move(page));
    }
    const QString id = book.id;
    State next = m_state;
    next.books.append(std::move(book));
    int changes = BooksChanged;
    if (activate) {
        next.activeBook = static_cast<int>(next.books.size()) - 1;
        next.page = 0;
        changes |= DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory;
    }
    if (!commit(std::move(next), changes)) return false;
    if (importedId) *importedId = id;
    return true;
}

bool Notebook::startDocumentImport(const QUrl &file, const QString &folderId)
{
    if (m_importing)
        return fail(tr("Attendi o annulla l’importazione in corso."));
    if (!file.isLocalFile() || (!file.host().isEmpty() && file.host() != QStringLiteral("localhost")))
        return fail(tr("Scegli un PDF, un EPUB o un Markdown salvato sul dispositivo."));
    if (!folderId.isEmpty() && findFolder(m_state, folderId) < 0)
        return fail(tr("Impossibile trovare la cartella."));
    const QFileInfo info(file.toLocalFile());
    if (isMarkdownExtension(info.suffix())) {
        QString importedId;
        const bool ok = importMarkdownFile(file, folderId, false, &importedId);
        if (ok)
            emit documentImported(importedId);
        emit importFinished(ok);
        return ok;
    }
    const QString extension = info.suffix().toLower();
    if (extension != QStringLiteral("pdf") && extension != QStringLiteral("epub"))
        return fail(tr("Il formato deve essere PDF, EPUB o Markdown."));
    m_importing = true;
    m_importProgress = 0;
    m_importStatus = tr("Preparazione di %1…").arg(info.fileName());
    setError({});
    emit importChanged();
    m_importCancellation = startDocumentPreparation(info.absoluteFilePath(), this,
        [this](int done, int total) {
            if (!m_importing || (m_importCancellation && m_importCancellation->load()))
                return;
            m_importProgress = total > 0 ? qBound(0.0, double(done) / total, 1.0) : 0;
            emit importChanged();
        },
        [this, title = info.completeBaseName(), folderId](PreparedDocument prepared) {
            const bool cancelled = m_importCancellation && m_importCancellation->load();
            bool success = false;
            QString importedId;
            if (!cancelled)
                success = importPreparedDocument(std::move(prepared), title, folderId, false, &importedId);
            m_importing = false;
            m_importCancellation.reset();
            m_importProgress = success ? 1 : 0;
            m_importStatus = cancelled ? tr("Importazione annullata.")
                : success ? tr("Documento importato.") : error();
            emit importChanged();
            if (success)
                emit documentImported(importedId);
            emit importFinished(success);
        });
    return true;
}

void Notebook::cancelDocumentImport()
{
    if (!m_importing || !m_importCancellation)
        return;
    m_importCancellation->store(true);
    m_importStatus = tr("Annullamento dell’importazione…");
    emit importChanged();
}

QString Notebook::normalizeMarkdownText(QString text)
{
    if (text.startsWith(QChar::ByteOrderMark))
        text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(u'\r', u'\n');
    return text;
}

QList<QString> Notebook::validatedMarkdownFragments(const QString &text)
{
    const auto tooLarge = [this] {
        fail(tr("Il Markdown supera il limite di 256 KiB."));
        return QList<QString>{};
    };
    if (text.size() > maximumMarkdownBytes)
        return tooLarge();
    if (!text.isValidUtf16() || text.contains(QChar::Null)) {
        fail(tr("Il Markdown deve essere un file di testo UTF-8."));
        return {};
    }
    if (text.toUtf8().size() > maximumMarkdownBytes)
        return tooLarge();
    auto fragments = paginateMarkdown(normalizeMarkdownText(text));
    if (fragments.isEmpty())
        fail(tr("Il Markdown supera il limite di 200 pagine."));
    return fragments;
}

Notebook::Page Notebook::newMarkdownPage(const QString &fragment)
{
    Page page;
    page.id = newId();
    page.markdownBlockId = newId();
    page.items.append(QVariantMap{
        {QStringLiteral("id"), page.markdownBlockId},
        {QStringLiteral("type"), QStringLiteral("text")},
        {QStringLiteral("text"), fragment},
        {QStringLiteral("x"), (1.0 - markdownWidth) / 2},
        {QStringLiteral("y"), (1.0 - markdownHeight) / 2},
        {QStringLiteral("width"), markdownWidth},
        {QStringLiteral("height"), markdownHeight},
    });
    return page;
}

bool Notebook::importMarkdownText(const QString &text, const QString &title, const QString &folderId)
{
    return importLinkedMarkdown(text, title, folderId, newId());
}

bool Notebook::importLinkedMarkdown(const QString &text, const QString &title, const QString &folderId, const QString &id,
                                    bool activate)
{
    if (!validUuid(id) || hasBookId(id))
        return fail(tr("L’identificatore del quaderno non è valido o è già in uso."));
    if (!folderId.isEmpty() && findFolder(m_state, folderId) < 0)
        return fail(tr("Impossibile trovare la cartella."));
    const auto fragments = validatedMarkdownFragments(text);
    if (fragments.isEmpty())
        return false;

    Book book;
    book.id = id;
    book.title = title.left(80).trimmed();
    if (book.title.isEmpty())
        book.title = tr("Documento importato");
    book.folderId = folderId;
    for (const QString &fragment : fragments)
        book.pages.append(newMarkdownPage(fragment));

    State next = m_state;
    next.books.append(std::move(book));
    int changes = BooksChanged;
    if (activate) {
        next.activeBook = static_cast<int>(next.books.size()) - 1;
        next.page = 0;
        changes |= DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory;
    }
    return commit(std::move(next), changes, true);
}

bool Notebook::refreshMarkdownBook(const QString &id, const QString &text, const QString &expectedBookRevision)
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno."));
    const Book &book = m_state.books.at(index);
    if (QString::fromLatin1(revisionOf(book).toHex()) != expectedBookRevision)
        return fail(tr("Il quaderno è cambiato durante la sincronizzazione."));
    if (!isPureMarkdown(book))
        return fail(tr("Il quaderno contiene annotazioni o altri contenuti locali."));
    return replaceMarkdownSource(index, text);
}

bool Notebook::updateMarkdownText(const QString &text)
{
    if (!markdownBook())
        return fail(tr("Apri un documento Markdown per modificarne il testo."));
    return replaceMarkdownSource(m_state.activeBook, text);
}

bool Notebook::replaceMarkdownSource(int bookIndex, const QString &text)
{
    const auto fragments = validatedMarkdownFragments(text);
    if (fragments.isEmpty())
        return false;

    const Book &book = m_state.books.at(bookIndex);
    State next = m_state;
    Book &updated = next.books[bookIndex];
    updated.pages.clear();
    updated.pages.reserve(qMax(book.pages.size(), fragments.size()));
    qsizetype fragment = 0;
    qsizetype afterSource = 0;
    for (const Page &page : book.pages) {
        if (page.markdownBlockId.isEmpty()) {
            updated.pages.append(page);
            continue;
        }
        const int source = findPageItem(page, page.markdownBlockId);
        if (fragment < fragments.size()) {
            Page retained = page;
            QVariantMap item = retained.items.at(source).toMap();
            item.insert(QStringLiteral("text"), fragments.at(fragment++));
            retained.items[source] = std::move(item);
            updated.pages.append(std::move(retained));
        } else if (hasMarkdownPageAnnotations(page)) {
            Page retained = page;
            retained.items.removeAt(source);
            retained.markdownBlockId.clear();
            updated.pages.append(std::move(retained));
        }
        afterSource = updated.pages.size();
    }
    for (; fragment < fragments.size(); ++fragment)
        updated.pages.insert(afterSource++, newMarkdownPage(fragments.at(fragment)));
    if (!updated.lastViewedPageId.isEmpty())
        preserveReadingPosition(updated, book);

    int changes = BooksChanged;
    if (bookIndex == next.activeBook) {
        next.page = resumePage(updated);
        changes |= DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory;
    }
    return commit(std::move(next), changes, true);
}

bool Notebook::openBook(const QString &id)
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno."));

    return openBookPage(index, resumePage(m_state.books.at(index)));
}

QVariantMap Notebook::openLibraryItem(const QString &bookId, const QString &pageId, const QString &itemId)
{
    const int bookIndex = findBook(m_state, bookId);
    if (bookIndex < 0 || m_state.books.at(bookIndex).trashed) {
        fail(tr("Impossibile trovare il quaderno."));
        return {};
    }
    const Book &book = m_state.books.at(bookIndex);
    const int pageIndex = findPage(book, pageId);
    if (pageIndex < 0) {
        fail(tr("Impossibile trovare la pagina."));
        return {};
    }
    const Page &page = book.pages.at(pageIndex);
    if (itemId.isEmpty() && book.workspace == QStringLiteral("diagrams")) {
        return openBookPage(bookIndex, pageIndex)
            ? QVariantMap{{QStringLiteral("workspace"), true}, {QStringLiteral("type"), QStringLiteral("diagram")}}
            : QVariantMap{};
    }
    const int itemIndex = findPageItem(page, itemId);
    if (itemIndex < 0 || !isLibraryItem(page.items.at(itemIndex).toMap().value(QStringLiteral("type")))) {
        fail(tr("L’elemento non è più presente sulla pagina."));
        return {};
    }

    const QVariantMap item = PageContent::resolveTableCharts(page.items).at(itemIndex).toMap();
    return openBookPage(bookIndex, pageIndex) ? item : QVariantMap{};
}

std::optional<Notebook::LinkLocation> Notebook::pageLinkTarget(const QVariantMap &link) const
{
    const int bookIndex = findBook(m_state, link.value(QStringLiteral("targetBookId")).toString());
    if (bookIndex < 0 || m_state.books.at(bookIndex).trashed)
        return {};
    const Book &book = m_state.books.at(bookIndex);
    const int pageIndex = findPage(book, link.value(QStringLiteral("targetPageId")).toString());
    if (pageIndex < 0)
        return {};
    const Page &page = book.pages.at(pageIndex);
    const int itemIndex = findPageItem(page, link.value(QStringLiteral("targetItemId")).toString());
    if (itemIndex < 0)
        return {};
    const QVariant type = page.items.at(itemIndex).toMap().value(QStringLiteral("type"));
    if (!isLibraryItem(type) || type != link.value(QStringLiteral("targetType")))
        return {};
    return LinkLocation{bookIndex, pageIndex, itemIndex};
}

QVariantMap Notebook::linkedPageItem(const QString &id) const
{
    const Page *page = currentPage();
    const int index = page ? findPageItem(*page, id) : -1;
    const QVariantMap link = index < 0 ? QVariantMap{} : page->items.at(index).toMap();
    if (link.value(QStringLiteral("type")) != QStringLiteral("link"))
        return {};
    const auto location = pageLinkTarget(link);
    if (!location)
        return {};
    const Book &book = m_state.books.at(location->book);
    const Page &targetPage = book.pages.at(location->page);
    QVariantMap item = PageContent::resolveTableCharts(targetPage.items).at(location->item).toMap();
    const QString itemTitle = libraryItemTitle(item, book.title);
    const QString workspace = workspaceForPage(targetPage, book);
    const bool internalPage = location->book == m_state.activeBook && location->page != m_state.page;
    item.insert(QStringLiteral("bookId"), book.id);
    item.insert(QStringLiteral("pageId"), targetPage.id);
    item.insert(QStringLiteral("bookTitle"), book.title);
    item.insert(QStringLiteral("pageNumber"), location->page + 1);
    item.insert(QStringLiteral("linkId"), id);
    item.insert(QStringLiteral("pageWorkspace"), workspace);
    item.insert(QStringLiteral("internalPage"), internalPage);
    item.insert(QStringLiteral("internalObjectSheet"), internalPage
        && workspace == objectWorkspace(item.value(QStringLiteral("type"))));
    item.insert(QStringLiteral("displayTitle"), itemTitle.isEmpty() ? book.title : itemTitle);
    return item;
}

QVariantMap Notebook::openPageLink(const QString &id)
{
    const Page *page = currentPage();
    const int index = page ? findPageItem(*page, id) : -1;
    const QVariantMap link = index < 0 ? QVariantMap{} : page->items.at(index).toMap();
    if (link.value(QStringLiteral("type")) != QStringLiteral("link")) {
        fail(tr("Il collegamento non è più presente sulla pagina."));
        return {};
    }
    const auto location = pageLinkTarget(link);
    if (!location) {
        fail(tr("La destinazione del collegamento non è disponibile in questa libreria."));
        return {};
    }
    const Page &targetPage = m_state.books.at(location->book).pages.at(location->page);
    const QVariantMap item = PageContent::resolveTableCharts(targetPage.items).at(location->item).toMap();
    return openBookPage(location->book, location->page) ? item : QVariantMap{};
}

bool Notebook::openBookPage(int book, int page)
{
    if (book == m_state.activeBook && page == m_state.page) {
        setError({});
        return true;
    }

    State next = m_state;
    next.activeBook = book;
    next.page = page;
    return commit(std::move(next), DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory);
}

bool Notebook::renameBook(const QString &title)
{
    const Book *book = currentBook();
    if (!book)
        return fail(tr("Apri un quaderno da rinominare."));

    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty())
        return fail(tr("Inserisci il nome del quaderno."));

    if (book->title == trimmedTitle) {
        setError({});
        return true;
    }

    State next = m_state;
    next.books[next.activeBook].title = trimmedTitle;
    return commit(std::move(next), BooksChanged | DocumentChanged);
}

bool Notebook::createFolder(const QString &title, const QString &parentId)
{
    const QString name = title.trimmed();
    if (name.isEmpty() || name.size() > 80)
        return fail(tr("Inserisci un nome di cartella tra 1 e 80 caratteri."));
    if (!parentId.isEmpty() && findFolder(m_state, parentId) < 0)
        return fail(tr("Impossibile trovare la cartella superiore."));

    State next = m_state;
    next.folders.append(Folder{newId(), name, parentId});
    return commit(std::move(next), BooksChanged);
}

bool Notebook::renameFolder(const QString &id, const QString &title)
{
    const int index = findFolder(m_state, id);
    const QString name = title.trimmed();
    if (index < 0)
        return fail(tr("Impossibile trovare la cartella."));
    if (name.isEmpty() || name.size() > 80)
        return fail(tr("Inserisci un nome di cartella tra 1 e 80 caratteri."));

    State next = m_state;
    next.folders[index].title = name;
    return commit(std::move(next), BooksChanged);
}

bool Notebook::removeFolder(const QString &id)
{
    const int index = findFolder(m_state, id);
    if (index < 0)
        return fail(tr("Impossibile trovare la cartella."));

    for (const Folder &folder : std::as_const(m_state.folders)) {
        if (folder.parentId == id)
            return fail(tr("Svuota la cartella prima di eliminarla."));
    }
    for (const Book &book : std::as_const(m_state.books)) {
        if (book.folderId == id && !book.trashed)
            return fail(tr("Svuota la cartella prima di eliminarla."));
    }

    State next = m_state;
    next.folders.removeAt(index);
    for (Book &book : next.books) {
        if (book.folderId == id)
            book.folderId.clear();
    }
    return commit(std::move(next), BooksChanged);
}

bool Notebook::moveBook(const QString &id, const QString &folderId)
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno."));
    if (!folderId.isEmpty() && findFolder(m_state, folderId) < 0)
        return fail(tr("Impossibile trovare la cartella."));

    State next = m_state;
    next.books[index].folderId = folderId;
    return commit(std::move(next), BooksChanged);
}

bool Notebook::setFavorite(const QString &id, bool favorite)
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno."));

    State next = m_state;
    next.books[index].favorite = favorite;
    return commit(std::move(next), BooksChanged);
}

bool Notebook::setBookTags(const QString &id, const QVariantList &input)
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno."));

    QStringList tags;
    if (!normalizedTags(input, tags))
        return fail(tr("Usa al massimo 20 tag, ciascuno tra 1 e 40 caratteri."));

    State next = m_state;
    next.books[index].tags = tags;
    return commit(std::move(next), BooksChanged);
}

bool Notebook::trashBook(const QString &id)
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno."));

    State next = m_state;
    next.books[index].trashed = true;
    int changes = BooksChanged;
    if (next.activeBook == index) {
        selectActiveBook(next);
        changes |= DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory;
    }
    return commit(std::move(next), changes);
}

bool Notebook::restoreBook(const QString &id)
{
    const int index = findBook(m_state, id);
    if (index < 0 || !m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno nel cestino."));

    State next = m_state;
    next.books[index].trashed = false;
    int changes = BooksChanged;
    if (next.activeBook < 0) {
        selectActiveBook(next);
        changes |= DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory;
    }
    return commit(std::move(next), changes);
}

bool Notebook::purgeBook(const QString &id)
{
    const int index = findBook(m_state, id);
    if (index < 0 || !m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno nel cestino."));

    State next = m_state;
    next.books.removeAt(index);
    if (next.activeBook > index)
        --next.activeBook;
    return commit(std::move(next), BooksChanged);
}

bool Notebook::duplicateBook(const QString &id)
{
    const int index = findBook(m_state, id);
    if (index < 0 || m_state.books.at(index).trashed)
        return fail(tr("Impossibile trovare il quaderno."));

    State next = m_state;
    Book copy = next.books.at(index);
    resetBookIds(copy, newId());
    copy.title = copy.title.left(72).trimmed() + tr(" (copia)");
    copy.importedRevision.clear();
    next.books.append(std::move(copy));
    return commit(std::move(next), BooksChanged, true);
}

bool Notebook::goToPage(int page)
{
    if (page < 0 || page >= pageCount())
        return fail(tr("Impossibile trovare la pagina."));

    return openBookPage(m_state.activeBook, page);
}

bool Notebook::openPage(const QString &id)
{
    const Book *active = currentBook();
    if (active) {
        const int page = findPage(*active, id);
        if (page >= 0)
            return goToPage(page);
    }
    int matchBook = -1;
    int matchPage = -1;
    for (int index = 0; index < m_state.books.size(); ++index) {
        const Book &book = m_state.books.at(index);
        if (book.trashed)
            continue;
        const int page = findPage(book, id);
        if (page >= 0) {
            if (matchBook >= 0)
                return fail(tr("La pagina è presente in più quaderni."));
            matchBook = index;
            matchPage = page;
        }
    }
    if (matchBook < 0)
        return fail(tr("Impossibile trovare la pagina."));
    return openBookPage(matchBook, matchPage);
}

bool Notebook::addPage()
{
    if (!currentPage())
        return fail(tr("Apri un quaderno per aggiungere una pagina."));

    State next = m_state;
    auto &pages = next.books[next.activeBook].pages;
    Page page;
    page.id = newId();
    page.paper = pageTemplate();
    pages.append(std::move(page));
    next.page = static_cast<int>(pages.size()) - 1;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory);
}

QVariantMap Notebook::pagePreview(int index) const
{
    const Book *book = currentBook();
    if (!book || index < 0 || index >= book->pages.size())
        return {};
    const Page &page = book->pages.at(index);
    return {{QStringLiteral("pageId"), page.id}, {QStringLiteral("strokes"), visibleStrokes(page)},
            {QStringLiteral("items"), PageContent::resolveTableCharts(page.items)},
            {QStringLiteral("paper"), page.paper},
            {QStringLiteral("background"), page.backgroundData()},
            {QStringLiteral("source"), sourceForPage(*book, page)},
            {QStringLiteral("tags"), page.tags}};
}

QVariantList Notebook::pageSummaries() const
{
    QVariantList result;
    const Book *book = currentBook();
    if (!book) return result;
    result.reserve(book->pages.size());
    for (int index = 0; index < book->pages.size(); ++index) {
        const Page &page = book->pages.at(index);
        const QString workspace = workspaceForPage(page, *book);
        QString title;
        if (workspace == QStringLiteral("charts") || workspace == QStringLiteral("tables") || workspace == QStringLiteral("diagrams")) {
            const auto target = std::find_if(page.items.cbegin(), page.items.cend(), [&](const QVariant &entry) {
                return objectWorkspace(entry.toMap().value(QStringLiteral("type"))) == workspace;
            });
            if (target != page.items.cend())
                title = libraryItemTitle(target->toMap(), book->title);
        }
        bool annotated = !page.strokes.isEmpty() || !page.items.isEmpty() || !page.transcripts.isEmpty();
        for (const InkLayer &layer : page.layers)
            annotated |= !layer.strokes.isEmpty();
        result.append(QVariantMap{{QStringLiteral("index"), index}, {QStringLiteral("pageId"), page.id},
            {QStringLiteral("bookmarked"), page.bookmarked}, {QStringLiteral("annotated"), annotated},
            {QStringLiteral("workspace"), workspace}, {QStringLiteral("title"), title}});
    }
    return result;
}

QVariantList Notebook::searchPages(const QString &query) const
{
    const Book *book = currentBook();
    const QString needle = query.trimmed();
    QVariantList matches;
    if (!book || needle.isEmpty())
        return matches;

    for (int index = 0; index < book->pages.size(); ++index) {
        const Page &page = book->pages.at(index);
        QString excerpt;
        QString source;
        for (const QString &transcript : page.transcripts) {
            excerpt = matchExcerpt(transcript, needle);
            if (!excerpt.isEmpty()) {
                source = QStringLiteral("handwriting");
                break;
            }
        }
        for (const QString &tag : page.tags) {
            if (!excerpt.isEmpty())
                break;
            if (tag.contains(needle, Qt::CaseInsensitive)) {
                excerpt = QStringLiteral("#%1").arg(tag);
                break;
            }
        }
        for (const QVariant &entry : PageContent::resolveTableCharts(page.items)) {
            if (!excerpt.isEmpty())
                break;
            const QVariantMap item = entry.toMap();
            const QString type = item.value(QStringLiteral("type")).toString();
            QVariantList values = type == QStringLiteral("table") ? item.value(QStringLiteral("cells")).toList()
                : type == QStringLiteral("chart") ? QVariantList{item.value(QStringLiteral("title")),
                     item.value(QStringLiteral("xLabel")), item.value(QStringLiteral("yLabel"))}
                : QVariantList{item.value(QStringLiteral("text"))};
            if (type == QStringLiteral("table"))
                values.append(PageContent::resolvedTableCells(item));
            if (type == QStringLiteral("diagram")) {
                for (const auto &key : {QStringLiteral("nodes"), QStringLiteral("edges")}) {
                    const QVariantList parts = item.value(key).toList();
                    for (const QVariant &part : parts)
                        values.append(part.toMap().value(QStringLiteral("text")));
                }
            }
            for (const QVariant &value : values) {
                excerpt = matchExcerpt(value.toString(), needle);
                if (!excerpt.isEmpty())
                    break;
            }
            if (!excerpt.isEmpty())
                break;
        }
        if (excerpt.isEmpty()) {
            excerpt = matchExcerpt(page.pdfText, needle);
            if (!excerpt.isEmpty())
                source = QStringLiteral("pdf");
        }
        if (!excerpt.isEmpty())
            matches.append(QVariantMap{{QStringLiteral("page"), index}, {QStringLiteral("pageId"), page.id},
                                       {QStringLiteral("excerpt"), excerpt},
                                       {QStringLiteral("source"), source}});
    }
    return matches;
}

QVariantList Notebook::pageBookmarks() const
{
    QVariantList bookmarks;
    const Book *book = currentBook();
    if (!book)
        return bookmarks;
    for (int index = 0; index < book->pages.size(); ++index) {
        if (book->pages.at(index).bookmarked)
            bookmarks.append(index);
    }
    return bookmarks;
}

QVariantList Notebook::pageBookmarkEntries() const
{
    QVariantList entries;
    const Book *book = currentBook();
    if (!book)
        return entries;
    for (int index = 0; index < book->pages.size(); ++index) {
        const Page &page = book->pages.at(index);
        if (page.bookmarked)
            entries.append(QVariantMap{{QStringLiteral("page"), index}, {QStringLiteral("pageId"), page.id}});
    }
    return entries;
}

bool Notebook::togglePageBookmark()
{
    if (!currentPage())
        return false;
    State next = m_state;
    Page &page = currentPage(next);
    page.bookmarked = !page.bookmarked;
    return commit(std::move(next), PageContentChanged);
}

bool Notebook::duplicatePage(int index)
{
    const Book *book = currentBook();
    if (!book || index < 0 || index >= book->pages.size())
        return fail(tr("Impossibile trovare la pagina."));

    State next = m_state;
    Page copy = book->pages.at(index);
    copy.id = newId();
    next.books[next.activeBook].pages.insert(index + 1, std::move(copy));
    if (index < next.page)
        ++next.page;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory, true);
}

bool Notebook::deletePage(int index)
{
    const Book *book = currentBook();
    if (!book || index < 0 || index >= book->pages.size())
        return fail(tr("Impossibile trovare la pagina."));
    if (book->pages.size() == 1)
        return fail(tr("Il quaderno deve avere almeno una pagina."));

    State next = m_state;
    auto &pages = next.books[next.activeBook].pages;
    pages.removeAt(index);
    if (index < next.page)
        --next.page;
    else if (next.page >= pages.size())
        next.page = static_cast<int>(pages.size()) - 1;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory);
}

bool Notebook::movePage(int from, int to)
{
    const int count = pageCount();
    if (from < 0 || to < 0 || from >= count || to >= count)
        return fail(tr("Impossibile spostare la pagina."));
    if (from == to) {
        setError({});
        return true;
    }

    State next = m_state;
    next.books[next.activeBook].pages.move(from, to);
    if (next.page == from)
        next.page = to;
    else if (from < next.page && to >= next.page)
        --next.page;
    else if (from > next.page && to <= next.page)
        ++next.page;
    return commit(std::move(next), LibraryItemsChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory);
}

bool Notebook::transferPage(int index, const QString &targetBookId)
{
    const Book *book = currentBook();
    const int target = findBook(m_state, targetBookId);
    if (!book || index < 0 || index >= book->pages.size())
        return fail(tr("Impossibile trovare la pagina."));
    if (target < 0 || target == m_state.activeBook || m_state.books.at(target).trashed)
        return fail(tr("Scegli un altro quaderno disponibile."));

    State next = m_state;
    auto &source = next.books[next.activeBook].pages;
    Book &destination = next.books[target];
    Page moved = source.takeAt(index);
    for (const auto &key : {QStringLiteral("asset"), QStringLiteral("origin")}) {
        const QString id = moved.source.value(key).toString();
        if (!id.isEmpty())
            destination.sourceDocuments.insert(id, book->sourceDocuments.value(id));
    }
    // A partially synchronized transfer can leave this page in both books.
    // A second move into the same book must give the distinct copy a new ID.
    if (findPage(destination, moved.id) >= 0)
        moved.id = newId();
    if (index == next.page)
        destination.lastViewedPageId = moved.id;
    destination.pages.append(std::move(moved));
    if (source.isEmpty()) {
        Page blank;
        blank.id = newId();
        source.append(std::move(blank));
    }
    if (index < next.page)
        --next.page;
    else if (next.page >= source.size())
        next.page = static_cast<int>(source.size()) - 1;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory, true);
}

bool Notebook::appendStroke(const QVariantMap &stroke)
{
    const Page *page = currentPage();
    if (!page)
        return fail(tr("Apri un quaderno per scrivere."));
    if (!canWriteLayer())
        return fail(tr("Mostra il livello per scrivere."));

    const QVariantMap validStroke = validatedStroke(stroke);
    if (validStroke.isEmpty())
        return fail(tr("Il tratto della penna contiene dati non validi."));

    return commitInkEdit(InkEdit{{erasableCount()}, {validStroke}, false, {}});
}

bool Notebook::appendCorrectedStroke(const QVariantMap &shape, const QVariantMap &original)
{
    QVariantMap corrected = shape;
    corrected.insert(QStringLiteral("original"), original);
    return appendStroke(corrected);
}

QString Notebook::handwritingLanguage() const
{
    return currentBook() ? currentBook()->handwritingLanguage : QStringLiteral("it");
}

bool Notebook::setHandwritingLanguage(const QString &language)
{
    if (!currentBook() || (language != QStringLiteral("it") && language != QStringLiteral("en")))
        return false;
    if (language == handwritingLanguage())
        return true;
    State next = m_state;
    next.books[next.activeBook].handwritingLanguage = language;
    return commit(std::move(next), DocumentChanged);
}

QVariantMap Notebook::selectInk(const QVariantList &outline) const
{
    return selectContent(outline, QStringLiteral("contained"), {}, QStringLiteral("replace"));
}

bool Notebook::selectionCurrent(const QVariantMap &selection) const
{
    const Page *page = currentPage();
    if (!page || !canWriteLayer() || selection.value(QStringLiteral("bookId")) != bookId()
        || selection.value(QStringLiteral("pageId")) != page->id
        || selection.value(QStringLiteral("layerId")) != activeLayer())
        return false;
    const QVariantList indexes = selection.value(QStringLiteral("indexes")).toList();
    const QVariantList strokes = selection.value(QStringLiteral("strokes")).toList();
    if (indexes.size() != strokes.size() || indexes.size() > 4000)
        return false;
    const QVariantList &ink = activeLayerInk(*page);
    const int offset = erasableFrom();
    int previousIndex = -1;
    for (qsizetype i = 0; i < indexes.size(); ++i) {
        bool ok = false;
        const double value = indexes.at(i).toDouble(&ok) - offset;
        if (!ok || !std::isfinite(value) || value != std::floor(value) || value <= previousIndex || value >= ink.size()
            || ink.at(static_cast<int>(value)) != strokes.at(i))
            return false;
        previousIndex = static_cast<int>(value);
    }
    QSet<QString> seen;
    for (const QVariant &entry : selection.value(QStringLiteral("items")).toList()) {
        const QString id = entry.toMap().value(QStringLiteral("id")).toString();
        const int index = findPageItem(*page, id);
        if (index < 0 || seen.contains(id) || page->items.at(index) != entry)
            return false;
        seen.insert(id);
    }
    return true;
}

QVariantMap Notebook::selectContent(const QVariantList &outline, const QString &mode,
                                    const QVariantMap &previous, const QString &operation) const
{
    const Page *page = currentPage();
    if (!page || !canWriteLayer() || outline.size() < 3 || outline.size() > 12000
        || (mode != QStringLiteral("contained") && mode != QStringLiteral("touched"))
        || (operation != QStringLiteral("replace") && operation != QStringLiteral("add") && operation != QStringLiteral("subtract")))
        return {};
    if (operation != QStringLiteral("replace") && !previous.isEmpty() && !selectionCurrent(previous))
        return {};
    QPolygonF polygon;
    polygon.reserve(outline.size());
    for (const QVariant &entry : outline) {
        const QVariantMap point = entry.toMap();
        double x = 0, y = 0;
        if (!readCoordinate(point.value(QStringLiteral("x")), x) || !readCoordinate(point.value(QStringLiteral("y")), y))
            return {};
        const QPointF position(x, y);
        // Preserve corners and tiny accents. Ignore only consecutive duplicate samples.
        if (polygon.isEmpty() || position != polygon.last())
            polygon.append(position);
    }
    if (polygon.size() < 3 || polygon.boundingRect().isEmpty())
        return {};
    if (polygon.size() > 512) {
        QSet<qsizetype> keep{0, polygon.size() - 1};
        QList<QPair<qsizetype, qsizetype>> pending{{0, polygon.size() - 1}};
        while (!pending.isEmpty()) {
            const auto span = pending.takeLast();
            const QPointF a = polygon.at(span.first), delta = polygon.at(span.second) - a;
            const qreal length2 = QPointF::dotProduct(delta, delta);
            qreal distance = .0002;
            qsizetype furthest = -1;
            for (qsizetype i = span.first + 1; i < span.second; ++i) {
                const QPointF point = polygon.at(i);
                const qreal t = length2 > 0 ? qBound(0.0, QPointF::dotProduct(point - a, delta) / length2, 1.0) : 0;
                const qreal value = QLineF(point, a + t * delta).length();
                if (value > distance) { distance = value; furthest = i; }
            }
            if (furthest >= 0) {
                keep.insert(furthest);
                pending.append({span.first, furthest}); pending.append({furthest, span.second});
            }
        }
        if (keep.size() > 1024)
            return {{QStringLiteral("error"), tr("Il contorno è troppo complesso. Disegna un lazo più semplice.")}};
        QPolygonF simplified;
        for (qsizetype i = 0; i < polygon.size(); ++i)
            if (keep.contains(i)) simplified.append(polygon.at(i));
        polygon = std::move(simplified);
    }
    const QRectF outlineBounds = polygon.boundingRect();
    const bool contained = mode == QStringLiteral("contained");
    const bool subtract = operation == QStringLiteral("subtract");
    QSet<int> included;
    QSet<QString> itemIds;
    if (operation != QStringLiteral("replace")) {
        for (const QVariant &index : previous.value(QStringLiteral("indexes")).toList())
            included.insert(index.toInt());
        for (const QVariant &entry : previous.value(QStringLiteral("items")).toList())
            itemIds.insert(entry.toMap().value(QStringLiteral("id")).toString());
    }
    const QVariantList &ink = activeLayerInk(*page);
    const int offset = erasableFrom();
    QVariantList selected, indexes, items;
    int reversible = 0;
    for (int index = 0; index < ink.size(); ++index) {
        const QVariantMap stroke = ink.at(index).toMap();
        const auto tool = inkToolFromName(stroke.value(QStringLiteral("tool")).toString());
        const auto anchors = storedInkPoints(stroke);
        const auto samples = isInkShape(tool) ? shapeOutline(anchors, tool) : anchors;
        QList<QPointF> points;
        for (const auto &sample : samples)
            points.append(sample.position);
        const QRectF strokeBounds = QPolygonF(points).boundingRect().adjusted(-1e-9, -1e-9, 1e-9, 1e-9);
        if (outlineBounds.intersects(strokeBounds) && InkSelectionGeometry::intersectsStroke(polygon, points, contained)) {
            if (subtract) included.remove(index + offset); else included.insert(index + offset);
        }
        if (included.contains(index + offset)) {
            indexes.append(index + offset);
            selected.append(stroke);
            reversible += stroke.contains(QStringLiteral("original"));
        }
    }
    for (const QVariant &entry : page->items) {
        const QVariantMap item = entry.toMap();
        const QString id = item.value(QStringLiteral("id")).toString();
        const QPolygonF corners = InkSelectionGeometry::itemOutline(item);
        const bool hit = InkSelectionGeometry::intersectsStroke(polygon, corners, contained)
            || (!contained && corners.containsPoint(polygon.first(), Qt::OddEvenFill));
        if (hit) {
            if (subtract) itemIds.remove(id); else itemIds.insert(id);
        }
        if (itemIds.contains(id))
            items.append(item);
    }
    return {{QStringLiteral("indexes"), indexes}, {QStringLiteral("strokes"), selected},
            {QStringLiteral("items"), items}, {QStringLiteral("bounds"), InkSelectionGeometry::boundsOf(selected, items)},
            {QStringLiteral("reversible"), reversible}, {QStringLiteral("bookId"), bookId()},
            {QStringLiteral("pageId"), page->id}, {QStringLiteral("layerId"), activeLayer()}};
}

QVariantMap Notebook::previewSelection(const QVariantMap &selection, qreal dx, qreal dy, qreal scale, qreal angle) const
{
    if (!selectionCurrent(selection))
        return {};
    const QVariantList strokes = selection.value(QStringLiteral("strokes")).toList();
    const QVariantList items = selection.value(QStringLiteral("items")).toList();
    if (strokes.isEmpty() && items.isEmpty())
        return {};
    const QPointF pivot = InkSelectionGeometry::boundsOf(strokes, items).center();
    const auto movedInk = InkSelectionGeometry::transformed(strokes, pivot, {dx, dy}, scale, angle);
    const auto movedItems = InkSelectionGeometry::transformedItems(items, pivot, {dx, dy}, scale, angle);
    if (movedInk.size() != strokes.size() || movedItems.size() != items.size())
        return {};
    return {{QStringLiteral("strokes"), movedInk}, {QStringLiteral("items"), movedItems},
            {QStringLiteral("bounds"), InkSelectionGeometry::boundsOf(movedInk, movedItems)}};
}

bool Notebook::transformSelection(const QVariantMap &selection, qreal dx, qreal dy, qreal scale, qreal angle)
{
    const QVariantMap preview = previewSelection(selection, dx, dy, scale, angle);
    if (preview.isEmpty())
        return fail(tr("La selezione è cambiata o la trasformazione supera i bordi della pagina."));
    InkEdit edit;
    for (const QVariant &index : selection.value(QStringLiteral("indexes")).toList())
        edit.indexes.append(index.toInt() - erasableFrom());
    edit.previous = selection.value(QStringLiteral("strokes")).toList();
    edit.strokes = preview.value(QStringLiteral("strokes")).toList();
    edit.itemsBefore = selection.value(QStringLiteral("items")).toList();
    edit.itemsAfter = preview.value(QStringLiteral("items")).toList();
    return commitInkEdit(std::move(edit));
}

bool Notebook::removeSelection(const QVariantMap &selection)
{
    if (!selectionCurrent(selection))
        return fail(tr("La selezione non è più valida."));
    InkEdit edit;
    edit.removes = true;
    for (const QVariant &index : selection.value(QStringLiteral("indexes")).toList())
        edit.indexes.append(index.toInt() - erasableFrom());
    edit.strokes = selection.value(QStringLiteral("strokes")).toList();
    edit.itemsBefore = selection.value(QStringLiteral("items")).toList();
    return (!edit.strokes.isEmpty() || !edit.itemsBefore.isEmpty()) && commitInkEdit(std::move(edit));
}

bool Notebook::restoreSelectedInk(const QVariantList &indexes)
{
    const Page *page = currentPage();
    if (!page || !canWriteLayer())
        return false;
    const int offset = erasableFrom();
    const QVariantList &ink = activeLayerInk(*page);
    InkEdit edit;
    QSet<int> seen;
    for (const QVariant &value : indexes) {
        bool ok = false;
        const double index = value.toDouble(&ok);
        if (!ok || !std::isfinite(index) || std::floor(index) != index || index < offset || index >= offset + ink.size())
            return fail(tr("La selezione dei tratti non è più valida."));
        const int local = static_cast<int>(index) - offset;
        if (seen.contains(local))
            continue;
        seen.insert(local);
        const QVariantMap stroke = ink.at(local).toMap();
        const QVariantMap original = stroke.value(QStringLiteral("original")).toMap();
        if (!original.isEmpty()) {
            edit.indexes.append(local);
            edit.previous.append(stroke);
            edit.strokes.append(original);
        }
    }
    return !edit.indexes.isEmpty() && commitInkEdit(std::move(edit));
}

bool Notebook::eraseStrokes(const QVariantList &indexes)
{
    const Page *page = currentPage();
    if (!page || !canWriteLayer() || indexes.isEmpty())
        return false;

    const int offset = erasableFrom();
    const QVariantList &ink = activeLayerInk(*page);
    QSet<int> distinct;
    for (const QVariant &entry : indexes) {
        bool ok = false;
        const double value = entry.toDouble(&ok);
        if (!ok || !std::isfinite(value) || std::floor(value) != value || value < offset || value >= offset + ink.size())
            return fail(tr("La selezione dei tratti non è valida."));
        distinct.insert(static_cast<int>(value) - offset);
    }

    InkEdit edit;
    edit.removes = true;
    edit.indexes = distinct.values();
    std::sort(edit.indexes.begin(), edit.indexes.end());
    for (const int index : std::as_const(edit.indexes))
        edit.strokes.append(ink.at(index));
    return commitInkEdit(std::move(edit));
}

bool Notebook::transformSelectedInk(const QVariantList &indexes, qreal pivotX, qreal pivotY,
                                    qreal dx, qreal dy, qreal scale, qreal angle)
{
    if (!canWriteLayer() || indexes.isEmpty() || indexes.size() > 4000)
        return fail(tr("Seleziona i tratti da trasformare nel livello attivo."));

    const int offset = erasableFrom();
    const QVariantList &ink = activeLayerInk(*currentPage());
    InkEdit edit;
    QSet<int> seen;
    for (const QVariant &value : indexes) {
        bool ok = false;
        const double index = value.toDouble(&ok);
        if (!ok || !std::isfinite(index) || std::floor(index) != index || index < offset || index >= offset + ink.size())
            return fail(tr("La selezione dei tratti non è più valida."));
        const int local = static_cast<int>(index) - offset;
        if (seen.contains(local))
            continue;
        seen.insert(local);
        edit.indexes.append(local);
        edit.previous.append(ink.at(local));
    }
    edit.strokes = InkSelectionGeometry::transformed(edit.previous, {pivotX, pivotY}, {dx, dy}, scale, angle);
    if (edit.strokes.size() != edit.previous.size())
        return fail(tr("La trasformazione supera i bordi della pagina o non è valida."));
    return edit.strokes == edit.previous || commitInkEdit(std::move(edit));
}

bool Notebook::pasteInk(const QVariantList &strokes, qreal pivotX, qreal pivotY,
                        qreal dx, qreal dy, qreal scale, qreal angle)
{
    if (!canWriteLayer() || strokes.isEmpty() || strokes.size() > 4000)
        return fail(tr("Mostra il livello attivo prima di incollare i tratti."));
    const auto transformed = InkSelectionGeometry::transformed(strokes, {pivotX, pivotY}, {dx, dy}, scale, angle);
    if (transformed.size() != strokes.size())
        return fail(tr("La selezione da incollare supera i bordi della pagina o non è valida."));

    InkEdit edit;
    const int first = erasableCount();
    for (const QVariant &stroke : transformed) {
        const QVariantMap valid = validatedStroke(stroke.toMap());
        if (valid.isEmpty())
            return fail(tr("La selezione contiene tratti non validi."));
        edit.indexes.append(first + static_cast<int>(edit.strokes.size()));
        edit.strokes.append(valid);
    }
    return commitInkEdit(std::move(edit));
}

bool Notebook::undo()
{
    return applyInkHistory(true);
}

bool Notebook::redo()
{
    return applyInkHistory(false);
}

bool Notebook::applyInkEdit(Page &page, InkEdit &edit, bool reverse)
{
    const QVariantList &beforeItems = reverse ? edit.itemsAfter : edit.itemsBefore;
    const QVariantList &afterItems = reverse ? edit.itemsBefore : edit.itemsAfter;
    for (const QVariant &entry : beforeItems) {
        const int index = findPageItem(page, entry.toMap().value(QStringLiteral("id")).toString());
        if (index < 0 || page.items.at(index) != entry)
            return fail(tr("L’elemento è cambiato. Rinnova la selezione prima di modificarlo."));
    }
    for (const QVariant &entry : beforeItems) {
        const QString id = entry.toMap().value(QStringLiteral("id")).toString();
        const auto replacement = std::find_if(afterItems.cbegin(), afterItems.cend(), [&id](const QVariant &candidate) {
            return candidate.toMap().value(QStringLiteral("id")).toString() == id;
        });
        const int index = findPageItem(page, id);
        if (replacement == afterItems.cend())
            page.items.removeAt(index);
        else
            page.items[index] = *replacement;
    }
    for (const QVariant &entry : afterItems) {
        if (findPageItem(page, entry.toMap().value(QStringLiteral("id")).toString()) < 0)
            page.items.append(entry);
    }
    if (page.items.size() > maxItemsPerPage)
        return fail(tr("La pagina ha raggiunto il limite di 120 elementi."));
    if (!reverse && !page.markdownBlockId.isEmpty() && findPageItem(page, page.markdownBlockId) < 0) {
        edit.removedMarkdownBlock = page.markdownBlockId;
        page.markdownBlockId.clear();
    } else if (reverse && !edit.removedMarkdownBlock.isEmpty() && findPageItem(page, edit.removedMarkdownBlock) >= 0) {
        page.markdownBlockId = edit.removedMarkdownBlock;
    }
    if (!edit.textItem.isEmpty()) {
        if (reverse) {
            const int index = findPageItem(page, edit.textItem.value(QStringLiteral("id")).toString());
            if (index >= 0) {
                // Preserve any changes made to the text or its position before Undo.
                edit.textItem = page.items.takeAt(index).toMap();
                edit.itemIndex = index;
            }
        } else {
            if (page.items.size() >= maxItemsPerPage)
                return fail(tr("La pagina ha raggiunto il limite di 120 elementi."));
            const qsizetype index = edit.itemIndex < 0 ? page.items.size() : qMin(edit.itemIndex, page.items.size());
            page.items.insert(index, edit.textItem);
        }
    }

    QVariantList &strokes = inkForEdit(page);
    if (!edit.previous.isEmpty()) {
        const QVariantList &replacement = reverse ? edit.previous : edit.strokes;
        for (qsizetype pos = 0; pos < edit.indexes.size(); ++pos)
            strokes[edit.indexes.at(pos)] = replacement.at(pos);
    } else if (edit.removes != reverse) {
        for (auto index = edit.indexes.crbegin(); index != edit.indexes.crend(); ++index)
            strokes.removeAt(*index);
    } else {
        for (qsizetype pos = 0; pos < edit.indexes.size(); ++pos)
            strokes.insert(edit.indexes.at(pos), edit.strokes.at(pos));
    }
    return true;
}

bool Notebook::commitInkEdit(InkEdit edit)
{
    State next = m_state;
    if (!applyInkEdit(currentPage(next), edit, false) || !commit(std::move(next), 0))
        return false;

    const bool changesItems = !edit.textItem.isEmpty() || !edit.itemsBefore.isEmpty() || !edit.itemsAfter.isEmpty();
    if (m_undo.size() == maxInkHistory)
        m_undo.removeFirst();
    m_undo.append(std::move(edit));
    m_redo.clear();
    emit strokesChanged();
    if (changesItems) {
        emit pageContentChanged();
        emit libraryItemsChanged();
        emit booksChanged();
    }
    return true;
}

bool Notebook::applyInkHistory(bool reverse)
{
    const Page *page = currentPage();
    auto &source = reverse ? m_undo : m_redo;
    auto &destination = reverse ? m_redo : m_undo;
    const bool stored = !source.isEmpty();
    if (!page || !canWriteLayer())
        return false;
    const QVariantList &ink = activeLayerInk(*page);
    if (!stored && (!reverse || ink.isEmpty()))
        return false;

    InkEdit edit = stored ? source.last()
        : InkEdit{{static_cast<int>(ink.size()) - 1}, {ink.last()}, false, {}};

    State next = m_state;
    if (!applyInkEdit(currentPage(next), edit, reverse) || !commit(std::move(next), 0))
        return false;

    if (stored)
        source.removeLast();
    if (destination.size() == maxInkHistory)
        destination.removeFirst();
    const bool changesItems = !edit.textItem.isEmpty() || !edit.itemsBefore.isEmpty() || !edit.itemsAfter.isEmpty();
    destination.append(std::move(edit));
    emit strokesChanged();
    if (changesItems) {
        emit pageContentChanged();
        emit libraryItemsChanged();
        emit booksChanged();
    }
    return true;
}

bool Notebook::commitLayers(State next)
{
    return commit(std::move(next), LayersChanged | StrokesChanged | ResetInkHistory);
}

bool Notebook::addLayer(const QString &name)
{
    const Page *page = currentPage();
    const QString title = name.trimmed();
    if (!page || title.isEmpty() || title.size() > 50)
        return fail(tr("Inserisci un nome di livello tra 1 e 50 caratteri."));
    if (page->layers.size() >= maxLayers)
        return fail(tr("La pagina ha già 12 livelli."));

    State next = m_state;
    Page &edited = currentPage(next);
    materializeLayers(edited);
    edited.activeLayer = newId();
    edited.layers.append(InkLayer{edited.activeLayer, title, true, {}});
    return commitLayers(std::move(next));
}

bool Notebook::setActiveLayer(const QString &id)
{
    const Page *page = currentPage();
    if (!page || (page->layers.isEmpty() ? id != defaultLayerId : findLayer(*page, id) < 0))
        return fail(tr("Impossibile trovare il livello."));
    if (activeLayer() == id) {
        setError({});
        return true;
    }

    State next = m_state;
    currentPage(next).activeLayer = id;
    return commitLayers(std::move(next));
}

bool Notebook::renameLayer(const QString &id, const QString &name)
{
    if (!currentPage())
        return fail(tr("Apri una pagina per modificare i livelli."));
    const QString title = name.trimmed();
    if (title.isEmpty() || title.size() > 50)
        return fail(tr("Inserisci un nome di livello tra 1 e 50 caratteri."));

    State next = m_state;
    Page &edited = currentPage(next);
    materializeLayers(edited);
    const int index = findLayer(edited, id);
    if (index < 0)
        return fail(tr("Impossibile trovare il livello."));
    edited.layers[index].name = title;
    return commit(std::move(next), LayersChanged);
}

bool Notebook::setLayerVisible(const QString &id, bool visible)
{
    if (!currentPage())
        return fail(tr("Apri una pagina per modificare i livelli."));

    State next = m_state;
    Page &edited = currentPage(next);
    materializeLayers(edited);
    const int index = findLayer(edited, id);
    if (index < 0)
        return fail(tr("Impossibile trovare il livello."));
    edited.layers[index].visible = visible;
    return commitLayers(std::move(next));
}

bool Notebook::deleteLayer(const QString &id)
{
    const Page *page = currentPage();
    if (!page || page->layers.size() <= 1)
        return fail(tr("La pagina deve conservare almeno un livello."));
    const int index = findLayer(*page, id);
    if (index < 0)
        return fail(tr("Impossibile trovare il livello."));

    State next = m_state;
    Page &edited = currentPage(next);
    edited.layers.removeAt(index);
    if (edited.activeLayer == id)
        edited.activeLayer = edited.layers.at(qMax(0, index - 1)).id;
    return commitLayers(std::move(next));
}

bool Notebook::moveLayer(const QString &id, int toIndex)
{
    const Page *page = currentPage();
    const int index = page ? findLayer(*page, id) : -1;
    if (index < 0 || toIndex < 0 || toIndex >= page->layers.size())
        return fail(tr("Impossibile spostare il livello."));

    State next = m_state;
    currentPage(next).layers.move(index, toIndex);
    return commitLayers(std::move(next));
}

bool Notebook::mergeLayerDown(const QString &id)
{
    const Page *page = currentPage();
    const int index = page ? findLayer(*page, id) : -1;
    if (index <= 0)
        return fail(tr("Seleziona un livello con un altro livello sotto."));
    if (page->layers.at(index - 1).visible != page->layers.at(index).visible)
        return fail(tr("Imposta la stessa visibilità per entrambi i livelli prima di unirli."));

    State next = m_state;
    Page &edited = currentPage(next);
    edited.layers[index - 1].strokes += edited.layers.at(index).strokes;
    if (edited.activeLayer == id)
        edited.activeLayer = edited.layers.at(index - 1).id;
    edited.layers.removeAt(index);
    return commitLayers(std::move(next));
}

bool Notebook::setPageTemplate(const QString &paper)
{
    const Page *page = currentPage();
    if (!page || !PageContent::isTemplate(paper))
        return fail(tr("Seleziona un modello di pagina valido."));
    if (page->hasBackground())
        return fail(tr("Questa pagina usa lo sfondo del documento importato."));
    if (page->paper == paper) {
        setError({});
        return true;
    }
    State next = m_state;
    currentPage(next).paper = paper;
    return commit(std::move(next), PageContentChanged);
}

bool Notebook::setPageTags(const QVariantList &input)
{
    if (!currentPage())
        return fail(tr("Apri una pagina per modificare i tag."));
    QStringList tags;
    if (!normalizedTags(input, tags))
        return fail(tr("Usa al massimo 20 tag, ciascuno tra 1 e 40 caratteri."));
    State next = m_state;
    currentPage(next).tags = tags;
    return commit(std::move(next), BooksChanged | PageContentChanged);
}

bool Notebook::savePageItem(const QVariantMap &item)
{
    const Page *page = currentPage();
    if (!page)
        return fail(tr("Apri una pagina per aggiungere un elemento."));

    const QString id = item.value(QStringLiteral("id")).toString();
    const int index = id.isEmpty() ? -1 : findPageItem(*page, id);
    if (!id.isEmpty() && index < 0)
        return fail(tr("L’elemento non è più presente sulla pagina."));
    if (index < 0 && page->items.size() >= maxItemsPerPage)
        return fail(tr("La pagina ha raggiunto il limite di 120 elementi."));

    const auto typeKey = QStringLiteral("type");
    const bool diagram = item.value(typeKey) == QStringLiteral("diagram");
    const bool image = item.value(typeKey) == QStringLiteral("image");
    const QVariant oldType = index >= 0 ? page->items.at(index).toMap().value(typeKey) : QVariant{};
    if (image && oldType != QStringLiteral("image") && imageCount(page->items) >= PageContent::maxImagesPerPage)
        return fail(tr("La pagina ha raggiunto il limite di %1 immagini.").arg(PageContent::maxImagesPerPage));
    QVariantMap valid;
    if (index < 0) {
        valid = newPlacedItem(item, page->items);
    } else if (image) {
        valid = PageContent::validatedItem(item);
    } else {
        QRectF rect = itemRect(page->items.at(index).toMap());
        if (!diagram) {
            const qreal height = PageContent::preferredItemHeight(item, rect.width());
            rect.setHeight(qMax(rect.height(), height));
        }
        if (rect.bottom() > 1)
            rect.moveBottom(1);
        valid = itemAt(item, rect);
    }
    if (index >= 0 && !valid.isEmpty()) {
        const QVariantMap previous = page->items.at(index).toMap();
        for (const QString &key : {QStringLiteral("handwritingSource"), QStringLiteral("rotation"), QStringLiteral("textScale"), QStringLiteral("styleScale")}) {
            if (previous.contains(key) && (!item.contains(key) || key == QStringLiteral("handwritingSource")))
                valid.insert(key, previous.value(key));
        }
        valid = PageContent::validatedItem(valid);
    }
    if (valid.isEmpty())
        return fail(diagram
            ? tr("Controlla le forme, le etichette e i collegamenti del diagramma.")
            : tr("Controlla il testo e le dimensioni dell’elemento."));

    State next = m_state;
    Page &updated = currentPage(next);
    auto &items = updated.items;
    if (index < 0)
        items.append(valid);
    else {
        items[index] = valid;
        if (updated.markdownBlockId == id && valid.value(typeKey) != QStringLiteral("text"))
            updated.markdownBlockId.clear();
    }
    int changes = PageContentChanged;
    const QVariant newType = valid.value(typeKey);
    if (isLibraryItem(newType) || isLibraryItem(oldType)) {
        changes |= LibraryItemsChanged;
        if (oldType != newType)
            changes |= BooksChanged;
    }
    if (updated.markdownBlockId != page->markdownBlockId)
        changes |= BooksChanged;
    return commit(std::move(next), changes, image);
}

bool Notebook::savePageNote(const QVariantMap &input, const QString &expectedBookId,
                            const QString &expectedPageId, const QVariantMap &expectedOriginal)
{
    const Page *page = currentPage();
    if (!page || expectedBookId != bookId() || expectedPageId != page->id)
        return fail(tr("La pagina della nota è cambiata. Riapri la nota prima di salvarla."));
    if (!canWriteLayer())
        return fail(tr("Mostra il livello attivo prima di modificare le note."));
    const QVariant idValue = input.value(QStringLiteral("id"), QString{});
    if (idValue.typeId() != QMetaType::QString)
        return fail(tr("La nota contiene dati non validi."));
    const QString id = idValue.toString();
    const int index = id.isEmpty() ? -1 : findPageItem(*page, id);
    const QVariantMap before = index < 0 ? QVariantMap{} : page->items.at(index).toMap();
    const auto canonical = [](const QVariantMap &value) { return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact); };
    if ((!id.isEmpty() && (index < 0 || before.value(QStringLiteral("type")) != QStringLiteral("note")
            || canonical(before) != canonical(expectedOriginal))) || (id.isEmpty() && !expectedOriginal.isEmpty()))
        return fail(tr("La nota è cambiata. Riaprila prima di salvarla."));
    if (index < 0 && page->items.size() >= maxItemsPerPage)
        return fail(tr("La pagina ha raggiunto il limite di 120 elementi."));
    QVariantMap item = input;
    item.insert(QStringLiteral("id"), id.isEmpty() ? newId() : id);
    item.insert(QStringLiteral("type"), QStringLiteral("note"));
    if (before.isEmpty()) {
        if (!item.contains(QStringLiteral("width"))) item.insert(QStringLiteral("width"), .05);
        if (!item.contains(QStringLiteral("height"))) item.insert(QStringLiteral("height"), .0375);
    } else {
        // Editing a note never silently moves its marker. Moving remains a
        // separate page action, with the note's ink in its own local canvas.
        for (const QString &key : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"),
                QStringLiteral("height"), QStringLiteral("rotation"), QStringLiteral("textScale")}) {
            item.remove(key);
            if (before.contains(key)) item.insert(key, before.value(key));
        }
    }
    item = PageContent::validatedItem(item);
    if (item.isEmpty())
        return fail(tr("Aggiungi testo o scrittura alla nota e controlla che sia nei limiti della pagina."));
    if (item == before) { setError({}); return true; }
    InkEdit edit;
    if (!before.isEmpty()) edit.itemsBefore.append(before);
    edit.itemsAfter.append(item);
    return commitInkEdit(std::move(edit));
}

bool Notebook::removePageNote(const QString &id, const QString &expectedBookId,
                              const QString &expectedPageId, const QVariantMap &expectedOriginal)
{
    const Page *page = currentPage();
    const int index = page ? findPageItem(*page, id) : -1;
    if (!page || expectedBookId != bookId() || expectedPageId != page->id)
        return fail(tr("La pagina della nota è cambiata. Riapri la nota prima di eliminarla."));
    if (!canWriteLayer())
        return fail(tr("Mostra il livello attivo prima di modificare le note."));
    const QVariantMap before = index < 0 ? QVariantMap{} : page->items.at(index).toMap();
    const auto canonical = [](const QVariantMap &value) { return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact); };
    if (index < 0 || before.value(QStringLiteral("type")) != QStringLiteral("note")
        || canonical(before) != canonical(expectedOriginal))
        return fail(tr("La nota è cambiata. Riaprila prima di eliminarla."));
    InkEdit edit;
    edit.itemsBefore.append(before);
    return commitInkEdit(std::move(edit));
}

bool Notebook::savePageLink(const QVariantMap &target, const QPointF &anchor, const QString &expectedBookId,
                           const QString &expectedPageId)
{
    const Page *page = currentPage();
    if (!page || expectedBookId != bookId() || expectedPageId != page->id)
        return fail(tr("La pagina del collegamento è cambiata."));
    if (!canWriteLayer())
        return fail(tr("Mostra il livello attivo prima di modificare i collegamenti."));
    if (page->items.size() >= maxItemsPerPage)
        return fail(tr("La pagina ha raggiunto il limite di 120 elementi."));
    if (!std::isfinite(anchor.x()) || !std::isfinite(anchor.y()) || anchor.x() < 0 || anchor.x() > 1
        || anchor.y() < 0 || anchor.y() > 1)
        return fail(tr("Scegli un punto sulla pagina per il collegamento."));

    const QVariantMap item = pageLinkItem(target, anchor);
    if (item.isEmpty() || !pageLinkTarget(item))
        return fail(tr("La destinazione del collegamento non è disponibile in questa libreria."));

    InkEdit edit;
    edit.itemsAfter.append(item);
    return commitInkEdit(std::move(edit));
}

bool Notebook::removePageLink(const QString &id, const QString &expectedBookId, const QString &expectedPageId)
{
    const Page *page = currentPage();
    if (!page || expectedBookId != bookId() || expectedPageId != page->id)
        return fail(tr("La pagina del collegamento è cambiata."));
    if (!canWriteLayer())
        return fail(tr("Mostra il livello attivo prima di modificare i collegamenti."));
    const int index = findPageItem(*page, id);
    const QVariantMap item = index < 0 ? QVariantMap{} : page->items.at(index).toMap();
    if (item.value(QStringLiteral("type")) != QStringLiteral("link"))
        return fail(tr("Il collegamento non è più presente sulla pagina."));
    InkEdit edit;
    edit.itemsBefore.append(item);
    return commitInkEdit(std::move(edit));
}

QVariantMap Notebook::tableChartPreview(const QVariantMap &options) const
{
    const Page *page = currentPage();
    if (!page)
        return {};
    const int index = findPageItem(*page, options.value(QStringLiteral("sourceTable")).toString());
    if (index < 0)
        return {};
    const QVariantMap table = page->items.at(index).toMap();
    if (table.value(QStringLiteral("type")) != QStringLiteral("table"))
        return {};
    QVariantMap chart = options;
    chart.insert(QStringLiteral("type"), QStringLiteral("chart"));
    return PageContent::chartFromTable(table, chart);
}

bool Notebook::saveTableChart(const QVariantMap &options)
{
    const QVariantMap chart = tableChartPreview(options);
    return chart.isEmpty() ? fail(tr("Scegli due colonne con valori numerici validi.")) : savePageItem(chart);
}

bool Notebook::savePageItemOnNewPage(const QVariantMap &item)
{
    return insertPageWithItem(item, false);
}

bool Notebook::saveTranscript(const QVariantMap &item)
{
    return insertPageWithItem(item, true);
}

QVariantMap Notebook::handwritingPreview(const QVariantMap &selection, const QString &text) const
{
    if (!selectionCurrent(selection) || text.trimmed().isEmpty())
        return {};
    QVariantList handwriting;
    for (const QVariant &entry : selection.value(QStringLiteral("strokes")).toList()) {
        const QString tool = entry.toMap().value(QStringLiteral("tool")).toString();
        if (tool.isEmpty() || tool == QStringLiteral("pen") || tool == QStringLiteral("pencil") || tool == QStringLiteral("marker"))
            handwriting.append(entry);
    }
    if (handwriting.isEmpty())
        return {};
    const QRectF bounds = InkSelectionGeometry::boundsOf(handwriting);
    return itemAt({{QStringLiteral("id"), QStringLiteral("ocr-preview")},
                   {QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}},
                  PageContent::textRectAtInk(text, bounds));
}

bool Notebook::replaceHandwriting(const QVariantMap &selection, const QString &text, bool keepInk)
{
    const Page *page = currentPage();
    const QVariantList indexes = selection.value(QStringLiteral("indexes")).toList();
    const QVariantList selected = selection.value(QStringLiteral("strokes")).toList();
    if (!page || !canWriteLayer() || selection.value(QStringLiteral("bookId")) != bookId()
        || selection.value(QStringLiteral("pageId")) != page->id
        || selection.value(QStringLiteral("layerId")) != activeLayer()
        || indexes.isEmpty() || indexes.size() != selected.size() || indexes.size() > 4000)
        return fail(tr("La selezione dei tratti non è più valida."));
    if (text.trimmed().isEmpty())
        return fail(tr("Inserisci il testo della trascrizione."));

    const int offset = erasableFrom();
    const QVariantList &ink = activeLayerInk(*page);
    InkEdit edit;
    edit.removes = true;
    qreal left = 1;
    qreal top = 1;
    qreal right = 0;
    int previousIndex = -1;
    for (qsizetype position = 0; position < indexes.size(); ++position) {
        bool ok = false;
        const double index = indexes.at(position).toDouble(&ok) - offset;
        if (!ok || !std::isfinite(index) || std::floor(index) != index || index <= previousIndex
            || index >= ink.size() || ink.at(static_cast<int>(index)) != selected.at(position))
            return fail(tr("La selezione dei tratti non è più valida."));
        previousIndex = static_cast<int>(index);
        const QVariantMap stroke = ink.at(previousIndex).toMap();
        const InkTool tool = inkToolFromName(stroke.value(QStringLiteral("tool")).toString());
        if (tool != InkTool::Pen && tool != InkTool::Pencil && tool != InkTool::Marker)
            continue;
        for (const InkPoint &point : storedInkPoints(stroke)) {
            left = qMin(left, point.position.x());
            top = qMin(top, point.position.y());
            right = qMax(right, point.position.x());
        }
        edit.indexes.append(previousIndex);
        edit.strokes.append(stroke);
    }
    if (edit.strokes.isEmpty())
        return fail(tr("Seleziona un tratto di scrittura a mano."));

    const QVariantMap draft{{QStringLiteral("id"), newId()}, {QStringLiteral("type"), QStringLiteral("text")},
                            {QStringLiteral("text"), text}};
    const QRectF bounds(left, top, right - left, 0);
    edit.textItem = itemAt(draft, PageContent::textRectAtInk(text, bounds));
    if (edit.textItem.isEmpty())
        return fail(tr("Controlla il testo e le dimensioni dell’elemento."));
    if (!keepInk)
        edit.textItem.insert(QStringLiteral("handwritingSource"), QVariantMap{
            {QStringLiteral("layerId"), activeLayer()}, {QStringLiteral("strokes"), edit.strokes}});
    edit.textItem = PageContent::validatedItem(edit.textItem);
    if (edit.textItem.isEmpty())
        return fail(tr("La selezione è troppo grande. Seleziona un passaggio più breve."));
    if (keepInk) {
        edit.indexes.clear();
        edit.strokes.clear();
    }
    return commitInkEdit(std::move(edit));
}

QVariantMap Notebook::diagramHandwritingPlan(const QVariantMap &selection, const QString &targetId) const
{
    if (!selectionCurrent(selection) || selection.value(QStringLiteral("strokes")).toList().isEmpty())
        return {{QStringLiteral("error"), tr("La selezione dei tratti non è più valida.")}};
    const QVariantList indexes = selection.value(QStringLiteral("indexes")).toList();
    for (const QVariant &index : indexes) {
        // Do not accept booleans or strings as stroke addresses.
        const auto type = index.metaType().id();
        if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong
            && type != QMetaType::ULongLong && type != QMetaType::Float && type != QMetaType::Double)
            return {{QStringLiteral("error"), tr("La selezione dei tratti non è più valida.")}};
    }
    QVariantList choices;
    QHash<QString, QVariantMap> diagrams;
    for (const QVariant &entry : currentPage()->items) {
        const QVariantMap item = entry.toMap();
        if (item.value(QStringLiteral("type")) != QStringLiteral("diagram")) continue;
        const QString id = item.value(QStringLiteral("id")).toString();
        diagrams.insert(id, item);
        QString title;
        for (const QVariant &node : item.value(QStringLiteral("nodes")).toList()) {
            title = node.toMap().value(QStringLiteral("text")).toString().trimmed();
            if (!title.isEmpty()) break;
        }
        choices.append(QVariantMap{{QStringLiteral("id"), id},
            {QStringLiteral("title"), title.isEmpty() ? tr("Diagramma %1").arg(choices.size() + 1) : title.left(80)}});
    }
    QString chosen = targetId;
    if (chosen.isEmpty()) {
        QStringList selected;
        for (const QVariant &entry : selection.value(QStringLiteral("items")).toList()) {
            const QString id = entry.toMap().value(QStringLiteral("id")).toString();
            if (diagrams.contains(id)) selected.append(id);
        }
        if (selected.size() == 1) chosen = selected.first();
        else if (diagrams.size() == 1) chosen = diagrams.cbegin().key();
        else if (diagrams.isEmpty()) chosen = QStringLiteral("new");
    }
    QVariantMap plan;
    if (chosen.isEmpty()) {
        plan.insert(QStringLiteral("requiresTarget"), true);
    } else if (chosen != QStringLiteral("new") && !diagrams.contains(chosen)) {
        plan.insert(QStringLiteral("error"), tr("Il diagramma scelto non è più presente sulla pagina."));
    } else {
        const QVariantMap existing = diagrams.value(chosen);
        const QVariantMap recovery = existing.value(QStringLiteral("handwritingSource")).toMap();
        const QString recoveryLayer = recovery.value(QStringLiteral("layerId")).toString();
        const bool recoveryLayerExists = currentPage()->layers.isEmpty()
            ? recoveryLayer == defaultLayerId : findLayer(*currentPage(), recoveryLayer) >= 0;
        if (recovery.contains(QStringLiteral("baseDiagram"))
            && recoveryLayerExists && recoveryLayer != activeLayer()) {
            plan.insert(QStringLiteral("error"), tr("Seleziona il livello delle aggiunte precedenti per aggiornare questo diagramma."));
        } else {
            plan = DiagramRecognition::recognize(selection.value(QStringLiteral("strokes")).toList(), existing);
        }
        plan.insert(QStringLiteral("requiresTarget"), false);
    }
    plan.insert(QStringLiteral("targetItemId"), chosen == QStringLiteral("new") ? QString{} : chosen);
    plan.insert(QStringLiteral("targetChoice"), chosen);
    plan.insert(QStringLiteral("targetChoices"), choices);
    plan.insert(QStringLiteral("bookId"), bookId());
    plan.insert(QStringLiteral("pageId"), pageId());
    plan.insert(QStringLiteral("layerId"), activeLayer());
    // The existing manifest digest is already available without disk I/O. A
    // concurrent edit anywhere in this library conservatively invalidates review.
    plan.insert(QStringLiteral("sourceRevision"), QString::fromLatin1(m_fileRevision.toHex()));
    return plan;
}

bool Notebook::applyDiagramHandwriting(const QVariantMap &selection, const QVariantMap &plan,
                                      const QVariantMap &reviewedDiagram, const QVariantList &acceptedLabelIds,
                                      bool removeInk)
{
    const QVariantMap fresh = diagramHandwritingPlan(selection, plan.value(QStringLiteral("targetChoice")).toString());
    if (fresh.value(QStringLiteral("requiresTarget")).toBool())
        return fail(tr("Scegli il diagramma da aggiornare oppure crea un diagramma separato."));
    if (!fresh.value(QStringLiteral("error")).toString().isEmpty())
        return fail(fresh.value(QStringLiteral("error")).toString());
    // JSON canonicalization treats QML arrays and native QStringLists uniformly,
    // while still rejecting forged membership, omitted fields and stale plans.
    const auto canonical = [](const QVariantMap &value) { return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact); };
    if (canonical(fresh) != canonical(plan))
        return fail(tr("Il disegno è cambiato. Rinnova la selezione prima di convertirlo."));
    if (fresh.contains(QStringLiteral("hasChanges")) && !fresh.value(QStringLiteral("hasChanges")).toBool())
        return fail(tr("Non ci sono nuove forme o nuovi collegamenti da applicare."));
    const QVariantMap draft = fresh.value(QStringLiteral("diagram")).toMap();
    if (draft.isEmpty() || reviewedDiagram.isEmpty())
        return fail(tr("Controlla le forme, le etichette e i collegamenti del diagramma."));

    const auto withoutLabels = [](QVariantMap item) {
        for (const QString &key : {QStringLiteral("nodes"), QStringLiteral("edges")}) {
            QVariantList parts;
            for (const QVariant &entry : item.value(key).toList()) {
                QVariantMap part = entry.toMap();
                part.remove(QStringLiteral("text"));
                parts.append(part);
            }
            item.insert(key, parts);
        }
        return item;
    };
    if (canonical(withoutLabels(draft)) != canonical(withoutLabels(reviewedDiagram)))
        return fail(tr("Le forme o i collegamenti della proposta sono cambiati. Riconosci di nuovo il disegno."));

    QHash<QString, QVariantMap> groups;
    for (const QVariant &entry : fresh.value(QStringLiteral("labelGroups")).toList()) {
        const QVariantMap group = entry.toMap();
        const QString id = group.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || groups.contains(id))
            return fail(tr("Controlla le etichette riconosciute prima di applicare il diagramma."));
        groups.insert(id, group);
    }
    QSet<QString> accepted;
    for (const QVariant &entry : acceptedLabelIds) {
        const QString id = entry.toString();
        if (entry.metaType().id() != QMetaType::QString || !groups.contains(id) || accepted.contains(id))
            return fail(tr("Controlla le etichette riconosciute prima di applicare il diagramma."));
        accepted.insert(id);
    }
    QSet<QString> matchedLabels;
    for (const QString &key : {QStringLiteral("nodes"), QStringLiteral("edges")}) {
        const QVariantList before = draft.value(key).toList(), after = reviewedDiagram.value(key).toList();
        for (qsizetype index = 0; index < after.size(); ++index) {
            const QVariantMap part = after.at(index).toMap();
            const QString id = part.value(QStringLiteral("id")).toString();
            const QVariant value = part.value(QStringLiteral("text"));
            if (value.metaType().id() != QMetaType::QString)
                return fail(tr("Controlla le etichette riconosciute prima di applicare il diagramma."));
            const QString existingKey = key == QStringLiteral("nodes") ? QStringLiteral("existingNodeIds") : QStringLiteral("existingEdgeIds");
            if (fresh.value(existingKey).toStringList().contains(id)
                && value != before.at(index).toMap().value(QStringLiteral("text")))
                return fail(tr("Le etichette del diagramma esistente devono restare invariate durante il riconoscimento."));
            if (!groups.contains(id)) continue; // A manually entered label needs no ink removal.
            const QString expectedKind = key == QStringLiteral("nodes") ? QStringLiteral("node") : QStringLiteral("edge");
            if (groups.value(id).value(QStringLiteral("kind")) != expectedKind)
                return fail(tr("Controlla le etichette riconosciute prima di applicare il diagramma."));
            if (accepted.contains(id)) {
                if (value.toString().trimmed().isEmpty())
                    return fail(tr("Inserisci o correggi il testo delle etichette selezionate."));
                matchedLabels.insert(id);
            } else if (value != before.at(index).toMap().value(QStringLiteral("text"))) {
                return fail(tr("Conferma le etichette da inserire oppure lascia il testo originale."));
            }
        }
    }
    if (matchedLabels != accepted)
        return fail(tr("Controlla le etichette riconosciute prima di applicare il diagramma."));

    QVariantMap item = reviewedDiagram;
    const QString targetId = fresh.value(QStringLiteral("targetItemId")).toString();
    const int targetIndex = targetId.isEmpty() ? -1 : findPageItem(*currentPage(), targetId);
    const QVariantMap previous = targetIndex < 0 ? QVariantMap{} : currentPage()->items.at(targetIndex).toMap();
    item.insert(QStringLiteral("id"), targetId.isEmpty() ? newId() : targetId);
    item = PageContent::validatedItem(item);
    if (item.isEmpty())
        return fail(tr("Controlla le forme, le etichette e i collegamenti del diagramma."));
    InkEdit edit;
    edit.removes = removeInk;
    if (removeInk) {
        QSet<int> selectedOffsets;
        const QVariantList selected = selection.value(QStringLiteral("strokes")).toList();
        const auto collect = [&](const QVariantMap &part) {
            for (const QVariant &entry : part.value(QStringLiteral("indexes")).toList()) {
                bool ok = false;
                const double offset = entry.toDouble(&ok);
                if (!ok || !std::isfinite(offset) || offset != std::floor(offset) || offset < 0 || offset >= selected.size())
                    return false;
                selectedOffsets.insert(static_cast<int>(offset));
            }
            return true;
        };
        for (const QVariant &entry : fresh.value(QStringLiteral("candidates")).toList()) {
            if (!collect(entry.toMap())) return fail(tr("La selezione dei tratti non è più valida."));
        }
        for (const QString &id : accepted) {
            if (!collect(groups.value(id))) return fail(tr("La selezione dei tratti non è più valida."));
        }
        QList<int> offsets = selectedOffsets.values();
        std::sort(offsets.begin(), offsets.end());
        const QVariantList addresses = selection.value(QStringLiteral("indexes")).toList();
        for (const int offset : offsets) {
            edit.indexes.append(addresses.at(offset).toInt() - erasableFrom());
            edit.strokes.append(selected.at(offset));
        }
        if (edit.strokes.isEmpty()) return fail(tr("Seleziona un tratto di scrittura a mano."));
        QVariantMap recovery{{QStringLiteral("layerId"), activeLayer()}, {QStringLiteral("strokes"), edit.strokes}};
        if (!previous.isEmpty()) {
            const QVariantMap oldRecovery = previous.value(QStringLiteral("handwritingSource")).toMap();
            if (oldRecovery.contains(QStringLiteral("baseDiagram"))) {
                QVariantList accumulated = oldRecovery.value(QStringLiteral("strokes")).toList();
                accumulated.append(edit.strokes);
                recovery.insert(QStringLiteral("strokes"), accumulated);
                recovery.insert(QStringLiteral("baseDiagram"), oldRecovery.value(QStringLiteral("baseDiagram")));
            } else {
                recovery.insert(QStringLiteral("baseDiagram"), previous);
            }
        }
        item.insert(QStringLiteral("handwritingSource"), recovery);
        item = PageContent::validatedItem(item);
        if (item.isEmpty()) return fail(tr("La selezione è troppo grande. Seleziona un passaggio più breve."));
    }
    if (!previous.isEmpty()) edit.itemsBefore.append(previous);
    edit.itemsAfter.append(item);
    return commitInkEdit(std::move(edit));
}

QVariantMap Notebook::tableHandwritingPlan(const QVariantMap &selection, const QString &tableId) const
{
    if (!selectionCurrent(selection))
        return {{QStringLiteral("error"), tr("La selezione dei tratti non è più valida.")}};
    const Page *page = currentPage();
    const int itemIndex = findPageItem(*page, tableId);
    if (itemIndex < 0 || page->items.at(itemIndex).toMap().value(QStringLiteral("type")) != QStringLiteral("table"))
        return {{QStringLiteral("error"), tr("Seleziona una tabella presente sulla pagina.")}};

    const QVariantMap table = page->items.at(itemIndex).toMap();
    const int rows = table.value(QStringLiteral("rows")).toInt();
    const int columns = table.value(QStringLiteral("cols")).toInt();
    const QVariantList previous = table.value(QStringLiteral("cells")).toList();
    const qreal left = table.value(QStringLiteral("x")).toDouble() * 600;
    const qreal top = table.value(QStringLiteral("y")).toDouble() * 800;
    const qreal width = table.value(QStringLiteral("width")).toDouble() * 600;
    const qreal height = table.value(QStringLiteral("height")).toDouble() * 800;
    if (rows <= 0 || columns <= 0 || previous.size() != rows * columns
        || !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        return {{QStringLiteral("error"), tr("Controlla il testo e le dimensioni dell’elemento.")}};
    const QPointF center(left + width / 2, top + height / 2);
    QTransform inverse;
    inverse.translate(center.x(), center.y());
    inverse.rotate(-table.value(QStringLiteral("rotation")).toDouble());
    inverse.translate(-center.x(), -center.y());

    QMap<int, QVariantMap> groups;
    const QVariantList selected = selection.value(QStringLiteral("strokes")).toList();
    const QVariantList indexes = selection.value(QStringLiteral("indexes")).toList();
    int unassigned = 0;
    // A stroke belongs to a cell only when every point lies strictly inside it.
    // A line crossing a border, or a dot exactly on it, must not silently choose
    // a destination. All points in a single rectangular cell also contain every
    // straight segment between those points.
    constexpr qreal borderTolerance = 1e-7;
    for (qsizetype position = 0; position < selected.size(); ++position) {
        const QVariantMap stroke = selected.at(position).toMap();
        const QString tool = stroke.value(QStringLiteral("tool")).toString();
        if (!tool.isEmpty() && tool != QStringLiteral("pen") && tool != QStringLiteral("pencil") && tool != QStringLiteral("marker"))
            continue;
        const QList<InkPoint> points = storedInkPoints(stroke);
        int cell = -1;
        bool assigned = !points.isEmpty();
        for (const InkPoint &point : points) {
            const QPointF local = inverse.map(QPointF(point.position.x() * 600, point.position.y() * 800));
            const qreal columnPosition = (local.x() - left) / width * columns;
            const qreal rowPosition = (local.y() - top) / height * rows;
            if (!std::isfinite(columnPosition) || !std::isfinite(rowPosition)
                || columnPosition <= 0 || columnPosition >= columns || rowPosition <= 0 || rowPosition >= rows
                || std::abs(columnPosition - std::round(columnPosition)) <= borderTolerance
                || std::abs(rowPosition - std::round(rowPosition)) <= borderTolerance) {
                assigned = false;
                break;
            }
            const int nextCell = static_cast<int>(std::floor(rowPosition)) * columns
                + static_cast<int>(std::floor(columnPosition));
            if (cell >= 0 && nextCell != cell) {
                assigned = false;
                break;
            }
            cell = nextCell;
        }
        if (!assigned) {
            ++unassigned;
            continue;
        }
        if (!groups.contains(cell)) {
            groups.insert(cell, {{QStringLiteral("row"), cell / columns}, {QStringLiteral("column"), cell % columns},
                                 {QStringLiteral("address"), QString(QChar(static_cast<char16_t>(u'A' + cell % columns))) + QString::number(cell / columns + 1)},
                                 {QStringLiteral("previous"), previous.at(cell).toString()}});
        }
        QVariantMap &group = groups[cell];
        QVariantList groupStrokes = group.value(QStringLiteral("strokes")).toList();
        groupStrokes.append(stroke);
        group.insert(QStringLiteral("strokes"), groupStrokes);
        QVariantList groupIndexes = group.value(QStringLiteral("indexes")).toList();
        groupIndexes.append(indexes.at(position));
        group.insert(QStringLiteral("indexes"), groupIndexes);
    }
    QVariantList cells;
    for (QVariantMap &group : groups) {
        // The recognizer sees upright handwriting even when the table has been
        // rotated. Stored ink and global indexes retain their original geometry.
        const QVariantList strokes = group.value(QStringLiteral("strokes")).toList();
        const QVariantList upright = InkSelectionGeometry::transformed(strokes,
            QPointF(center.x() / 600, center.y() / 800), {}, 1, -table.value(QStringLiteral("rotation")).toDouble());
        if (upright.size() != strokes.size())
            return {{QStringLiteral("error"), tr("La selezione è troppo grande. Seleziona un passaggio più breve.")}};
        group.insert(QStringLiteral("strokes"), upright);
        cells.append(group);
    }
    return {{QStringLiteral("tableId"), tableId}, {QStringLiteral("tableSnapshot"), table},
            {QStringLiteral("rows"), rows}, {QStringLiteral("cols"), columns},
            {QStringLiteral("cells"), cells}, {QStringLiteral("unassignedCount"), unassigned}};
}

bool Notebook::applyTableHandwriting(const QVariantMap &selection, const QVariantMap &plan,
                                    const QVariantList &updates, bool removeInk, bool overwriteExisting)
{
    const QVariantMap fresh = tableHandwritingPlan(selection, plan.value(QStringLiteral("tableId")).toString());
    if (fresh.contains(QStringLiteral("error")))
        return fail(fresh.value(QStringLiteral("error")).toString());
    const QVariantMap before = fresh.value(QStringLiteral("tableSnapshot")).toMap();
    if (before != plan.value(QStringLiteral("tableSnapshot")).toMap())
        return fail(tr("La tabella è cambiata. Rinnova la selezione prima di aggiornarla."));
    const int rows = fresh.value(QStringLiteral("rows")).toInt();
    const int columns = fresh.value(QStringLiteral("cols")).toInt();
    if (updates.isEmpty() || updates.size() > rows * columns)
        return fail(tr("Scegli almeno una cella con testo da inserire."));
    QMap<int, QVariantMap> groups;
    for (const QVariant &entry : fresh.value(QStringLiteral("cells")).toList()) {
        const QVariantMap group = entry.toMap();
        groups.insert(group.value(QStringLiteral("row")).toInt() * columns + group.value(QStringLiteral("column")).toInt(), group);
    }
    QVariantMap after = before;
    QVariantList values = before.value(QStringLiteral("cells")).toList();
    QSet<int> seen;
    QMap<int, QVariant> removedStrokes;
    const QVariantList &ink = activeLayerInk(*currentPage());
    const int offset = erasableFrom();
    const auto isNumeric = [](const QVariant &value) {
        switch (value.metaType().id()) {
        case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong: case QMetaType::ULongLong:
        case QMetaType::Double: case QMetaType::Float: return true;
        default: return false;
        }
    };
    for (const QVariant &entry : updates) {
        const QVariantMap update = entry.toMap();
        bool rowValid = false;
        bool columnValid = false;
        const double row = update.value(QStringLiteral("row")).toDouble(&rowValid);
        const double column = update.value(QStringLiteral("column")).toDouble(&columnValid);
        const QVariant textValue = update.value(QStringLiteral("text"));
        if (!isNumeric(update.value(QStringLiteral("row"))) || !isNumeric(update.value(QStringLiteral("column")))
            || !rowValid || !columnValid || !std::isfinite(row) || !std::isfinite(column)
            || row != std::floor(row) || column != std::floor(column)
            || row < 0 || row >= rows || column < 0 || column >= columns
            || textValue.metaType().id() != QMetaType::QString)
            return fail(tr("Controlla le celle e il testo della trascrizione."));
        const int cell = static_cast<int>(row) * columns + static_cast<int>(column);
        const QString text = textValue.toString();
        if (!groups.contains(cell) || seen.contains(cell) || text.trimmed().isEmpty()
            || text.size() > PageContent::maxTableCellLength)
            return fail(tr("Controlla le celle e il testo della trascrizione."));
        seen.insert(cell);
        if (!overwriteExisting && !values.at(cell).toString().isEmpty() && values.at(cell).toString() != text)
            return fail(tr("Conferma la sostituzione dei valori o delle formule già presenti."));
        values[cell] = text;
        if (removeInk) {
            for (const QVariant &globalIndex : groups.value(cell).value(QStringLiteral("indexes")).toList()) {
                const int index = globalIndex.toInt() - offset;
                removedStrokes.insert(index, ink.at(index));
            }
        }
    }
    after.insert(QStringLiteral("cells"), values);
    after = PageContent::validatedItem(after);
    if (after.isEmpty())
        return fail(tr("Controlla le celle e il testo della trascrizione."));
    if (after == before && removedStrokes.isEmpty()) {
        setError({});
        return true;
    }
    InkEdit edit;
    edit.itemsBefore.append(before);
    edit.itemsAfter.append(after);
    edit.removes = removeInk;
    for (auto it = removedStrokes.cbegin(); it != removedStrokes.cend(); ++it) {
        edit.indexes.append(it.key());
        edit.strokes.append(it.value());
    }
    return commitInkEdit(std::move(edit));
}

bool Notebook::restoreOriginalHandwriting(const QString &itemId)
{
    const Page *page = currentPage();
    const int index = page ? findPageItem(*page, itemId) : -1;
    if (index < 0)
        return fail(tr("L’elemento non è più presente sulla pagina."));
    const QVariantMap item = page->items.at(index).toMap();
    const QVariantMap source = item.value(QStringLiteral("handwritingSource")).toMap();
    const QVariantList strokes = source.value(QStringLiteral("strokes")).toList();
    if (strokes.isEmpty())
        return fail(tr("Questo elemento non contiene la scrittura originale."));
    const QString sourceLayer = source.value(QStringLiteral("layerId")).toString();
    const bool sourceLayerExists = page->layers.isEmpty() ? sourceLayer == defaultLayerId : findLayer(*page, sourceLayer) >= 0;
    if (!canWriteLayer() || (sourceLayerExists && sourceLayer != activeLayer()))
        return fail(tr("Seleziona e mostra il livello originale per ripristinare la scrittura."));
    // If that layer was deliberately removed, recover into the current visible
    // layer rather than making the still-preserved handwriting inaccessible.
    InkEdit edit;
    for (const QVariant &stroke : strokes) {
        const QVariantMap valid = validatedStroke(stroke.toMap());
        if (valid.isEmpty())
            return fail(tr("La scrittura originale contiene dati non validi."));
        edit.indexes.append(erasableCount() + edit.strokes.size());
        edit.strokes.append(valid);
    }
    edit.itemsBefore.append(item);
    if (source.contains(QStringLiteral("baseDiagram"))) {
        QVariantMap base = PageContent::validatedItem(source.value(QStringLiteral("baseDiagram")).toMap());
        if (base.isEmpty())
            return fail(tr("Il diagramma originale contiene dati non validi."));
        // Library copies receive a new outer item ID while their recovery
        // snapshot remains immutable. Recover into this copy, never its source.
        base.insert(QStringLiteral("id"), item.value(QStringLiteral("id")));
        edit.itemsAfter.append(base);
    }
    return commitInkEdit(std::move(edit));
}

bool Notebook::insertPageWithItem(const QVariantMap &item, bool transcript)
{
    if (!currentPage() || item.contains(QStringLiteral("id")))
        return fail(tr("Apri un quaderno per inserire un nuovo elemento."));

    const QVariantMap validated = newPlacedItem(item, {});
    if (validated.isEmpty())
        return fail(item.value(QStringLiteral("type")) == QStringLiteral("diagram")
            ? tr("Controlla le forme, le etichette e i collegamenti del diagramma.")
            : tr("Controlla il contenuto dell’elemento."));

    State next = m_state;
    auto &pages = next.books[next.activeBook].pages;
    if (transcript) {
        const QString text = validated.value(QStringLiteral("text")).toString().trimmed();
        if (text.isEmpty())
            return fail(tr("Inserisci il testo della trascrizione."));
        QStringList &entries = pages[next.page].transcripts;
        if (!entries.contains(text)) {
            if (entries.size() >= maxTranscripts)
                return fail(tr("La pagina ha raggiunto il limite di 32 trascrizioni."));
            entries.append(text);
        }
    }
    Page page;
    page.id = newId();
    page.items.append(validated);
    ++next.page;
    pages.insert(next.page, std::move(page));
    const bool image = validated.value(QStringLiteral("type")) == QStringLiteral("image");
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory, image);
}

bool Notebook::movePageItem(const QString &id, qreal x, qreal y)
{
    const Page *page = currentPage();
    const int index = page ? findPageItem(*page, id) : -1;
    if (index < 0 || !std::isfinite(x) || !std::isfinite(y))
        return fail(tr("Impossibile spostare l’elemento."));

    QVariantMap item = page->items.at(index).toMap();
    const QRectF rect = itemRect(item);
    const double left = std::clamp(x, 0.0, 1.0 - rect.width());
    const double top = std::clamp(y, 0.0, 1.0 - rect.height());
    if (qFuzzyCompare(left + 1, rect.x() + 1) && qFuzzyCompare(top + 1, rect.y() + 1))
        return true;

    const auto moved = InkSelectionGeometry::transformedItems({item}, rect.center(), {left - rect.x(), top - rect.y()}, 1, 0);
    if (moved.size() != 1)
        return fail(tr("La trasformazione supera i bordi della pagina o non è valida."));
    item = moved.first().toMap();
    const QVariant type = item.value(QStringLiteral("type"));
    if (type == QStringLiteral("note") || type == QStringLiteral("link")) {
        if (!canWriteLayer())
            return fail(type == QStringLiteral("note") ? tr("Mostra il livello attivo prima di modificare le note.")
                                                       : tr("Mostra il livello attivo prima di modificare i collegamenti."));
        InkEdit edit;
        edit.itemsBefore.append(page->items.at(index));
        edit.itemsAfter.append(item);
        return commitInkEdit(std::move(edit));
    }
    State next = m_state;
    currentPage(next).items[index] = item;
    return commit(std::move(next), PageContentChanged);
}

bool Notebook::removePageItem(const QString &id)
{
    const Page *page = currentPage();
    const int index = page ? findPageItem(*page, id) : -1;
    if (index < 0)
        return fail(tr("L’elemento non è più presente sulla pagina."));
    State next = m_state;
    Page &updated = currentPage(next);
    updated.items.removeAt(index);
    if (updated.markdownBlockId == id)
        updated.markdownBlockId.clear();
    int changes = PageContentChanged;
    if (isLibraryItem(page->items.at(index).toMap().value(QStringLiteral("type")))
        || updated.markdownBlockId != page->markdownBlockId)
        changes |= BooksChanged;
    return commit(std::move(next), changes);
}

const Notebook::Book *Notebook::currentBook() const
{
    return m_state.activeBook >= 0 ? &m_state.books.at(m_state.activeBook) : nullptr;
}

void Notebook::selectActiveBook(State &state)
{
    state.activeBook = -1;
    state.page = 0;
    for (int index = 0; index < state.books.size(); ++index) {
        if (!state.books.at(index).trashed) {
            state.activeBook = index;
            state.page = resumePage(state.books.at(index));
            return;
        }
    }
}

const Notebook::Page *Notebook::currentPage() const
{
    const Book *book = currentBook();
    return book ? &book->pages.at(m_state.page) : nullptr;
}

Notebook::Page &Notebook::currentPage(State &state) const
{
    return state.books[state.activeBook].pages[state.page];
}

int Notebook::findPageItem(const Page &page, const QString &id)
{
    for (int index = 0; index < page.items.size(); ++index) {
        if (page.items.at(index).toMap().value(QStringLiteral("id")).toString() == id)
            return index;
    }
    return -1;
}

int Notebook::findLayer(const Page &page, const QString &id)
{
    for (int index = 0; index < page.layers.size(); ++index) {
        if (page.layers.at(index).id == id)
            return index;
    }
    return -1;
}

void Notebook::materializeLayers(Page &page)
{
    if (!page.layers.isEmpty())
        return;
    page.activeLayer = defaultLayerId;
    page.layers.append(InkLayer{defaultLayerId, tr("Inchiostro"), true, std::move(page.strokes)});
    page.strokes.clear();
}

QVariantList Notebook::visibleStrokes(const Page &page)
{
    if (page.layers.isEmpty())
        return page.strokes;
    QVariantList strokes;
    for (const InkLayer &layer : page.layers) {
        if (layer.visible)
            strokes += layer.strokes;
    }
    return strokes;
}

QVariantList &Notebook::inkForEdit(Page &page)
{
    return page.layers.isEmpty() ? page.strokes : page.layers[findLayer(page, page.activeLayer)].strokes;
}

const QVariantList &Notebook::activeLayerInk(const Page &page)
{
    return page.layers.isEmpty() ? page.strokes : page.layers.at(findLayer(page, page.activeLayer)).strokes;
}

int Notebook::findBook(const State &state, const QString &id)
{
    for (int index = 0; index < state.books.size(); ++index) {
        if (state.books.at(index).id == id)
            return index;
    }
    return -1;
}

int Notebook::findPage(const Book &book, const QString &id)
{
    if (id.isEmpty())
        return -1;
    for (int index = 0; index < book.pages.size(); ++index) {
        if (book.pages.at(index).id == id)
            return index;
    }
    return -1;
}

int Notebook::resumePage(const Book &book)
{
    return qMax(0, findPage(book, book.lastViewedPageId));
}

int Notebook::pageAtRevision(const Book &book, const QString &revision, const QString &pageId, int pageNumber)
{
    if (book.trashed || revision.size() != 64 || !validUuid(pageId))
        return -1;
    const int page = findPage(book, pageId);
    // Peers that imported this book through a legacy app may have different
    // IDs. Only identical ordered content is safe to resume by page number.
    if ((page < 0 && (pageNumber < 1 || pageNumber > book.pages.size()))
        || (page >= 0 && pageNumber != 0 && pageNumber != page + 1)
        || QString::fromLatin1(revisionOf(book).toHex()) != revision)
        return -1;
    return page >= 0 ? page : pageNumber - 1;
}

void Notebook::resetBookIds(Book &book, const QString &newBookId)
{
    const QString previousBookId = book.id;
    const int last = findPage(book, book.lastViewedPageId);
    QHash<QString, QString> pageIds;
    book.id = newBookId;
    for (qsizetype index = 0; index < book.pages.size(); ++index) {
        Page &page = book.pages[index];
        const QString id = legacyPageId(book.id, index);
        pageIds.insert(page.id, id);
        page.id = id;
    }
    if (last >= 0)
        book.lastViewedPageId = book.pages.at(last).id;
    for (Page &page : book.pages) {
        for (QVariant &entry : page.items) {
            auto item = entry.toMap();
            if (item.value(QStringLiteral("type")) != QStringLiteral("link")
                || item.value(QStringLiteral("targetBookId")) != previousBookId)
                continue;
            item.insert(QStringLiteral("targetBookId"), book.id);
            const QString targetPage = item.value(QStringLiteral("targetPageId")).toString();
            item.insert(QStringLiteral("targetPageId"), pageIds.value(targetPage, targetPage));
            entry = item;
        }
    }
}

void Notebook::preserveReadingPosition(Book &incoming, const Book &local)
{
    const int oldIndex = resumePage(local);
    // A legacy ID describes the sender's ordinal rather than a page it
    // has tracked through reorder. Do not use it to redirect a position.
    const int found = incoming.hasStoredPageIds ? findPage(incoming, local.lastViewedPageId) : -1;
    const int index = found >= 0 ? found : qMin(oldIndex, static_cast<int>(incoming.pages.size()) - 1);
    incoming.lastViewedPageId = incoming.pages.at(index).id;
}

void Notebook::preserveMarkdownBlocks(Book &incoming, const Book &local)
{
    if (incoming.workspace.isEmpty()) incoming.workspace = local.workspace;
    QHash<QString, QString> sourceByPage;
    QSet<QString> sourceIds;
    for (const Page &page : local.pages) {
        if (!page.markdownBlockId.isEmpty()) {
            sourceByPage.insert(page.id, page.markdownBlockId);
            sourceIds.insert(page.markdownBlockId);
        }
    }
    if (sourceIds.isEmpty())
        return;

    // Older peers preserve text item IDs but do not carry the source marker.
    for (Page &page : incoming.pages) {
        if (!page.markdownBlockId.isEmpty())
            continue;
        const auto preferred = sourceByPage.value(page.id);
        QString matching;
        for (const QVariant &entry : std::as_const(page.items)) {
            const auto item = entry.toMap();
            if (item.value(QStringLiteral("type")) != QStringLiteral("text"))
                continue;
            const auto id = item.value(QStringLiteral("id")).toString();
            if (id == preferred) {
                matching = id;
                break;
            }
            if (matching.isEmpty() && sourceIds.contains(id))
                matching = id;
        }
        page.markdownBlockId = matching;
    }
}

int Notebook::findFolder(const State &state, const QString &id)
{
    for (int index = 0; index < state.folders.size(); ++index) {
        if (state.folders.at(index).id == id)
            return index;
    }
    return -1;
}

QVariant Notebook::pageStorageValue(const Page &page)
{
    if (!page.hasLayout()) return page.strokes;
    QVariantMap layout{{QStringLiteral("template"), page.paper}, {QStringLiteral("strokes"), page.strokes},
                       {QStringLiteral("items"), page.items}};
    if (!page.encodedBackground.isEmpty()) layout.insert(QStringLiteral("background"), page.encodedBackground);
    if (!page.source.isEmpty()) layout.insert(QStringLiteral("source"), page.source);
    if (!page.tags.isEmpty()) layout.insert(QStringLiteral("tags"), page.tags);
    if (!page.pdfText.isEmpty()) layout.insert(QStringLiteral("pdfText"), page.pdfText);
    if (!page.markdownBlockId.isEmpty()) layout.insert(QStringLiteral("markdownBlockId"), page.markdownBlockId);
    if (!page.transcripts.isEmpty()) layout.insert(QStringLiteral("transcripts"), page.transcripts);
    if (page.bookmarked) layout.insert(QStringLiteral("bookmarked"), true);
    if (!page.layers.isEmpty()) {
        QVariantList layers;
        for (const InkLayer &layer : page.layers)
            layers.append(QVariantMap{{QStringLiteral("id"), layer.id}, {QStringLiteral("name"), layer.name},
                                       {QStringLiteral("visible"), layer.visible}, {QStringLiteral("strokes"), layer.strokes}});
        layout.insert(QStringLiteral("layers"), layers);
        layout.insert(QStringLiteral("activeLayer"), page.activeLayer);
    }
    return layout;
}

QJsonArray Notebook::writePages(const Book &book)
{
    QJsonArray pages;
    for (const Page &page : book.pages)
        pages.append(QJsonValue::fromVariant(pageStorageValue(page)));
    return pages;
}

QByteArray Notebook::revisionOf(const Book &book)
{
    // Page IDs and the local reading position live outside this content
    // digest. Older peers also omit the Markdown marker but preserve text.
    QJsonArray pages = writePages(book);
    for (qsizetype index = 0; index < pages.size(); ++index) {
        const Page &stored = book.pages.at(index);
        if (stored.layers.isEmpty() && stored.markdownBlockId.isEmpty())
            continue;
        QJsonObject page = pages.at(index).toObject();
        page.remove(QStringLiteral("activeLayer"));
        page.remove(QStringLiteral("markdownBlockId"));
        pages.replace(index, page);
    }
    QJsonArray content{book.title, pages};
    if (book.handwritingLanguage != QStringLiteral("it") || !book.sourceDocuments.isEmpty()) {
        content.append(book.handwritingLanguage);
        content.append(QJsonArray::fromStringList(book.sourceDocuments.keys()));
    }
    return fileRevision(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

QString Notebook::load()
{
    const auto stored = PageStore::read(m_filePath);
    if (!stored.error.isEmpty() || stored.payload.isNull())
        return stored.error;

    State loaded;
    const QString error = readState(stored.payload, loaded);
    if (!error.isEmpty())
        return error;
    m_fileRevision = fileRevision(stored.currentBytes);
    m_recoveredStorage = stored.recovered;
    // Builds that saved reading positions separately left this sidecar next to
    // the portable library. Import it once only when it matches that snapshot.
    QFile positionsFile(m_filePath + QStringLiteral(".reading.json"));
    if (!stored.recovered && positionsFile.open(QIODevice::ReadOnly) && positionsFile.size() <= 1024 * 1024) {
        const auto positions = QJsonDocument::fromJson(positionsFile.readAll()).object();
        if (positions.value(QStringLiteral("libraryRevision")).toString().toLatin1() == m_fileRevision.toHex()) {
            const auto pages = positions.value(QStringLiteral("positions")).toObject();
            for (Book &book : loaded.books) {
                const QString id = pages.value(book.id).toString();
                if (findPage(book, id) >= 0) book.lastViewedPageId = id;
            }
            const int active = findBook(loaded, positions.value(QStringLiteral("activeBook")).toString());
            if (active >= 0 && !loaded.books.at(active).trashed) {
                loaded.activeBook = active;
                loaded.page = resumePage(loaded.books.at(active));
            }
        }
    }
    m_state = std::move(loaded);
    return {};
}

QString Notebook::readState(const QByteArray &contents, State &loaded)
{
    const auto invalidFile = tr("Il file dei quaderni contiene dati non validi.");
    const QJsonDocument document = QJsonDocument::fromJson(contents);
    if (!document.isObject())
        return invalidFile;

    const QJsonObject root = document.object();
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version < LegacyFileVersion || version > NotebookFormat::maximumSupportedVersion)
        return tr("La versione del file dei quaderni non è supportata.");
    if (!root.value(QStringLiteral("books")).isArray())
        return invalidFile;

    if (version >= LibraryFileVersion) {
        const QJsonValue folderData = root.value(QStringLiteral("folders"));
        if (!folderData.isArray())
            return invalidFile;
        QHash<QString, QString> parents;
        for (const QJsonValue value : folderData.toArray()) {
            const QJsonObject folder = value.toObject();
            const QString id = folder.value(QStringLiteral("id")).toString();
            const QString title = folder.value(QStringLiteral("title")).toString().trimmed();
            const QJsonValue parent = folder.value(QStringLiteral("parentId"));
            if (id.isEmpty() || title.isEmpty() || title.size() > 80 || !parent.isString() || parents.contains(id))
                return invalidFile;
            parents.insert(id, parent.toString());
            loaded.folders.append(Folder{id, title, parent.toString()});
        }
        QSet<QString> checked;
        for (const Folder &folder : std::as_const(loaded.folders)) {
            QSet<QString> path;
            QString id = folder.id;
            while (!id.isEmpty() && !checked.contains(id)) {
                if (!parents.contains(id) || path.contains(id))
                    return invalidFile;
                path.insert(id);
                id = parents.value(id);
            }
            checked.unite(path);
        }
    }

    QSet<QString> bookIds;
    const QJsonArray books = root.value(QStringLiteral("books")).toArray();
    for (const QJsonValue bookValue : books) {
        const QJsonObject bookObject = bookValue.toObject();
        Book book;
        book.id = bookObject.value(QStringLiteral("id")).toString();
        book.title = bookObject.value(QStringLiteral("title")).toString();
        book.handwritingLanguage = bookObject.value(QStringLiteral("handwritingLanguage")).toString(QStringLiteral("it"));
        const QJsonValue workspace = bookObject.value(QStringLiteral("workspace"));
        if (!workspace.isUndefined() && (!workspace.isString() || workspace.toString() != QStringLiteral("diagrams")))
            return invalidFile;
        book.workspace = workspace.toString();
        if (book.handwritingLanguage != QStringLiteral("it") && book.handwritingLanguage != QStringLiteral("en"))
            return invalidFile;
        const auto sourceData = bookObject.value(QStringLiteral("sourceDocuments"));
        if (!sourceData.isUndefined()) {
            if (version < ExtendedFileVersion || !sourceData.isObject() || sourceData.toObject().size() > 1000)
                return invalidFile;
            const auto sources = sourceData.toObject();
            for (auto it = sources.begin(); it != sources.end(); ++it) {
                const auto entry = it.value().toObject();
                const QString kind = entry.value(QStringLiteral("kind")).toString();
                const QString data = entry.value(QStringLiteral("data")).toString();
                const QByteArray hash = it.key().toLatin1();
                if ((kind != QStringLiteral("pdf") && kind != QStringLiteral("epub"))
                    || data.isEmpty() || data.size() > ((kind == QStringLiteral("epub") ? 64LL : 48LL) * 1024 * 1024 + 2) / 3 * 4 || hash.size() != 64
                    || QByteArray::fromHex(hash).toHex() != hash)
                    return invalidFile;
                const QByteArray decoded = QByteArray::fromBase64(data.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
                if (decoded.isEmpty() || decoded.size() > (kind == QStringLiteral("epub") ? 64LL : 48LL) * 1024 * 1024
                    || QCryptographicHash::hash(decoded, QCryptographicHash::Sha256).toHex() != hash)
                    return invalidFile;
                book.sourceDocuments.insert(it.key(), QVariantMap{{QStringLiteral("kind"), kind}, {QStringLiteral("data"), data}});
            }
        }
        const QByteArray importedRevision = bookObject.value(QStringLiteral("importedRevision")).toString().toLatin1();
        if (importedRevision.size() == 64) {
            const QByteArray decoded = QByteArray::fromHex(importedRevision);
            if (decoded.toHex() == importedRevision)
                book.importedRevision = decoded;
        }
        const QJsonArray pages = bookObject.value(QStringLiteral("pages")).toArray();
        if (book.id.isEmpty() || book.title.trimmed().isEmpty() || pages.isEmpty() || bookIds.contains(book.id))
            return invalidFile;
        const QJsonValue pageIdData = bookObject.value(QStringLiteral("pageIds"));
        const QJsonArray pageIds = pageIdData.toArray();
        book.hasStoredPageIds = !pageIdData.isUndefined();
        const QJsonValue lastViewed = bookObject.value(QStringLiteral("lastViewedPageId"));
        if ((!pageIdData.isUndefined() && (!pageIdData.isArray() || pageIds.size() != pages.size()))
            || (!lastViewed.isUndefined() && !lastViewed.isString()))
            return invalidFile;

        if (version >= LibraryFileVersion) {
            const QJsonValue folder = bookObject.value(QStringLiteral("folderId"));
            const QJsonValue favorite = bookObject.value(QStringLiteral("favorite"));
            const QJsonValue trashed = bookObject.value(QStringLiteral("trashed"));
            const QJsonValue tags = bookObject.value(QStringLiteral("tags"));
            if ((!folder.isUndefined() && !folder.isString()) || (!favorite.isUndefined() && !favorite.isBool())
                || (!trashed.isUndefined() && !trashed.isBool()) || (!tags.isUndefined() && !tags.isArray()))
                return invalidFile;
            book.folderId = folder.toString();
            book.favorite = favorite.toBool();
            book.trashed = trashed.toBool();
            if ((!book.folderId.isEmpty() && findFolder(loaded, book.folderId) < 0)
                || !normalizedTags(tags.toArray().toVariantList(), book.tags))
                return invalidFile;
        }

        QSet<QString> seenPageIds;
        for (const QJsonValue value : pages) {
            Page page;
            if (pageIdData.isUndefined()) {
                page.id = legacyPageId(book.id, book.pages.size());
            } else {
                page.id = pageIds.at(book.pages.size()).toString();
                if (!validUuid(page.id) || seenPageIds.contains(page.id))
                    return invalidFile;
                seenPageIds.insert(page.id);
            }
            QJsonArray strokeArray;
            if (value.isArray()) {
                strokeArray = value.toArray();
            } else if (version >= LayoutFileVersion && value.isObject()) {
                const QJsonObject layout = value.toObject();
                page.paper = layout.value(QStringLiteral("template")).toString();
                const QJsonValue strokes = layout.value(QStringLiteral("strokes"));
                const QJsonValue items = layout.value(QStringLiteral("items"));
                if (!PageContent::isTemplate(page.paper) || !strokes.isArray() || !items.isArray())
                    return invalidFile;
                const QJsonValue background = layout.value(QStringLiteral("background"));
                if (!background.isUndefined()) {
                    const QString encoded = background.toString();
                    if (version < LibraryFileVersion || encoded.isEmpty() || encoded.size() > maxEncodedBackground)
                        return invalidFile;
                    page.encodedBackground = encoded;
                    if (!validBackground(page.backgroundData()))
                        return invalidFile;
                }
                const auto source = layout.value(QStringLiteral("source"));
                if (!source.isUndefined()) {
                    const auto descriptor = source.toObject();
                    const QString asset = descriptor.value(QStringLiteral("asset")).toString();
                    const auto origin = descriptor.value(QStringLiteral("origin"));
                    const int pageNumber = descriptor.value(QStringLiteral("page")).toInt(-1);
                    if (version < ExtendedFileVersion || !source.isObject() || !page.encodedBackground.isEmpty()
                        || descriptor.value(QStringLiteral("kind")) != QStringLiteral("pdf")
                        || pageNumber < 0 || pageNumber >= 100000 || descriptor.value(QStringLiteral("page")).toDouble(-1) != pageNumber
                        || !book.sourceDocuments.contains(asset)
                        || book.sourceDocuments.value(asset).toMap().value(QStringLiteral("kind")) != QStringLiteral("pdf")
                        || (!origin.isUndefined() && (!origin.isString() || !book.sourceDocuments.contains(origin.toString()))))
                        return invalidFile;
                    page.source = {{QStringLiteral("kind"), QStringLiteral("pdf")}, {QStringLiteral("asset"), asset},
                                   {QStringLiteral("page"), pageNumber}};
                    if (!origin.isUndefined()) page.source.insert(QStringLiteral("origin"), origin.toString());
                }
                const QJsonValue layerData = layout.value(QStringLiteral("layers"));
                const QJsonValue indexedText = layout.value(QStringLiteral("pdfText"));
                const QJsonValue markdownBlock = layout.value(QStringLiteral("markdownBlockId"));
                const QJsonValue transcripts = layout.value(QStringLiteral("transcripts"));
                const QJsonValue bookmark = layout.value(QStringLiteral("bookmarked"));
                if ((!indexedText.isUndefined() && (!indexedText.isString() || !page.hasBackground()))
                    || (!markdownBlock.isUndefined() && (!markdownBlock.isString() || markdownBlock.toString().isEmpty()))
                    || (!transcripts.isUndefined() && !transcripts.isArray())
                    || (!bookmark.isUndefined() && !bookmark.isBool()))
                    return invalidFile;
                page.pdfText = indexedText.toString();
                page.markdownBlockId = markdownBlock.toString();
                page.bookmarked = bookmark.toBool();
                const QJsonArray transcriptArray = transcripts.toArray();
                if (page.pdfText.size() > maxPagePdfText || transcriptArray.size() > maxTranscripts)
                    return invalidFile;
                for (const QJsonValue &entry : transcriptArray) {
                    const QString text = entry.toString();
                    if (text.trimmed().isEmpty() || text.size() > 4000)
                        return invalidFile;
                    page.transcripts.append(text);
                }
                const QJsonValue tags = layout.value(QStringLiteral("tags"));
                if (!tags.isUndefined() && (version < LibraryFileVersion || !tags.isArray()
                    || !normalizedTags(tags.toArray().toVariantList(), page.tags)))
                    return invalidFile;
                if (!layerData.isUndefined()) {
                    const QJsonArray layers = layerData.toArray();
                    if (version < LibraryFileVersion || layers.isEmpty() || layers.size() > maxLayers
                        || !strokes.toArray().isEmpty())
                        return invalidFile;
                    QSet<QString> ids;
                    for (const QJsonValue &value : layers) {
                        const QJsonObject object = value.toObject();
                        InkLayer layer;
                        layer.id = object.value(QStringLiteral("id")).toString();
                        layer.name = object.value(QStringLiteral("name")).toString().trimmed();
                        const QJsonValue visible = object.value(QStringLiteral("visible"));
                        const QJsonValue ink = object.value(QStringLiteral("strokes"));
                        if (layer.id.isEmpty() || layer.id.size() > 128 || ids.contains(layer.id)
                            || layer.name.isEmpty() || layer.name.size() > 50 || !visible.isBool() || !ink.isArray()
                            || !readStrokes(ink.toArray(), layer.strokes))
                            return invalidFile;
                        layer.visible = visible.toBool();
                        ids.insert(layer.id);
                        page.layers.append(std::move(layer));
                    }
                    page.activeLayer = layout.value(QStringLiteral("activeLayer")).toString();
                    if (!ids.contains(page.activeLayer))
                        return invalidFile;
                }
                const QJsonArray itemArray = items.toArray();
                if (itemArray.size() > maxItemsPerPage)
                    return invalidFile;

                QSet<QString> itemIds;
                qsizetype images = 0;
                for (const QJsonValue itemValue : itemArray) {
                    const QJsonObject itemObject = itemValue.toObject();
                    if (itemObject.value(QStringLiteral("type")) == QStringLiteral("image")
                        && ++images > PageContent::maxImagesPerPage)
                        return invalidFile;
                    const QVariantMap item = PageContent::validatedItem(itemObject.toVariantMap());
                    const QString id = item.value(QStringLiteral("id")).toString();
                    if (item.isEmpty() || itemIds.contains(id) || version < requiredItemVersion(item))
                        return invalidFile;
                    itemIds.insert(id);
                    page.items.append(item);
                }
                if (!page.markdownBlockId.isEmpty()) {
                    const int index = findPageItem(page, page.markdownBlockId);
                    if (index < 0 || page.items.at(index).toMap().value(QStringLiteral("type")) != QStringLiteral("text"))
                        return invalidFile;
                }
                strokeArray = strokes.toArray();
            } else {
                return invalidFile;
            }

            if (!readStrokes(strokeArray, page.strokes))
                return invalidFile;
            book.pages.append(std::move(page));
        }

        book.lastViewedPageId = lastViewed.toString();
        if (!book.lastViewedPageId.isEmpty() && findPage(book, book.lastViewedPageId) < 0)
            return invalidFile;
        bookIds.insert(book.id);
        loaded.books.append(std::move(book));
    }

    const int index = findBook(loaded, root.value(QStringLiteral("activeBook")).toString());
    if (index >= 0 && !loaded.books.at(index).trashed) {
        loaded.activeBook = index;
        auto &book = loaded.books[index];
        const int page = root.value(QStringLiteral("page")).toInt(-1);
        if (book.lastViewedPageId.isEmpty() && page >= 0 && page < book.pages.size())
            book.lastViewedPageId = book.pages.at(page).id;
        loaded.page = resumePage(book);
    } else {
        selectActiveBook(loaded);
    }
    if (loaded.activeBook >= 0 && loaded.books.at(loaded.activeBook).lastViewedPageId.isEmpty())
        loaded.books[loaded.activeBook].lastViewedPageId = loaded.books.at(loaded.activeBook).pages.at(loaded.page).id;

    return {};
}

int Notebook::dataVersion(const State &state)
{
    int version = state.folders.isEmpty() ? LegacyFileVersion : LibraryFileVersion;
    for (const auto &book : state.books) {
        if (!book.sourceDocuments.isEmpty() || book.handwritingLanguage != QStringLiteral("it"))
            version = qMax(version, ExtendedFileVersion);
        for (const auto &page : book.pages) {
            if (!page.source.isEmpty()) version = qMax(version, ExtendedFileVersion);
            for (const auto &item : page.items)
                version = qMax(version, requiredItemVersion(item.toMap()));
        }
    }
    for (const Book &book : state.books) {
        if (!book.tags.isEmpty() || book.favorite || book.trashed || !book.folderId.isEmpty())
            version = qMax(version, LibraryFileVersion);
        for (const Page &page : book.pages) {
            for (const QVariant &item : page.items) {
                version = qMax(version, requiredItemVersion(item.toMap()));
                if (version == ImageFileVersion)
                    return version;
            }
            if (version >= ReadingFileVersion)
                continue;
            if (!page.pdfText.isEmpty() || !page.transcripts.isEmpty() || page.bookmarked)
                version = ReadingFileVersion;
            if (page.hasLayout())
                version = qMax(version, LayoutFileVersion);
            if (page.hasBackground() || !page.layers.isEmpty() || !page.tags.isEmpty()
                || std::any_of(page.strokes.cbegin(), page.strokes.cend(), [](const QVariant &stroke) {
                    return stroke.toMap().contains(QStringLiteral("tool"));
                }))
                version = qMax(version, LibraryFileVersion);
            if (hasOriginalInk(page.strokes)
                || std::any_of(page.layers.cbegin(), page.layers.cend(), [](const InkLayer &layer) {
                    return hasOriginalInk(layer.strokes);
                })
                || std::any_of(page.items.cbegin(), page.items.cend(), [](const QVariant &entry) {
                    const QVariantMap item = entry.toMap();
                    return item.value(QStringLiteral("type")) == QStringLiteral("chart");
                }))
                version = qMax(version, RecognitionFileVersion);
            if (std::any_of(page.items.cbegin(), page.items.cend(), [](const QVariant &entry) {
                return entry.toMap().contains(QStringLiteral("minimum"));
            }))
                version = ReadingFileVersion;
        }
    }
    return version;
}

QByteArray Notebook::serialize(const State &state)
{
    QJsonArray books;
    for (const Book &book : state.books) {
        QJsonArray pageIds;
        for (const Page &page : book.pages)
            pageIds.append(page.id);
        QJsonObject object{
            {QStringLiteral("id"), book.id},
            {QStringLiteral("title"), book.title},
            {QStringLiteral("pages"), writePages(book)},
            {QStringLiteral("pageIds"), pageIds},
        };
        if (!book.workspace.isEmpty()) object.insert(QStringLiteral("workspace"), book.workspace);
        if (book.handwritingLanguage != QStringLiteral("it"))
            object.insert(QStringLiteral("handwritingLanguage"), book.handwritingLanguage);
        if (!book.sourceDocuments.isEmpty())
            object.insert(QStringLiteral("sourceDocuments"), QJsonObject::fromVariantMap(book.sourceDocuments));
        if (!book.lastViewedPageId.isEmpty())
            object.insert(QStringLiteral("lastViewedPageId"), book.lastViewedPageId);
        if (!book.importedRevision.isEmpty())
            object.insert(QStringLiteral("importedRevision"), QString::fromLatin1(book.importedRevision.toHex()));
        if (!book.folderId.isEmpty())
            object.insert(QStringLiteral("folderId"), book.folderId);
        if (!book.tags.isEmpty())
            object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(book.tags));
        if (book.favorite)
            object.insert(QStringLiteral("favorite"), true);
        if (book.trashed)
            object.insert(QStringLiteral("trashed"), true);
        books.append(object);
    }

    const int version = dataVersion(state);
    QJsonObject root{
        {QStringLiteral("version"), version},
        {QStringLiteral("books"), books},
        {QStringLiteral("activeBook"), state.activeBook >= 0 ? state.books.at(state.activeBook).id : QString{}},
        {QStringLiteral("page"), state.page},
    };
    if (version >= LibraryFileVersion) {
        QJsonArray folders;
        for (const Folder &folder : state.folders) {
            folders.append(QJsonObject{{QStringLiteral("id"), folder.id},
                                       {QStringLiteral("title"), folder.title},
                                       {QStringLiteral("parentId"), folder.parentId}});
        }
        root.insert(QStringLiteral("folders"), folders);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool Notebook::commit(State next, int changes, bool enforceSizeLimit)
{
    const QString previousBookId = bookId();
    if (!m_loadError.isEmpty())
        return fail(m_loadError);
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath()))
        return fail(tr("Impossibile creare la cartella dei quaderni."));

    QLockFile lock(m_filePath + QStringLiteral(".lock"));
    if (!lock.tryLock(0))
        return fail(tr("Impossibile accedere al file dei quaderni per il salvataggio."));

    QByteArray previousContents;
    const QString readError = readFile(m_filePath, previousContents);
    if (!readError.isEmpty())
        return fail(readError);
    if (fileRevision(previousContents) != m_fileRevision)
        return fail(tr("Il file dei quaderni è cambiato. Riapri l’app per caricarlo."));

    if (next.activeBook >= 0)
        next.books[next.activeBook].lastViewedPageId = next.books.at(next.activeBook).pages.at(next.page).id;
    // Reuse untouched page objects and shared immutable assets. Portable JSON
    // is materialized only for import/export and initial library loading.
    PageStore::Builder builder(previousContents, &m_storageBlobs);
    QVariantList books;
    QString storageError;
    int manifestVersion = NotebookFormat::pageLinksVersion;
    for (const Book &book : std::as_const(next.books)) {
        const int previousBookIndex = findBook(m_state, book.id);
        QHash<QString, const Page *> previousPages;
        if (previousBookIndex >= 0)
            for (const Page &old : m_state.books.at(previousBookIndex).pages) previousPages.insert(old.id, &old);
        QVariantList pages;
        QStringList pageIds;
        for (const Page &page : book.pages) {
            if (manifestVersion < NotebookFormat::maximumSupportedVersion) {
                for (const auto &item : page.items) {
                    manifestVersion = qMax(manifestVersion, requiredItemVersion(item.toMap()));
                    if (manifestVersion == NotebookFormat::maximumSupportedVersion) break;
                }
            }
            const Page *old = previousPages.value(page.id, nullptr);
            QString hash;
            if (old && *old == page && old->activeLayer == page.activeLayer && old->markdownBlockId == page.markdownBlockId)
                hash = builder.reusePage(book.id, page.id);
            if (hash.isEmpty()) hash = builder.addPage(pageStorageValue(page), &storageError);
            if (hash.isEmpty()) return fail(storageError);
            pages.append(hash);
            pageIds.append(page.id);
        }
        QVariantMap metadata{{QStringLiteral("id"), book.id}, {QStringLiteral("title"), book.title},
            {QStringLiteral("pages"), pages}, {QStringLiteral("pageIds"), pageIds},
            {QStringLiteral("handwritingLanguage"), book.handwritingLanguage}};
        if (!book.workspace.isEmpty()) metadata.insert(QStringLiteral("workspace"), book.workspace);
        if (!book.sourceDocuments.isEmpty()) metadata.insert(QStringLiteral("sourceDocuments"), book.sourceDocuments);
        if (!book.lastViewedPageId.isEmpty()) metadata.insert(QStringLiteral("lastViewedPageId"), book.lastViewedPageId);
        if (!book.importedRevision.isEmpty()) metadata.insert(QStringLiteral("importedRevision"), QString::fromLatin1(book.importedRevision.toHex()));
        if (!book.folderId.isEmpty()) metadata.insert(QStringLiteral("folderId"), book.folderId);
        if (!book.tags.isEmpty()) metadata.insert(QStringLiteral("tags"), book.tags);
        if (book.favorite) metadata.insert(QStringLiteral("favorite"), true);
        if (book.trashed) metadata.insert(QStringLiteral("trashed"), true);
        books.append(metadata);
    }
    QVariantList folders;
    for (const Folder &folder : std::as_const(next.folders))
        folders.append(QVariantMap{{QStringLiteral("id"), folder.id}, {QStringLiteral("title"), folder.title},
                                   {QStringLiteral("parentId"), folder.parentId}});
    const QVariantMap metadata{{QStringLiteral("version"), manifestVersion},
        {QStringLiteral("books"), books}, {QStringLiteral("folders"), folders},
        {QStringLiteral("activeBook"), next.activeBook >= 0 ? next.books.at(next.activeBook).id : QString{}},
        {QStringLiteral("page"), next.page}};
    PageStore::Update update;
    if (!builder.finish(metadata, &update, &storageError)) return fail(storageError);
    // The local object store has its own bounded budget. The 64 MiB cap applies
    // to explicit portable transfers, not to unrelated pages already saved.
    Q_UNUSED(enforceSizeLimit)
    QByteArray manifest;
    if (!PageStore::writeUpdate(m_filePath, update, previousContents, &manifest, &storageError, m_recoveredStorage))
        return fail(tr("Impossibile salvare il file dei quaderni. %1").arg(storageError));

    m_recoveredStorage = false;
    m_fileRevision = fileRevision(manifest);
    m_state = std::move(next);
    if (previousBookId != bookId()) emit localReadingPreferencesChanged();
    if (changes & ResetInkHistory) {
        m_undo.clear();
        m_redo.clear();
    }
    lock.unlock();

    setError({});
    if (changes & BooksChanged)
        emit booksChanged();
    if (changes & (BooksChanged | LibraryItemsChanged))
        emit libraryItemsChanged();
    if (changes & DocumentChanged)
        emit documentChanged();
    if (changes & StrokesChanged)
        emit strokesChanged();
    if (changes & PageContentChanged)
        emit pageContentChanged();
    if ((changes & LayersChanged) || (changes & DocumentChanged))
        emit layersChanged();
    return true;
}

bool Notebook::fail(const QString &message)
{
    setError(message);
    return false;
}

void Notebook::setError(const QString &message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}
