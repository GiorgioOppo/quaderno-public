#include "mixedpageanalyzer.h"

#include "diagramitem.h"
#include "diagramrecognizer.h"
#include "documentreader.h"
#include "inkrender.h"
#include "inkselection.h"
#include "mixedpagegeometry.h"
#include "mixedpageregions.h"
#include "mixedpagetable.h"
#include "mixedpagetextink.h"
#include "pagecontent.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QProcess>
#include <QRegion>
#include <QThreadPool>
#include <QTransform>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr QSize analysisSize(900, 1200);
constexpr QSize rasterOcrSize(1600, 2133);
constexpr int maxProseLines = 24;
constexpr int maxTableRows = 12;
constexpr int maxTableColumns = 8;
constexpr int proseMargin = 5;
constexpr int tableMargin = 16;
constexpr int maxDiagramLabels = 16;
constexpr int maxRasterDiagramLabels = 32;
constexpr int diagramLabelMargin = 5;
using MixedPageRegions::Regions;

enum class RasterOcrCropKind { Line, Cell, DiagramLabel, VerticalDiagramLabel };

struct RasterOcrCrop {
    QRectF bounds;
    const Regions *regions;
    int margin;
    RasterOcrCropKind kind = RasterOcrCropKind::Line;
    bool stacked = false;
    QVariantList glyphBounds;
    bool tinyLabel = false;
};

struct RasterOcrRequest {
    QByteArray data;
    QList<qsizetype> positions;
    qsizetype extraSearches = 0;
};

QThreadPool *analysisPool()
{
    static QThreadPool *const pool = [] {
        auto *value = new QThreadPool(QCoreApplication::instance());
        value->setMaxThreadCount(1);
        return value;
    }();
    return pool;
}

QRectF itemRect(const QVariantMap &item)
{
    return {item.value(QStringLiteral("x")).toReal(), item.value(QStringLiteral("y")).toReal(),
        item.value(QStringLiteral("width")).toReal(), item.value(QStringLiteral("height")).toReal()};
}

QRectF diagramNodeRect(const QVariantMap &diagram, const QVariantMap &node)
{
    const auto placement = itemRect(diagram);
    const auto local = itemRect(node);
    return placement.isEmpty() ? local : QRectF(placement.x() + local.x() * placement.width(),
        placement.y() + local.y() * placement.height(), local.width() * placement.width(), local.height() * placement.height());
}

QImage pageBackground(const QVariantMap &content, QString &error)
{
    const auto source = content.value(QStringLiteral("source")).toMap();
    if (!source.isEmpty())
        return renderDocumentPage(source, 1600, error);
    const auto bytes = content.value(QStringLiteral("background")).toByteArray();
    if (!bytes.isEmpty()) {
        QBuffer buffer;
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QSize size = reader.size();
        if (size.isValid() && qMax(size.width(), size.height()) > 1600)
            reader.setScaledSize(size.scaled(QSize(1600, 1600), Qt::KeepAspectRatio));
        const QImage background = reader.read();
        if (background.isNull())
            error = MixedPageAnalyzer::tr("Impossibile leggere l’immagine della pagina.");
        return background;
    }
    return {};
}

QImage renderPage(const QVariantMap &content, const QImage &background, QSize targetSize)
{
    QImage image(targetSize, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    if (!background.isNull())
        paintPdfBackground(painter, image.size(), background);
    PageContent::paintItems(painter, image.size(), content.value(QStringLiteral("items")).toList());
    paintStoredStrokes(painter, image.size(), content.value(QStringLiteral("strokes")).toList());
    painter.end();
    return image;
}

QRect pixelBounds(const QRectF &bounds, QSize size)
{
    return QRectF(bounds.x() * size.width(), bounds.y() * size.height(),
        bounds.width() * size.width(), bounds.height() * size.height()).toAlignedRect();
}

QString rasterOcrHelper()
{
#ifdef Q_OS_LINUX
    const QFileInfo binary(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("quaderno-raster-ocr")));
    if (binary.isFile() && binary.isExecutable())
        return binary.absoluteFilePath();
#endif
    return {};
}

QImage trimTinyLabelBorder(const QImage &gray)
{
    const int edgeRows = qMin(3, gray.height() / 7);
    const auto trimFrom = [&](int start, int step) {
        int trimmed = 0;
        for (int offset = 0; offset < edgeRows; ++offset) {
            const auto *pixels = gray.constScanLine(start + offset * step);
            int solid = 0, longest = 0;
            for (int x = 0; x < gray.width(); ++x) {
                solid = pixels[x] < 150 ? solid + 1 : 0;
                longest = qMax(longest, solid);
            }
            // A box edge spans a substantial fraction of a short caption.
            // Only look at the first few rows where the edge can enter the crop.
            const int minimumRun = trimmed > 0 ? gray.width() / 4 : gray.width() * 2 / 5;
            if (longest >= qMax(14, minimumRun))
                trimmed = offset + 1;
        }
        return trimmed;
    };
    const int top = trimFrom(0, 1);
    const int bottom = trimFrom(gray.height() - 1, -1);
    const int height = gray.height() - top - bottom;
    return (top || bottom) && height >= 6 ? gray.copy(0, top, gray.width(), height) : gray;
}

