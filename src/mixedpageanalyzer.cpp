#include "mixedpageanalyzer.h"

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
#include <QProcess>
#include <QThreadPool>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr QSize analysisSize(900, 1200);
constexpr QSize rasterOcrSize(1600, 2133);
constexpr int maxProseLines = 24;
constexpr int maxTableRows = 12;
constexpr int maxTableColumns = 8;
constexpr int proseMargin = 5;
constexpr int tableMargin = 16;
constexpr int maxDiagramLabels = 16;
constexpr int diagramLabelMargin = 5;
using MixedPageRegions::Regions;

struct RasterOcrCrop {
    QRectF bounds;
    const Regions *regions;
    int margin;
    bool cell = false;
};

struct RasterOcrRequest {
    QByteArray data;
    QList<qsizetype> positions;
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

RasterOcrRequest rasterOcrRequest(const QImage &page, const QList<RasterOcrCrop> &crops,
                                 const DocumentCancellation &cancel)
{
    constexpr qsizetype maxRequestBytes = 8 * 1024 * 1024;
    constexpr qsizetype maxImageBytes = 1024 * 1024;
    static const auto contrast = [] {
        std::array<uchar, 256> levels{};
        for (int value = 0; value < int(levels.size()); ++value)
            levels[value] = value >= 178 ? 255 : int(std::pow(value / 178.0, 1.1) * 255);
        return levels;
    }();
    qsizetype encodedBytes = 0;
    QJsonArray images;
    RasterOcrRequest request;
    for (qsizetype index = 0; index < crops.size(); ++index) {
        if (cancel->load())
            return {};
        const auto &spec = crops.at(index);
        const bool cell = spec.cell;
        const int margin = spec.margin;
        QRect crop = pixelBounds(spec.bounds, page.size());
        if (!cell)
            crop.adjust(-margin, -margin, margin, margin);
        crop = crop.intersected(page.rect());
        if (crop.isEmpty() || crop.width() * crop.height() > 2'000'000)
            return {};
        QImage gray = MixedPageRegions::mask(page, *spec.regions, crop).convertToFormat(QImage::Format_Grayscale8);
        int left = gray.width(), right = -1, top = gray.height(), bottom = -1;
        for (int y = 0; y < gray.height(); ++y) {
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
        QByteArray png;
        QBuffer output(&png);
        if (!output.open(QIODevice::WriteOnly) || !gray.save(&output, "PNG") || png.size() > maxImageBytes)
            return {};
        const QByteArray encoded = png.toBase64();
        encodedBytes += encoded.size() + 3;
        if (encodedBytes + 16 > maxRequestBytes)
            return {};
        images.append(QString::fromLatin1(encoded));
        request.positions.append(index);
    }
    if (!images.isEmpty())
        request.data = QJsonDocument(QJsonObject{{QStringLiteral("images"), images}}).toJson(QJsonDocument::Compact);
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
    QDeadlineTimer deadline(18'000 + 1'500 * request.positions.size());
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

QVariantList diagramLabels(const QVariantMap &diagram, const QVariantList &inkLines)
{
    const QVariantList nodes = diagram.value(QStringLiteral("nodes")).toList();
    QVariantList result;
    for (const QVariant &entry : inkLines) {
        const auto line = entry.toMap();
        const auto bounds = line.value(QStringLiteral("bounds")).toRectF();
        QString target;
        qreal bestScore = .05;
        for (const QVariant &nodeEntry : nodes) {
            const auto node = nodeEntry.toMap();
            if (!node.value(QStringLiteral("text")).toString().trimmed().isEmpty())
                continue;
            const auto rect = diagramNodeRect(diagram, node);
            if (bounds.left() < rect.left() - .018 || bounds.right() > rect.right() + .018)
                continue;
            const bool inside = rect.contains(bounds.center());
            const qreal offset = inside ? 0 : std::min(qAbs(bounds.center().y() - rect.top()),
                qAbs(bounds.center().y() - rect.bottom()));
            if (!inside && offset > .026)
                continue;
            const qreal score = (inside ? 0 : .023 + offset) + .04 * rect.width() * rect.height();
            if (score < bestScore) {
                bestScore = score;
                target = node.value(QStringLiteral("id")).toString();
            }
        }
        if (!target.isEmpty())
            result.append(QVariantMap{{QStringLiteral("nodeId"), target},
                {QStringLiteral("bounds"), bounds},
                {QStringLiteral("strokes"), line.value(QStringLiteral("strokes"))}});
        if (result.size() >= maxDiagramLabels)
            break;
    }
    return result;
}

QVariantList rasterDiagramLabels(const QVariantMap &diagram, const QVariantList &inkLines)
{
    struct Candidate {
        QString nodeId;
        QRectF node;
        QRectF bounds;
        QVariantList strokes;
        qreal quality = 0;
    };
    QList<Candidate> candidates;
    for (const QVariant &entry : diagram.value(QStringLiteral("nodes")).toList()) {
        const auto node = entry.toMap();
        const auto rect = diagramNodeRect(diagram, node);
        if (!rect.isEmpty() && node.value(QStringLiteral("text")).toString().trimmed().isEmpty())
            candidates.append({node.value(QStringLiteral("id")).toString(), rect, {}, {}, 0});
    }
    // A single raster row can span labels under several adjacent nodes.
    // Assign its strokes to nearby blank nodes before preparing the crops.
    for (const QVariant &entry : inkLines) {
        QList<QVariantList> parts(candidates.size());
        for (const auto &stroke : entry.toMap().value(QStringLiteral("strokes")).toList()) {
            const QRectF ink = InkSelectionGeometry::boundsOf({stroke});
            const QPointF center = ink.center();
            qsizetype target = -1;
            qreal closest = std::numeric_limits<qreal>::max();
            for (qsizetype i = 0; i < candidates.size(); ++i) {
                const QRectF node = candidates.at(i).node;
                const qreal xMargin = std::clamp(node.width() * .50, .008, .028);
                const qreal yMargin = std::clamp(node.height() * .70, .012, .026);
                if (!node.adjusted(-xMargin, -yMargin, xMargin, yMargin).contains(center))
                    continue;
                const qreal edgeX = qMin(qAbs(center.x() - node.left()), qAbs(center.x() - node.right()));
                const qreal edgeY = qMin(qAbs(center.y() - node.top()), qAbs(center.y() - node.bottom()));
                if ((edgeX < .004 && ink.width() < .007 && ink.height() >= node.height() * .55)
                    || (edgeY < .004 && ink.height() < .006 && ink.width() >= node.width() * .55)
                    || (ink.width() > node.width() * .75 && ink.height() > node.height() * .75))
                    continue;
                const qreal dy = std::max({node.top() - center.y(), center.y() - node.bottom(), 0.0});
                const qreal dx = qAbs(center.x() - node.center().x()) / node.width();
                const qreal distance = dy * .55 + dx * .005 + qMin(.025, node.width() * node.height() * .8);
                if (distance < closest) {
                    closest = distance;
                    target = i;
                }
            }
            if (target >= 0)
                parts[target].append(stroke);
        }
        for (qsizetype i = 0; i < parts.size(); ++i) {
            const auto &strokes = parts.at(i);
            const auto bounds = InkSelectionGeometry::boundsOf(strokes);
            if (bounds.width() < .007 || bounds.height() < .004)
                continue;
            auto &candidate = candidates[i];
            const qreal distance = qAbs(candidate.node.center().y() - bounds.center().y());
            const qreal quality = qMin(qsizetype(10), strokes.size()) * .003 + qMin(.15, bounds.width()) * .09 - distance * .15;
            if (quality > candidate.quality) {
                candidate.bounds = bounds;
                candidate.strokes = strokes;
                candidate.quality = quality;
            }
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto &left, const auto &right) {
        return left.quality > right.quality;
    });
    QVariantList labels;
    for (const auto &candidate : candidates) {
        if (candidate.strokes.isEmpty() || labels.size() >= maxDiagramLabels)
            break;
        labels.append(QVariantMap{{QStringLiteral("nodeId"), candidate.nodeId},
            {QStringLiteral("bounds"), candidate.bounds}, {QStringLiteral("strokes"), candidate.strokes}});
    }
    return labels;
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
            const auto diagramLines = nativeLabels.isEmpty() && !cancel->load() && !diagram.isEmpty()
                ? textLines(image, diagramRegions, nativeStrokes) : QVariantList{};
            draft.insert(QStringLiteral("diagramInk"), nativeLabels.isEmpty() ? diagramLabels(diagram, diagramLines) : nativeLabels);
            const auto tableLines = !cancel->load() && table.isEmpty()
                ? textLines(image, tableRegions, nativeStrokes).mid(0, maxTableRows) : QVariantList{};
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
                            crops.append({cell, &tableRegions, tableMargin, true});
                    }
                } else {
                    cells.clear();
                    appendLines(tableLines, tableRegions, tableMargin);
                }
                const auto tableCrops = crops.size();
                appendLines(proseLines, textRegions, proseMargin);
                const auto labelStart = crops.size();
                const auto labelJobs = rasterDiagramLabels(diagram, diagramLines);
                appendLines(labelJobs, diagramRegions, diagramLabelMargin);
                if (!crops.isEmpty() && !cancel->load()) {
                    const QImage detail = renderPage(content, background, rasterOcrSize);
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
