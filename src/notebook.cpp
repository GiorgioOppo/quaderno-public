#include "notebook.h"

#include "epubimport.h"
#include "inkrender.h"
#include "inkselection.h"
#include "pagecontent.h"
#include "pdfimport.h"

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
#include <QTemporaryDir>
#include <QUrl>
#include <QtMath>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace {
constexpr int LegacyFileVersion = 1;
constexpr int LayoutFileVersion = 2;
constexpr int LibraryFileVersion = 3;
constexpr int RecognitionFileVersion = 4;
constexpr int ReadingFileVersion = 5;
constexpr int DiagramFileVersion = 6;
constexpr int OrthogonalDiagramFileVersion = 7;
constexpr int ImageFileVersion = 8;
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
    return !size.isEmpty() && size.width() <= 1600 && size.height() <= 1600 && !reader.read().isNull();
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

bool isLibraryItem(const QVariant &type)
{
    return type == QStringLiteral("chart") || type == QStringLiteral("table");
}

qsizetype imageCount(const QVariantList &items)
{
    return std::count_if(items.cbegin(), items.cend(), [](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("type")) == QStringLiteral("image");
    });
}

bool mostlyInside(const QPolygonF &polygon, const QRectF &bounds, const QList<InkPoint> &points)
{
    if (points.isEmpty())
        return false;
    if (points.size() == 1)
        return polygon.containsPoint(points.constFirst().position, Qt::OddEvenFill);

    qreal total = 0;
    qreal enclosed = 0;
    for (qsizetype i = 1; i < points.size(); ++i) {
        const QPointF from = points.at(i - 1).position;
        const QPointF to = points.at(i).position;
        const QPointF delta = to - from;
        const qreal length = std::hypot(delta.x(), delta.y() * 4 / 3);
        if (length <= 0)
            continue;
        total += length;
        if (qMax(from.x(), to.x()) < bounds.left() || qMin(from.x(), to.x()) > bounds.right()
            || qMax(from.y(), to.y()) < bounds.top() || qMin(from.y(), to.y()) > bounds.bottom())
            continue;
        const int samples = qBound(1, qCeil(length / 0.008), 192);
        const qreal weight = length / samples;
        for (int j = 0; j < samples; ++j) {
            if (polygon.containsPoint(from + delta * ((j + 0.5) / samples), Qt::OddEvenFill))
                enclosed += weight;
        }
    }
    return total > 0 ? enclosed >= total * 0.72
                     : polygon.containsPoint(points.constFirst().position, Qt::OddEvenFill);
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

int requiredItemVersion(const QVariantMap &item)
{
    const QVariant type = item.value(QStringLiteral("type"));
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
    const double width = type == QStringLiteral("text") ? 0.76 : 0.84;
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
}

Notebook::Notebook(QString filePath, QObject *parent)
    : QObject(parent), m_filePath(QFileInfo(filePath).absoluteFilePath())
{
    m_loadError = load();
    setError(m_loadError);
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
        for (const Page &page : book.pages) {
            document |= !page.background.isEmpty();
            markdown |= !page.markdownBlockId.isEmpty();
            if (!hasCharts || !hasTables) {
                for (const QVariant &entry : page.items) {
                    const QVariant type = entry.toMap().value(QStringLiteral("type"));
                    hasCharts |= type == QStringLiteral("chart");
                    hasTables |= type == QStringLiteral("table");
                    if (hasCharts && hasTables)
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
            {QStringLiteral("markdown"), markdown},
            {QStringLiteral("hasCharts"), hasCharts},
            {QStringLiteral("hasTables"), hasTables},
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
        for (int index = 0; index < book.pages.size(); ++index) {
            const Page &page = book.pages.at(index);
            for (const QVariant &entry : page.items) {
                const QVariantMap item = entry.toMap();
                const QVariant type = item.value(QStringLiteral("type"));
                if (!isLibraryItem(type))
                    continue;
                result.append(QVariantMap{
                    {QStringLiteral("bookId"), book.id},
                    {QStringLiteral("bookTitle"), book.title},
                    {QStringLiteral("pageId"), page.id},
                    {QStringLiteral("pageNumber"), index + 1},
                    {QStringLiteral("itemId"), item.value(QStringLiteral("id"))},
                    {QStringLiteral("type"), type},
                    {QStringLiteral("title"), type == QStringLiteral("chart")
                        ? item.value(QStringLiteral("title")).toString() : tableLibraryTitle(item)},
                    {QStringLiteral("folderId"), book.folderId},
                    {QStringLiteral("favorite"), book.favorite},
                    {QStringLiteral("tags"), book.tags},
                    {QStringLiteral("pageTags"), page.tags},
                });
            }
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
    return page ? page->background : QByteArray{};
}

bool Notebook::hasPageBackground() const
{
    const Page *page = currentPage();
    return page && !page->background.isEmpty();
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
    return page.items.size() != 1 || !page.strokes.isEmpty() || !page.layers.isEmpty() || !page.background.isEmpty()
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
    for (const Page &page : book.pages) {
        for (const QVariant &entry : page.items) {
            const QVariantMap item = entry.toMap();
            if (item.value(QStringLiteral("type")) != QStringLiteral("text"))
                continue;
            const QString fragment = item.value(QStringLiteral("text")).toString();
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
            painter.fillRect(QRect(QPoint(), size), Qt::white);
            const Page &page = book->pages.at(index);
            if (page.background.isEmpty())
                PageContent::paintTemplate(painter, size, page.paper);
            else
                paintPdfBackground(painter, size, QImage::fromData(page.background, "JPEG"));
            PageContent::paintItems(painter, size, PageContent::resolveTableCharts(page.items));
            paintStoredStrokes(painter, size, visibleStrokes(page));
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
    const QByteArray contents = file.read(maxImportBytes + 1);
    if (file.error() != QFileDevice::NoError)
        return fail(tr("Impossibile leggere il file dei quaderni."));
    return importFromData(contents);
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

QVariantMap Notebook::mergeImport(const QByteArray &contents, bool preview, const QString &expectedToken)
{
    const auto problem = [](const QString &message) {
        return QVariantMap{{QStringLiteral("error"), message}};
    };
    if (!m_loadError.isEmpty())
        return problem(m_loadError);
    if (contents.size() > maxImportBytes)
        return problem(tr("Il file dei quaderni supera 64 MB."));

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
            book.id = copyId;
            resetPageIds(book);
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
    if (action == QStringLiteral("unchanged"))
        return reply;

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
            book.id = QUuid::createUuidV5(transferNamespace, book.id.toUtf8() + sourceRevision).toString(QUuid::WithoutBraces);
            const int copyIndex = findBook(next, book.id);
            if (copyIndex >= 0) {
                const Book &existing = next.books.at(copyIndex);
                if (!existing.trashed && existing.pages == book.pages)
                    return reply;
                book.id = newId();
            }
            resetPageIds(book);
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
    return createBookWithFirstPage(title, folderId, std::move(page));
}

bool Notebook::createBookWithFirstPage(const QString &title, const QString &folderId, Page page)
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
    page.id = newId();
    book.pages.append(std::move(page));
    next.books.append(std::move(book));
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
    if (isMarkdownExtension(extension)) {
        QFile input(path);
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
        return importMarkdownText(decoded, info.completeBaseName(), folderId);
    }

    QString error;
    QList<QByteArray> backgrounds;
    QStringList pdfText;
    if (extension == QStringLiteral("pdf")) {
        auto imported = importPdfPages(path, error);
        backgrounds = std::move(imported.pages);
        pdfText = std::move(imported.pageText);
    } else if (extension == QStringLiteral("epub"))
        backgrounds = renderEpubPages(path, error);
    else
        return fail(tr("Il formato deve essere PDF, EPUB o Markdown."));
    if (backgrounds.isEmpty())
        return fail(error.isEmpty() ? tr("Impossibile importare il documento.") : error);

    Book book;
    book.id = newId();
    book.title = info.completeBaseName().left(80).trimmed();
    if (book.title.isEmpty())
        book.title = tr("Documento importato");
    book.folderId = folderId;
    for (qsizetype index = 0; index < backgrounds.size(); ++index) {
        const QByteArray &background = backgrounds.at(index);
        if (background.isEmpty() || background.size() > maxEncodedBackground / 4 * 3)
            return fail(tr("Una pagina importata supera il limite di dimensione."));
        Page page;
        page.id = newId();
        page.background = background;
        page.pdfText = pdfText.value(index).left(maxPagePdfText);
        book.pages.append(std::move(page));
    }

    qint64 addedBytes = 0;
    for (const QByteArray &background : backgrounds)
        addedBytes += ((background.size() + 2) / 3) * 4;
    if (QFileInfo(m_filePath).size() + addedBytes > maxImportBytes)
        return fail(tr("Il documento porterebbe la libreria oltre 64 MB."));

    State next = m_state;
    next.books.append(std::move(book));
    next.activeBook = static_cast<int>(next.books.size()) - 1;
    next.page = 0;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory);
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

bool Notebook::importLinkedMarkdown(const QString &text, const QString &title, const QString &folderId, const QString &id)
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
    next.activeBook = static_cast<int>(next.books.size()) - 1;
    next.page = 0;
    return commit(std::move(next), BooksChanged | DocumentChanged | StrokesChanged | PageContentChanged | ResetInkHistory, true);
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
    const int itemIndex = findPageItem(page, itemId);
    if (itemIndex < 0 || !isLibraryItem(page.items.at(itemIndex).toMap().value(QStringLiteral("type")))) {
        fail(tr("L’elemento non è più presente sulla pagina."));
        return {};
    }

    const QVariantMap item = PageContent::resolveTableCharts(page.items).at(itemIndex).toMap();
    return openBookPage(bookIndex, pageIndex) ? item : QVariantMap{};
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
    copy.id = newId();
    resetPageIds(copy);
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
            {QStringLiteral("background"), page.background},
            {QStringLiteral("tags"), page.tags}};
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

QVariantMap Notebook::selectInk(const QVariantList &outline) const
{
    const Page *page = currentPage();
    if (!page || !canWriteLayer() || outline.size() < 3 || outline.size() > 12000)
        return {};

    QPolygonF polygon;
    polygon.reserve(qMin<qsizetype>(outline.size(), 256));
    const qsizetype step = (outline.size() + 255) / 256;
    for (qsizetype i = 0; i < outline.size(); i += step) {
        const QVariantMap point = outline.at(i).toMap();
        double x = 0;
        double y = 0;
        if (!readCoordinate(point.value(QStringLiteral("x")), x) || !readCoordinate(point.value(QStringLiteral("y")), y))
            return {};
        polygon.append(QPointF(x, y));
    }
    const QRectF bounds = polygon.boundingRect();
    if (polygon.size() < 3 || bounds.width() < 0.008 || bounds.height() < 0.008)
        return {};

    QVariantList selected;
    QVariantList indexes;
    int reversible = 0;
    const QVariantList &ink = activeLayerInk(*page);
    const int offset = erasableFrom();
    for (int index = 0; index < ink.size(); ++index) {
        const QVariantMap stroke = ink.at(index).toMap();
        const auto tool = inkToolFromName(stroke.value(QStringLiteral("tool")).toString());
        const auto anchors = storedInkPoints(stroke);
        const auto points = isInkShape(tool) ? shapeOutline(anchors, tool) : anchors;
        if (!mostlyInside(polygon, bounds, points))
            continue;
        indexes.append(index + offset);
        selected.append(stroke);
        if (stroke.contains(QStringLiteral("original")))
            ++reversible;
    }
    return {{QStringLiteral("indexes"), indexes}, {QStringLiteral("strokes"), selected},
            {QStringLiteral("reversible"), reversible}, {QStringLiteral("bookId"), bookId()},
            {QStringLiteral("pageId"), page->id}, {QStringLiteral("layerId"), activeLayer()}};
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

    const bool changesItems = !edit.textItem.isEmpty();
    if (m_undo.size() == maxInkHistory)
        m_undo.removeFirst();
    m_undo.append(std::move(edit));
    m_redo.clear();
    emit strokesChanged();
    if (changesItems)
        emit pageContentChanged();
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
    const bool changesItems = !edit.textItem.isEmpty();
    destination.append(std::move(edit));
    emit strokesChanged();
    if (changesItems)
        emit pageContentChanged();
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
    if (!page->background.isEmpty())
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

bool Notebook::replaceHandwriting(const QVariantMap &selection, const QString &text)
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

    item.insert(QStringLiteral("x"), left);
    item.insert(QStringLiteral("y"), top);
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

void Notebook::resetPageIds(Book &book)
{
    const int last = findPage(book, book.lastViewedPageId);
    for (qsizetype index = 0; index < book.pages.size(); ++index)
        book.pages[index].id = legacyPageId(book.id, index);
    if (last >= 0)
        book.lastViewedPageId = book.pages.at(last).id;
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

QJsonArray Notebook::writePages(const Book &book)
{
    QJsonArray pages;
    for (const Page &page : book.pages) {
        const QJsonArray ink = QJsonArray::fromVariantList(page.strokes);
        if (page.hasLayout()) {
            QJsonObject layout{
                {QStringLiteral("template"), page.paper},
                {QStringLiteral("strokes"), ink},
                {QStringLiteral("items"), QJsonArray::fromVariantList(page.items)},
            };
            if (!page.background.isEmpty())
                layout.insert(QStringLiteral("background"), QString::fromLatin1(page.background.toBase64()));
            if (!page.tags.isEmpty())
                layout.insert(QStringLiteral("tags"), QJsonArray::fromStringList(page.tags));
            if (!page.pdfText.isEmpty())
                layout.insert(QStringLiteral("pdfText"), page.pdfText);
            if (!page.markdownBlockId.isEmpty())
                layout.insert(QStringLiteral("markdownBlockId"), page.markdownBlockId);
            if (!page.transcripts.isEmpty())
                layout.insert(QStringLiteral("transcripts"), QJsonArray::fromStringList(page.transcripts));
            if (page.bookmarked)
                layout.insert(QStringLiteral("bookmarked"), true);
            if (!page.layers.isEmpty()) {
                QJsonArray layers;
                for (const InkLayer &layer : page.layers) {
                    layers.append(QJsonObject{{QStringLiteral("id"), layer.id}, {QStringLiteral("name"), layer.name},
                                               {QStringLiteral("visible"), layer.visible},
                                               {QStringLiteral("strokes"), QJsonArray::fromVariantList(layer.strokes)}});
                }
                layout.insert(QStringLiteral("layers"), layers);
                layout.insert(QStringLiteral("activeLayer"), page.activeLayer);
            }
            pages.append(layout);
        } else {
            pages.append(ink);
        }
    }
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
    return fileRevision(QJsonDocument(QJsonArray{book.title, pages}).toJson(QJsonDocument::Compact));
}

QString Notebook::load()
{
    QByteArray contents;
    const QString readError = readFile(m_filePath, contents);
    if (!readError.isEmpty() || contents.isNull())
        return readError;

    State loaded;
    const QString error = readState(contents, loaded);
    if (!error.isEmpty())
        return error;
    m_fileRevision = fileRevision(contents);
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
    if (version < LegacyFileVersion || version > ImageFileVersion)
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
                    page.background = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
                    if (!validBackground(page.background))
                        return invalidFile;
                }
                const QJsonValue layerData = layout.value(QStringLiteral("layers"));
                const QJsonValue indexedText = layout.value(QStringLiteral("pdfText"));
                const QJsonValue markdownBlock = layout.value(QStringLiteral("markdownBlockId"));
                const QJsonValue transcripts = layout.value(QStringLiteral("transcripts"));
                const QJsonValue bookmark = layout.value(QStringLiteral("bookmarked"));
                if ((!indexedText.isUndefined() && (!indexedText.isString() || page.background.isEmpty()))
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
            if (!page.background.isEmpty() || !page.layers.isEmpty() || !page.tags.isEmpty()
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
    const QByteArray contents = serialize(next);
    if (enforceSizeLimit && contents.size() > maxImportBytes)
        return fail(tr("Il documento porterebbe la libreria oltre 64 MB."));
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit())
        return fail(tr("Impossibile salvare il file dei quaderni. %1").arg(file.errorString()));

    m_fileRevision = fileRevision(contents);
    m_state = std::move(next);
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