RasterOcrRequest rasterOcrRequest(const QImage &page, const QList<RasterOcrCrop> &crops,
                                 const DocumentCancellation &cancel)
{
    constexpr qsizetype maxRequestBytes = 8 * 1024 * 1024;
    constexpr qsizetype maxImageBytes = 1024 * 1024;
    constexpr qsizetype maxImages = 136;
    constexpr qsizetype maxHalfTurns = 32;
    constexpr qsizetype maxTinyLabels = 32;
    constexpr qsizetype jsonOverhead = 1536; // Array names and bounded crop position lists.
    const auto contrastLevels = [](int whitePoint) {
        std::array<uchar, 256> levels{};
        for (int value = 0; value < int(levels.size()); ++value)
            levels[value] = value >= whitePoint ? 255 : int(std::pow(value / double(whitePoint), 1.1) * 255);
        return levels;
    };
    static const auto standardContrast = contrastLevels(178);
    static const auto compactContrast = contrastLevels(205);
    qsizetype encodedBytes = 0;
    QJsonArray images;
    QJsonArray halfTurns;
    QJsonArray tinyLabels;
    RasterOcrRequest request;
    for (qsizetype index = 0; index < crops.size() && images.size() < maxImages; ++index) {
        if (cancel->load())
            return {};
        const auto &spec = crops.at(index);
        const bool cell = spec.kind == RasterOcrCropKind::Cell;
        const bool vertical = spec.kind == RasterOcrCropKind::VerticalDiagramLabel;
        const bool diagramLabel = vertical || spec.kind == RasterOcrCropKind::DiagramLabel;
        QRect crop = pixelBounds(spec.bounds, page.size());
        const int bodyHeight = vertical ? crop.width() : crop.height();
        const bool tinyLabel = spec.tinyLabel && tinyLabels.size() < maxTinyLabels
            && crop.width() >= 8 && crop.width() <= 320 && crop.height() >= 6 && crop.height() <= 64;
        // For short glyphs, fixed padding would take up much of the model's
        // 48-pixel input height and the stronger contrast erases thin edges.
        const int margin = diagramLabel ? std::clamp(qRound(bodyHeight * .14), 2, spec.margin) : spec.margin;
        const auto &contrast = diagramLabel && bodyHeight <= 20 ? compactContrast : standardContrast;
        if (!cell && !tinyLabel)
            crop.adjust(-margin, -margin, margin, margin);
        crop = crop.intersected(page.rect());
        if (crop.isEmpty() || crop.width() * crop.height() > 2'000'000) {
            if (diagramLabel)
                continue;
            return {};
        }
        QImage selected = MixedPageRegions::mask(page, *spec.regions, crop);
        if (spec.stacked && !spec.glyphBounds.isEmpty()) {
            QRegion letterRegions;
            for (const auto &glyph : spec.glyphBounds) {
                const auto bounds = pixelBounds(glyph.toRectF(), page.size()).adjusted(-1, -1, 1, 1);
                letterRegions |= bounds.translated(-crop.topLeft()).intersected(selected.rect());
            }
            if (!letterRegions.isEmpty()) {
                QImage isolated(selected.size(), QImage::Format_RGB32);
                if (isolated.isNull())
                    continue;
                isolated.fill(Qt::white);
                QPainter painter(&isolated);
                painter.setClipRegion(letterRegions);
                painter.drawImage(0, 0, selected);
                painter.end();
                selected = std::move(isolated);
            }
        }
        QImage gray = selected.convertToFormat(QImage::Format_Grayscale8);
        if (gray.isNull())
            return {};
        if (tinyLabel)
            gray = trimTinyLabelBorder(gray);
        int left = gray.width(), right = -1, top = gray.height(), bottom = -1;
        for (int y = 0; !tinyLabel && y < gray.height(); ++y) {
            uchar *row = gray.scanLine(y);
            for (int x = 0; x < gray.width(); ++x) {
                if (cell && row[x] < 145) {
                    left = qMin(left, x);
                    right = qMax(right, x);
                    top = qMin(top, y);
                    bottom = y;
                }
                row[x] = contrast[row[x]];
            }
        }
        if (cell) {
            if (right < 0)
                continue; // Keep the position for an empty cell without calling the model.
            const QRect ink(QPoint(left, top), QPoint(right, bottom));
            gray = gray.copy(ink.adjusted(-margin, -margin, margin, margin).intersected(gray.rect()));
        }
        if (vertical) {
            gray = gray.transformed(QTransform().rotate(90));
            if (gray.width() > rasterOcrSize.width())
                gray = gray.scaledToWidth(rasterOcrSize.width(), Qt::SmoothTransformation);
        }
        QByteArray png;
        QBuffer output(&png);
        if (!output.open(QIODevice::WriteOnly) || !gray.save(&output, "PNG") || png.size() > maxImageBytes) {
            if (diagramLabel)
                continue;
            return {};
        }
        const QByteArray encoded = png.toBase64();
        const qsizetype nextBytes = encodedBytes + encoded.size() + 3;
        if (nextBytes + jsonOverhead > maxRequestBytes) {
            if (diagramLabel)
                continue;
            return {};
        }
        encodedBytes = nextBytes;
        if (vertical && halfTurns.size() < maxHalfTurns) {
            const int position = int(images.size());
            halfTurns.append(spec.stacked ? QJsonValue(QJsonObject{{QStringLiteral("index"), position},
                {QStringLiteral("stacked"), true}}) : QJsonValue(position));
        }
        if (tinyLabel && gray.width() >= 8 && gray.height() >= 6)
            tinyLabels.append(int(images.size()));
        images.append(QString::fromLatin1(encoded));
        request.positions.append(index);
    }
    if (!images.isEmpty()) {
        QJsonObject object{{QStringLiteral("images"), images}};
        if (!halfTurns.isEmpty())
            object.insert(QStringLiteral("tryHalfTurn"), halfTurns);
        if (!tinyLabels.isEmpty())
            object.insert(QStringLiteral("tinyLabels"), tinyLabels);
        request.data = QJsonDocument(object).toJson(QJsonDocument::Compact);
        request.extraSearches = halfTurns.size() + tinyLabels.size();
    }
    return request;
}

QStringList rasterOcrText(const QString &helper, const RasterOcrRequest &request, qsizetype cropCount,
                          const DocumentCancellation &cancel)
{
    if (request.data.isEmpty() || cancel->load())
        return {};
    constexpr qsizetype maxOutputBytes = 64 * 1024;
    constexpr qsizetype maxLineLength = 512;
    constexpr int pollMs = 50;
    // On the tablet, recognition time grows with the number of nonempty crops.
    // Small horizontal labels and vertical labels can each try two extra passes.
    QDeadlineTimer deadline(18'000 + 1'500 * (request.positions.size() + 2 * request.extraSearches));
    QProcess process;
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(helper, {});
    while (process.state() == QProcess::Starting && !cancel->load() && !deadline.hasExpired())
        process.waitForStarted(pollMs);
    if (process.state() != QProcess::Running || cancel->load() || process.write(request.data) != request.data.size())
        return {};
    while (process.bytesToWrite() > 0 && process.state() == QProcess::Running
           && !cancel->load() && !deadline.hasExpired())
        process.waitForBytesWritten(pollMs);
    process.closeWriteChannel();

    QByteArray response;
    while (process.state() == QProcess::Running && !cancel->load() && !deadline.hasExpired()) {
        process.waitForReadyRead(pollMs);
        response.append(process.readAllStandardOutput());
        if (response.size() > maxOutputBytes)
            break;
    }
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(250);
        return {};
    }
    response.append(process.readAllStandardOutput());
    if (cancel->load() || deadline.hasExpired() || response.size() > maxOutputBytes
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {};

    const auto lines = QJsonDocument::fromJson(response).object().value(QStringLiteral("lines")).toArray();
    if (lines.size() != request.positions.size())
        return {};
    QStringList result(cropCount);
    for (qsizetype index = 0; index < lines.size(); ++index) {
        const auto line = lines.at(index);
        const QString text = line.toString();
        if (!line.isString() || text.size() > maxLineLength || !text.isValidUtf16()
            || std::any_of(text.cbegin(), text.cend(), [](QChar ch) {
                return ch.category() == QChar::Other_Control || ch.category() == QChar::Other_Format
                    || ch == QChar::LineSeparator || ch == QChar::ParagraphSeparator;
            }))
            return {};
        result[request.positions.at(index)] = text.trimmed();
    }
    return result;
}

QVariantMap nativeItem(const QVariantList &items, const Regions &regions, const QString &type)
{
    QVariantMap answer;
    for (const QVariant &entry : items) {
        const auto item = entry.toMap();
        if (item.value(QStringLiteral("type")).toString() != type || !MixedPageRegions::contains(itemRect(item).center(), regions))
            continue;
        if (!answer.isEmpty())
            return {};
        answer = item;
        answer.remove(QStringLiteral("id"));
    }
    return answer;
}

QString tableText(const QVariantMap &table)
{
    const int columns = table.value(QStringLiteral("cols")).toInt();
    const auto cells = table.value(QStringLiteral("cells")).toList();
    if (columns <= 0 || cells.isEmpty())
        return {};
    QStringList rows;
    for (int start = 0; start + columns <= cells.size(); start += columns) {
        QStringList row;
        for (int i = 0; i < columns; ++i)
            row.append(cells.at(start + i).toString().replace(u'\n', u' ').replace(u'\t', u' '));
        rows.append(row.join(u'\t'));
    }
    return rows.join(u'\n');
}

QString rasterTableText(const QStringList &cells, qsizetype columns)
{
    if (columns <= 0 || cells.size() % columns
        || std::all_of(cells.cbegin(), cells.cend(), [](const QString &cell) { return cell.isEmpty(); }))
        return {};
    QStringList rows;
    for (qsizetype start = 0; start < cells.size(); start += columns)
        rows.append(cells.mid(start, columns).join(u'\t'));
    return rows.join(u'\n');
}

QVariantList textLines(const QImage &image, const Regions &regions, const QVariantList &nativeStrokes)
{
    auto lines = MixedPageTextInk::lines(MixedPageRegions::mask(image, regions));
    if (nativeStrokes.isEmpty())
        return lines;

    QList<QVariantList> originalInk(lines.size());
    for (const auto &stroke : nativeStrokes) {
        const QPointF center = InkSelectionGeometry::boundsOf({stroke}).center();
        if (!MixedPageRegions::contains(center, regions))
            continue;
        qsizetype closest = -1;
        qreal distance = std::numeric_limits<qreal>::max();
        for (qsizetype i = 0; i < lines.size(); ++i) {
            const auto bounds = lines.at(i).toMap().value(QStringLiteral("bounds")).toRectF();
            const qreal offset = qAbs(center.y() - bounds.center().y());
            if (bounds.adjusted(-.01, -.01, .01, .01).contains(center) && offset < distance) {
                distance = offset;
                closest = i;
            }
        }
        if (closest >= 0)
            originalInk[closest].append(stroke);
    }
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (!originalInk.at(i).isEmpty()) {
            auto line = lines.at(i).toMap();
            line.insert(QStringLiteral("strokes"), originalInk.at(i));
            lines[i] = line;
        }
    }
    return lines;
}

struct CaptionFragment {
    QRectF bounds;
    QVariantList strokes;
};

struct CaptionCandidate {
    QRectF bounds;
    QVariantList strokes;
    QList<QRectF> glyphBounds;
    qreal score = 0;
    bool vertical = false;
};

bool smallCaptionNode(const QRectF &box)
{
    return box.width() >= 15 && box.width() <= 58 && box.height() >= 15 && box.height() <= 52;
}

// Box text can sit just above or below its outline. Test the narrow bands
// around small nodes before considering the enclosing frame as the owner.
int smallCaptionBand(const QRectF &box, const QRectF &ink, qreal &distance)
{
    const QPointF center = ink.center();
    const qreal margin = std::clamp(box.width() * .47, 10., 19.);
    if (!smallCaptionNode(box) || ink.height() > 19 || ink.width() > box.width() + margin
        || center.x() < box.left() - margin || center.x() > box.right() + margin)
        return -1;
    const qreal dy = qMax(box.top() - center.y(), center.y() - box.bottom());
    const qreal dx = qAbs(center.x() - box.center().x());
    if (dy > std::clamp(box.height() * .58, 13., 22.) || dx > box.width() / 2 + margin)
        return -1;
    if (dy < 0) {
        if (!box.adjusted(3, 3, -3, -3).contains(center)) return -1;
        distance = dx * .30 + qAbs(center.y() - box.center().y()) * .35;
        return 0;
    }
    distance = dx * .30 + dy * .73 + 2.;
    return center.y() < box.top() ? 1 : 2;
}

QRectF smallCaptionCrop(const QRectF &box, const QRectF &bounds)
{
    auto crop = bounds.adjusted(-1.5, -1.8, 1.5, 1.5);
    const bool above = bounds.center().y() < box.top();
    const bool below = bounds.center().y() > box.bottom();
    if (above || below) {
        // The closest glyph can touch the box and be removed along with its
        // outline during segmentation. Include that part of the caption without
        // bringing the horizontal border back into the OCR crop.
        if (bounds.width() < box.width() * .8 + 5) {
            crop.setLeft(qMin(crop.left(), box.left() - 2));
            crop.setRight(qMax(crop.right(), box.right() + 2));
        }
        if (above)
            crop.setBottom(qMin(crop.bottom(), box.top() - .8));
        else
            crop.setTop(qMax(crop.top(), box.bottom() + .8));
    }
    return crop;
}

QRectF captionAxes(const QRectF &box, bool vertical)
{
    return vertical ? QRectF(box.y(), box.x(), box.height(), box.width()) : box;
}

CaptionCandidate supplementalCaption(const QList<CaptionFragment> &fragments, const QRectF &node, bool vertical)
{
    const bool titleStrip = vertical && node.height() >= node.width() * 2.3;
    QList<qsizetype> indexes;
    QList<qreal> heights;
    QList<qreal> advances;
    for (qsizetype index = 0; index < fragments.size(); ++index) {
        const auto box = captionAxes(fragments.at(index).bounds, vertical);
        if (box.height() >= 2.0 && box.height() <= 26.0)
            heights.append(box.height());
        if (box.width() >= 3.0 && box.width() <= 30.0)
            advances.append(box.width());
        indexes.append(index);
    }
    if (heights.isEmpty() || advances.isEmpty())
        return {};
    std::sort(heights.begin(), heights.end());
    std::sort(advances.begin(), advances.end());
    const qreal height = heights.at(heights.size() / 2);
    const qreal advance = advances.at(advances.size() / 2);
    const qreal baselineTolerance = titleStrip ? qMax(6., height * .9) : qMax(2.8, height * .63);
    const qreal maxGap = titleStrip ? std::clamp(advance * 1.45, 12., 26.) : std::clamp(height * 1.5, 4., 13.);
    const auto along = [&](qsizetype index) { return captionAxes(fragments.at(index).bounds, vertical); };
    std::stable_sort(indexes.begin(), indexes.end(), [&](qsizetype a, qsizetype b) {
        return along(a).left() < along(b).left();
    });

    struct Group {
        QRectF box;
        QList<qsizetype> members;
        qreal baseline = 0;
    };
    QList<Group> groups;
    for (const auto index : indexes) {
        const auto box = along(index);
        if (box.height() > qMax(9., height * 2.25)
            || (box.height() < 2.2 && box.width() > qMax(titleStrip ? 24. : 9., height * 2.)))
            continue;
        qsizetype selected = -1;
        qreal best = std::numeric_limits<qreal>::infinity();
        for (qsizetype i = 0; i < groups.size(); ++i) {
            const auto &group = groups.at(i);
            const qreal gap = box.left() - group.box.right();
            const qreal offset = qAbs(box.center().y() - group.baseline);
            const qreal cost = offset + qMax(0., gap) * .25;
            if (gap <= maxGap && offset <= baselineTolerance && cost < best) {
                selected = i;
                best = cost;
            }
        }
        if (selected < 0) {
            groups.append({box, {index}, box.center().y()});
        } else {
            auto &group = groups[selected];
            group.box |= box;
            group.members.append(index);
            group.baseline += (box.center().y() - group.baseline) / group.members.size();
        }
    }

    CaptionCandidate best;
    for (const auto &group : groups) {
        const auto length = group.box.width();
        const auto across = group.box.height();
        const auto parts = group.members.size();
        if (parts < 2 || across < 2.8 || across > qMax(7., height * (titleStrip ? 3.1 : 2.1))
            || length < qMax(6., across)
            || (vertical && length < qMax(10., across * (parts == 2 ? 2.15 : 1.6))))
            continue;
        const qreal offset = qAbs(group.box.center().y() - captionAxes(node, vertical).center().y());
        const qreal score = qMin(qsizetype(12), parts) * 3.6 + qMin(120., length) * .12 - offset * .15;
        if (score <= best.score)
            continue;
        best = {captionAxes(group.box, vertical), {}, {}, score, vertical};
        QList<QRectF> glyphs;
        for (const auto index : group.members) {
            const auto &fragment = fragments.at(index);
            best.strokes.append(fragment.strokes);
            if (glyphs.isEmpty() || fragment.bounds.top() > glyphs.last().bottom() + 1.)
                glyphs.append(fragment.bounds);
            else
                glyphs.last() |= fragment.bounds;
        }
        int spaces = 0;
        for (qsizetype i = 1; i < glyphs.size(); ++i)
            spaces += glyphs.at(i).top() - glyphs.at(i - 1).bottom() >= qMax(3.5, advance * .48);
        if (vertical && glyphs.size() >= 3 && (titleStrip || spaces >= (glyphs.size() + 1) / 2))
            best.glyphBounds = glyphs;
    }
    return best;
}

QVariantList diagramLabelJobs(const QVariantMap &diagram, const QVariantList &inkLines, const QImage &rasterCaptionImage)
{
    struct OrderedStroke {
        qsizetype position;
        QVariant ink;
    };
    struct Target {
        QString id;
        QRectF box;
        QPainterPath footprint;
        QPainterPath outlineBand;
        bool writable;
        QRectF bounds;
        QList<OrderedStroke> strokes;
        qreal quality = 0;
        bool vertical = false;
        QList<QRectF> glyphBounds;
    };
    struct Fragment {
        QRectF bounds;
        QList<OrderedStroke> strokes;
    };

    const auto wordGap = [](qreal firstHeight, qreal secondHeight) {
        return std::clamp(qMax(firstHeight, secondHeight) * 2.4, 23., 38.);
    };
    const QTransform pixels = QTransform::fromScale(analysisSize.width(), analysisSize.height());
    const QTransform normalized = pixels.inverted();
    QTransform rotation;
    const qreal angle = diagram.value(QStringLiteral("rotation")).toReal();
    if (std::isfinite(angle) && angle != 0) {
        const auto pivot = pixels.mapRect(itemRect(diagram)).center();
        rotation.translate(pivot.x(), pivot.y());
        rotation.rotate(angle);
        rotation.translate(-pivot.x(), -pivot.y());
    }
    QList<Target> targets;
    for (const auto &entry : diagram.value(QStringLiteral("nodes")).toList()) {
        const auto node = entry.toMap();
        const auto unrotated = pixels.mapRect(diagramNodeRect(diagram, node));
        const auto id = node.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || unrotated.isEmpty())
            continue;
        const auto footprint = rotation.map(diagramNodeOutline(node.value(QStringLiteral("shape")).toString(), unrotated));
        const auto box = footprint.boundingRect();
        QPainterPathStroker stroker;
        stroker.setWidth(std::clamp(qMin(box.width(), box.height()) * .2, 6., 14.));
        targets.append({id, box, footprint, stroker.createStroke(footprint),
            node.value(QStringLiteral("text")).toString().trimmed().isEmpty(), {}, {}, 0});
    }

    qsizetype position = 0;
    for (const auto &entry : inkLines) {
        QList<Fragment> fragments;
        QList<qreal> glyphHeights;
        for (const auto &stroke : entry.toMap().value(QStringLiteral("strokes")).toList()) {
            const qsizetype originalPosition = position++;
            const auto bounds = pixels.mapRect(InkSelectionGeometry::boundsOf({stroke})).adjusted(-.5, -.5, .5, .5);
            const auto points = storedInkPoints(stroke.toMap());
            if (points.isEmpty())
                continue;
            // Reject recognized outlines before considering any target, including
            // child outlines that an enclosing node would otherwise claim.
            const bool outline = std::any_of(targets.cbegin(), targets.cend(), [&](const Target &target) {
                const auto frame = target.box;
                if (!frame.adjusted(-7, -7, 7, 7).contains(bounds)
                    || (bounds.width() < frame.width() * .46 && bounds.height() < frame.height() * .46))
                    return false;
                constexpr qsizetype maxSamples = 16;
                const qsizetype samples = qMin(maxSamples, points.size());
                if (samples < 4)
                    return false;
                int hits = 0;
                for (qsizetype i = 0; i < samples; ++i) {
                    const auto point = pixels.map(points.at(i * (points.size() - 1) / (samples - 1)).position);
                    hits += target.outlineBand.contains(point);
                }
                return hits * 4 >= samples * 3;
            });
            if (outline)
                continue;
            fragments.append({bounds, {{originalPosition, stroke}}});
            if (bounds.height() >= 4 && bounds.height() <= 48)
                glyphHeights.append(bounds.height());
        }
        if (fragments.isEmpty() || glyphHeights.isEmpty())
            continue;
        std::sort(glyphHeights.begin(), glyphHeights.end());
        const qreal glyphHeight = glyphHeights.at(glyphHeights.size() / 2);
        std::stable_sort(fragments.begin(), fragments.end(), [](const Fragment &a, const Fragment &b) {
            return a.bounds.left() < b.bounds.left();
        });

        // Preserve whole words when the ink is near more than one nearby node.
        // The same page line may contain independent captions side by side.
        QList<Fragment> words;
        for (const auto &fragment : fragments) {
            if (words.isEmpty() || fragment.bounds.left() - words.last().bounds.right() > qMax(5., glyphHeight * .66)) {
                words.append(fragment);
            } else {
                words.last().bounds |= fragment.bounds;
                words.last().strokes.append(fragment.strokes);
            }
        }
        QList<Fragment> lineParts(targets.size());
        for (const auto &word : words) {
            const auto &bounds = word.bounds;
            // A tall isolated connector can share a text row and otherwise
            // widen its crop enough to discard a usable caption.
            if (word.strokes.size() == 1 && bounds.width() < qMax(9., glyphHeight * .75)
                && bounds.height() > qMax(30., glyphHeight * 3.0))
                continue;
            const auto center = bounds.center();
            qsizetype owner = -1;
            qreal closest = std::numeric_limits<qreal>::max();
            for (qsizetype i = 0; i < targets.size(); ++i) {
                const auto &target = targets.at(i);
                const auto &box = target.box;
                const qreal overlap = qMin(bounds.right(), box.right()) - qMax(bounds.left(), box.left());
                if (overlap < qMin(bounds.width(), box.width()) * .53)
                    continue;
                const qreal outside = std::max({box.top() - center.y(), center.y() - box.bottom(), 0.});
                const bool inside = target.footprint.contains(center);
                if (!inside && (outside > std::clamp(box.height() * .7, 13., 31.)
                    || (outside == 0 && !target.footprint.intersects(bounds))))
                    continue;
                const qreal dx = qAbs(center.x() - box.center().x()) / box.width();
                const qreal scalePenalty = qMin(38., std::sqrt(box.width() * box.height()) * .14);
                const qreal distance = outside * .70 + dx * 7.0 + scalePenalty + (inside ? 0. : 4.);
                if (distance < closest) {
                    closest = distance;
                    owner = i;
                }
            }
            if (owner < 0 || !targets.at(owner).writable)
                continue;
            auto &part = lineParts[owner];
            if (part.strokes.isEmpty()
                || bounds.left() - part.bounds.right() <= wordGap(bounds.height(), part.bounds.height())) {
                part.bounds |= bounds;
                part.strokes.append(word.strokes);
            } else if (word.strokes.size() > part.strokes.size()) {
                part = word;
            }
        }
        for (qsizetype i = 0; i < lineParts.size(); ++i) {
            const auto &part = lineParts.at(i);
            if (part.bounds.width() < 6 || part.bounds.height() < 5 || part.bounds.height() > qMax(30., glyphHeight * 2.3))
                continue;
            auto &target = targets[i];
            const auto &existing = target.bounds;
            const qreal gap = qMax(existing.left(), part.bounds.left()) - qMin(existing.right(), part.bounds.right());
            const qreal verticalOffset = qAbs(existing.center().y() - part.bounds.center().y());
            const bool merge = !target.strokes.isEmpty() && gap >= -3
                && gap <= wordGap(existing.height(), part.bounds.height())
                && verticalOffset <= qMax(existing.height(), part.bounds.height()) * .75;
            QRectF combined = part.bounds;
            QList<OrderedStroke> strokes = part.strokes;
            if (merge) {
                combined |= existing;
                if (part.bounds.left() < existing.left())
                    strokes.append(target.strokes);
                else {
                    strokes = target.strokes;
                    strokes.append(part.strokes);
                }
            }
            const qreal dy = qAbs(target.box.center().y() - combined.center().y());
            const qreal quality = qMin(qsizetype(10), strokes.size()) * 3.6
                + qMin(135., combined.width()) * .12 - dy * .15;
            if (quality > target.quality) {
                target.bounds = combined;
                target.strokes = strokes;
                target.quality = quality;
            }
        }
    }

    if (!rasterCaptionImage.isNull()) {
        // Recover components below the body-text baseline threshold and group
        // them on each axis inside their own shape. This supplements the
        // established horizontal pass, including its outside-node captions.
        QList<QList<CaptionFragment>> fragments(targets.size());
        QList<std::array<QList<CaptionFragment>, 3>> smallBands(targets.size());
        for (const auto &entry : MixedPageTextInk::diagramComponents(rasterCaptionImage)) {
            const auto component = entry.toMap();
            const auto bounds = pixels.mapRect(component.value(QStringLiteral("bounds")).toRectF());
            if (qMax(bounds.width(), bounds.height()) > 47. || qMax(bounds.width(), bounds.height()) < 2.)
                continue;
            const auto center = bounds.center();
            const CaptionFragment fragment{bounds, component.value(QStringLiteral("strokes")).toList()};
            qsizetype smallOwner = -1;
            int band = -1;
            qreal bestSmall = std::numeric_limits<qreal>::infinity(), secondSmall = bestSmall;
            for (qsizetype i = 0; i < targets.size(); ++i) {
                const auto &target = targets.at(i);
                qreal distance = 0;
                const int region = smallCaptionBand(target.box, bounds, distance);
                if (region < 0) continue;
                if (distance < bestSmall) {
                    secondSmall = bestSmall;
                    bestSmall = distance;
                    smallOwner = i;
                    band = region;
                } else {
                    secondSmall = qMin(secondSmall, distance);
                }
            }
            if (smallOwner >= 0 && secondSmall - bestSmall >= 1.5 && targets.at(smallOwner).writable)
                smallBands[smallOwner][band].append(fragment);
            qsizetype owner = -1;
            qreal smallest = std::numeric_limits<qreal>::infinity();
            for (qsizetype i = 0; i < targets.size(); ++i) {
                const auto &target = targets.at(i);
                const qreal area = target.box.width() * target.box.height();
                if (area < smallest && target.footprint.contains(center)) {
                    owner = i;
                    smallest = area;
                }
            }
            if (owner < 0 || !targets.at(owner).writable || targets.at(owner).outlineBand.contains(center))
                continue;
            const auto &box = targets.at(owner).box;
            const qreal endClearance = qMin(center.y() - box.top(), box.bottom() - center.y());
            if (box.height() >= box.width() * 2.3 && endClearance < qMax(7., box.width() * .22))
                continue;
            fragments[owner].append(fragment);
        }
        for (qsizetype i = 0; i < targets.size(); ++i) {
            auto &target = targets[i];
            const auto &ink = fragments.at(i);
            if (ink.size() < 2)
                continue;
            const auto horizontal = supplementalCaption(ink, target.box, false);
            const auto vertical = supplementalCaption(ink, target.box, true);
            const bool hasWideCaption = target.bounds.width() >= qMax(11., target.bounds.height() * 1.5);
            const bool hasStackedTitle = target.box.height() >= target.box.width() * 2.3 && !vertical.glyphBounds.isEmpty();
            const bool useVertical = (!hasWideCaption || hasStackedTitle) && vertical.score > horizontal.score + 1.5;
            const auto &candidate = useVertical ? vertical : horizontal;
            if (candidate.strokes.isEmpty() || (!target.strokes.isEmpty() && !useVertical))
                continue;
            const bool alreadyOwned = std::any_of(targets.cbegin(), targets.cend(), [&](const Target &other) {
                if (&other == &target || other.strokes.isEmpty()
                    || !target.footprint.contains(other.box.center())
                    || other.box.width() * other.box.height() >= target.box.width() * target.box.height())
                    return false;
                const auto overlap = candidate.bounds.intersected(other.bounds);
                return overlap.width() * overlap.height() >= candidate.bounds.width() * candidate.bounds.height() * .6;
            });
            if (alreadyOwned)
                continue;
            target.bounds = candidate.bounds;
            target.strokes.clear();
            target.quality = candidate.score;
            target.vertical = candidate.vertical;
            target.glyphBounds = candidate.glyphBounds;
            for (const auto &stroke : candidate.strokes)
                target.strokes.append({position++, stroke});
        }
        for (qsizetype i = 0; i < targets.size(); ++i) {
            auto &target = targets[i];
            if (target.vertical) continue;
            CaptionCandidate best;
            for (const auto &band : smallBands.at(i)) {
                if (band.size() < 2) continue;
                const auto candidate = supplementalCaption(band, target.box, false);
                if (candidate.bounds.width() >= 8 && candidate.bounds.height() >= 5
                    && candidate.bounds.height() <= 15 && candidate.score > best.score)
                    best = candidate;
            }
            if (best.strokes.isEmpty()) continue;
            const auto overlap = best.bounds.intersected(target.bounds);
            const bool extendsOld = overlap.width() * overlap.height() > qMin(best.bounds.width() * best.bounds.height(),
                    target.bounds.width() * target.bounds.height()) * .3
                && best.bounds.width() > target.bounds.width() + 3;
            if (!target.strokes.isEmpty() && !extendsOld && target.bounds.height() >= 6
                && best.score < target.quality * 1.16) continue;
            target.bounds = best.bounds;
            target.quality = best.score;
            target.strokes.clear();
            for (const auto &stroke : best.strokes)
                target.strokes.append({position++, stroke});
        }
    }
    targets.erase(std::remove_if(targets.begin(), targets.end(), [](const Target &target) {
        return target.strokes.isEmpty();
    }), targets.end());
    std::stable_sort(targets.begin(), targets.end(), [](const Target &a, const Target &b) {
        return a.quality > b.quality;
    });
    const auto limit = rasterCaptionImage.isNull() ? maxDiagramLabels : maxRasterDiagramLabels;
    targets.resize(qMin(targets.size(), qsizetype(limit)));
    std::stable_sort(targets.begin(), targets.end(), [](const Target &a, const Target &b) {
        return a.bounds.top() != b.bounds.top() ? a.bounds.top() < b.bounds.top() : a.bounds.left() < b.bounds.left();
    });
    QVariantList jobs;
    for (auto &target : targets) {
        std::sort(target.strokes.begin(), target.strokes.end(), [](const auto &a, const auto &b) {
            return a.position < b.position;
        });
        QVariantList strokes;
        strokes.reserve(target.strokes.size());
        for (const auto &stroke : target.strokes)
            strokes.append(stroke.ink);
        QVariantMap job{{QStringLiteral("nodeId"), target.id}, {QStringLiteral("vertical"), target.vertical},
            {QStringLiteral("bounds"), normalized.mapRect(target.bounds)}, {QStringLiteral("strokes"), strokes}};
        if (!rasterCaptionImage.isNull() && !target.vertical && smallCaptionNode(target.box)
            && target.bounds.height() >= 5 && target.bounds.height() <= 15
            && target.bounds.width() >= qMax(9., target.bounds.height() * 1.5)
            && target.bounds.width() <= target.box.width() * 1.4) {
            job.insert(QStringLiteral("tinyCrop"), normalized.mapRect(smallCaptionCrop(target.box, target.bounds)));
        }
        if (!target.glyphBounds.isEmpty()) {
            QVariantList glyphBounds;
            for (const auto &bounds : target.glyphBounds)
                glyphBounds.append(normalized.mapRect(bounds));
            job.insert(QStringLiteral("stacked"), true);
            job.insert(QStringLiteral("glyphBounds"), glyphBounds);
        }
        jobs.append(job);
    }
    return jobs;
}

void applyRasterLabels(QVariantMap &draft, const QVariantList &jobs, const QStringList &recognized)
{
    if (jobs.isEmpty() || jobs.size() != recognized.size())
        return;
    auto diagram = draft.value(QStringLiteral("diagram")).toMap();
    auto nodes = diagram.value(QStringLiteral("nodes")).toList();
    QVariantList unresolved;
    for (qsizetype i = 0; i < jobs.size(); ++i) {
        QString text = recognized.at(i).left(200);
        if (!text.isEmpty() && text.back().isHighSurrogate())
            text.chop(1);
        const auto id = jobs.at(i).toMap().value(QStringLiteral("nodeId"));
        if (std::none_of(text.cbegin(), text.cend(), [](QChar ch) { return ch.isLetterOrNumber(); })) {
            unresolved.append(jobs.at(i));
            continue;
        }
        for (auto &entry : nodes) {
            auto node = entry.toMap();
            if (node.value(QStringLiteral("id")) == id && node.value(QStringLiteral("text")).toString().trimmed().isEmpty()) {
                node.insert(QStringLiteral("text"), text);
                entry = node;
                break;
            }
        }
    }
    diagram.insert(QStringLiteral("nodes"), nodes);
    draft.insert(QStringLiteral("diagram"), diagram);
    draft.insert(QStringLiteral("diagramInk"), unresolved);
}

class AnalysisJob final : public QObject
{
    Q_OBJECT

public:
    void run(const QVariantMap &content, const Regions &diagramRegions, const Regions &tableRegions,
             const Regions &textRegions, const DocumentCancellation &cancel)
    {
        if (cancel->load()) {
            deleteLater();
            return;
        }
        QString error;
        const QImage background = pageBackground(content, error);
        const QImage image = error.isEmpty() ? renderPage(content, background, analysisSize) : QImage{};
        QVariantMap draft;
        if (!image.isNull() && !cancel->load()) {
            const auto items = content.value(QStringLiteral("items")).toList();
            const bool isInkPage = content.value(QStringLiteral("source")).toMap().isEmpty()
                && content.value(QStringLiteral("background")).toByteArray().isEmpty();
            const auto nativeStrokes = isInkPage ? content.value(QStringLiteral("strokes")).toList() : QVariantList{};
            auto diagram = nativeItem(items, diagramRegions, QStringLiteral("diagram"));
            QVariantList nativeLabels;
            if (diagram.isEmpty() && !nativeStrokes.isEmpty()) {
                QVariantList diagramInk;
                for (const QVariant &stroke : nativeStrokes) {
                    if (MixedPageRegions::contains(InkSelectionGeometry::boundsOf({stroke}).center(), diagramRegions))
                        diagramInk.append(stroke);
                }
                const auto proposal = DiagramRecognition::recognize(diagramInk);
                diagram = proposal.value(QStringLiteral("diagram")).toMap();
                for (const auto &entry : proposal.value(QStringLiteral("labelGroups")).toList()) {
                    const auto label = entry.toMap();
                    if (label.value(QStringLiteral("kind")) != QStringLiteral("node"))
                        continue;
                    const auto strokes = label.value(QStringLiteral("strokes")).toList();
                    nativeLabels.append(QVariantMap{{QStringLiteral("nodeId"), label.value(QStringLiteral("id"))},
                        {QStringLiteral("bounds"), InkSelectionGeometry::boundsOf(strokes)}, {QStringLiteral("strokes"), strokes}});
                    if (nativeLabels.size() >= maxDiagramLabels)
                        break;
                }
            }
            if (diagram.isEmpty() && !cancel->load())
                diagram = MixedPageGeometry::diagram(MixedPageRegions::mask(image, diagramRegions));
            const auto table = nativeItem(items, tableRegions, QStringLiteral("table"));
            const auto text = nativeItem(items, textRegions, QStringLiteral("text"))
                .value(QStringLiteral("text")).toString();
            draft = {{QStringLiteral("diagram"), diagram}, {QStringLiteral("tableText"), tableText(table)},
                {QStringLiteral("text"), text}};
            const QImage detail = !background.isNull() && !cancel->load()
                ? renderPage(content, background, rasterOcrSize) : QImage{};
            const auto diagramLines = nativeLabels.isEmpty() && !cancel->load() && !diagram.isEmpty()
                ? textLines(image, diagramRegions, nativeStrokes) : QVariantList{};
            const auto rasterCaptionImage = nativeLabels.isEmpty() && !cancel->load() && !diagram.isEmpty()
                ? MixedPageRegions::mask(detail, diagramRegions) : QImage{};
            const auto labelJobs = nativeLabels.isEmpty() ? diagramLabelJobs(diagram, diagramLines, rasterCaptionImage) : nativeLabels;
            draft.insert(QStringLiteral("diagramInk"), labelJobs);
            QList<QList<QRectF>> nativeCells;
            if (table.isEmpty() && tableRegions.size() == 1 && !nativeStrokes.isEmpty() && !cancel->load()) {
                // An embedded image can also contain a grid. Use the native ink
                // alone here so only a grid drawn with the pen takes this path.
                const auto ink = items.isEmpty() ? image
                    : renderPage({{QStringLiteral("strokes"), nativeStrokes}}, {}, analysisSize);
                nativeCells = MixedPageTable::cells(MixedPageRegions::mask(ink, tableRegions),
                    tableRegions.first().boundingRect());
            }
            const auto tableLines = !cancel->load() && table.isEmpty() && nativeCells.isEmpty()
                ? textLines(image, tableRegions, nativeStrokes).mid(0, maxTableRows) : QVariantList{};
            if (!nativeCells.isEmpty() && !cancel->load()) {
                const QString emptyRow(nativeCells.first().size() - 1, u'\t');
                draft.insert(QStringLiteral("tableText"), QStringList(nativeCells.size(), emptyRow).join(u'\n'));
                draft.insert(QStringLiteral("tableCellInk"),
                    MixedPageTable::cellInk(nativeCells, nativeStrokes, tableRegions.first()));
            }
            const auto proseLines = !cancel->load() && text.isEmpty()
                ? textLines(image, textRegions, nativeStrokes).mid(0, maxProseLines) : QVariantList{};
            draft.insert(QStringLiteral("tableInk"), tableLines);
            draft.insert(QStringLiteral("textInk"), proseLines);

            const QString helper = !background.isNull() && !cancel->load() ? rasterOcrHelper() : QString{};
            if (!helper.isEmpty()) {
                QList<RasterOcrCrop> crops;
                const auto appendLines = [&](const QVariantList &lines, const Regions &regions, int margin) {
                    for (const auto &line : lines)
                        crops.append({line.toMap().value(QStringLiteral("bounds")).toRectF(), &regions, margin});
                };
                auto cells = table.isEmpty() && tableRegions.size() == 1
                    ? MixedPageTable::cells(MixedPageRegions::mask(image, tableRegions), tableRegions.first().boundingRect()) : QList<QList<QRectF>>{};
                const auto columns = cells.isEmpty() ? 1 : cells.first().size();
                if (cells.size() <= maxTableRows && columns <= maxTableColumns && !cells.isEmpty()) {
                    for (const auto &row : cells) {
                        for (const auto &cell : row)
                            crops.append({cell, &tableRegions, tableMargin, RasterOcrCropKind::Cell});
                    }
                } else {
                    cells.clear();
                    appendLines(tableLines, tableRegions, tableMargin);
                }
                const auto tableCrops = crops.size();
                appendLines(proseLines, textRegions, proseMargin);
                const auto labelStart = crops.size();
                for (const auto &entry : labelJobs) {
                    const auto job = entry.toMap();
                    const bool vertical = job.value(QStringLiteral("vertical")).toBool();
                    const auto tinyCrop = job.value(QStringLiteral("tinyCrop")).toRectF();
                    const bool tinyLabel = !tinyCrop.isEmpty();
                    crops.append({tinyLabel ? tinyCrop : job.value(QStringLiteral("bounds")).toRectF(), &diagramRegions, diagramLabelMargin,
                        vertical ? RasterOcrCropKind::VerticalDiagramLabel : RasterOcrCropKind::DiagramLabel,
                        job.value(QStringLiteral("stacked")).toBool(), job.value(QStringLiteral("glyphBounds")).toList(), tinyLabel});
                }
                if (!crops.isEmpty() && !cancel->load()) {
                    const auto request = rasterOcrRequest(detail, crops, cancel);
                    const auto recognized = rasterOcrText(helper, request, crops.size(), cancel);
                    const QString recognizedTable = rasterTableText(recognized.mid(0, tableCrops),
                        cells.isEmpty() ? 1 : columns);
                    if (!recognizedTable.isEmpty()) {
                        draft.insert(QStringLiteral("tableText"), recognizedTable);
                        draft.remove(QStringLiteral("tableInk"));
                    }
                    const QString recognizedProse = recognized.mid(tableCrops, labelStart - tableCrops).join(u'\n');
                    if (!recognizedProse.trimmed().isEmpty()) {
                        draft.insert(QStringLiteral("text"), recognizedProse);
                        draft.remove(QStringLiteral("textInk"));
                    }
                    applyRasterLabels(draft, labelJobs, recognized.mid(labelStart));
                }
            }
        }
        if (!cancel->load())
            emit ready(draft, error);
        deleteLater();
    }

signals:
    void ready(const QVariantMap &draft, const QString &error);
};
}

MixedPageAnalyzer::MixedPageAnalyzer(QObject *parent) : QObject(parent) {}
MixedPageAnalyzer::~MixedPageAnalyzer()
{
    if (m_job)
        m_job->store(true);
}

void MixedPageAnalyzer::analyze(const QVariantMap &content, const QVariantMap &regions)
{
    if (busy())
        return;
    const auto diagram = MixedPageRegions::validate(regions.value(QStringLiteral("diagram")).toList());
    const auto table = MixedPageRegions::validate(regions.value(QStringLiteral("table")).toList());
    const auto text = MixedPageRegions::validate(regions.value(QStringLiteral("text")).toList());
    if (content.isEmpty() || diagram.isEmpty() || table.isEmpty() || text.isEmpty()) {
        emit finished({}, tr("Indica almeno un’area per ogni parte della pagina."));
        return;
    }
    const auto cancellation = std::make_shared<std::atomic_bool>(false);
    auto *job = new AnalysisJob;
    m_job = cancellation;
    emit busyChanged();
    connect(job, &AnalysisJob::ready, this, [this, cancellation](const QVariantMap &draft, const QString &error) {
        if (m_job != cancellation)
            return;
        m_job.reset();
        emit busyChanged();
        emit finished(draft, error);
    }, Qt::QueuedConnection);
    analysisPool()->start([job, content, diagram, table, text, cancellation] {
        job->run(content, diagram, table, text, cancellation);
    });
}

void MixedPageAnalyzer::cancel()
{
    if (!m_job)
        return;
    m_job->store(true);
    m_job.reset();
    emit busyChanged();
}

#include "mixedpageanalyzer.moc"
