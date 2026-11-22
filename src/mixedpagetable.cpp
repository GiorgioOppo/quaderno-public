#include "mixedpagetable.h"

#include "inkrender.h"

#include <algorithm>
#include <vector>

namespace {
constexpr int maximumSide = 1000;
constexpr int maximumRows = 12;
constexpr int maximumColumns = 8;
constexpr int inkThreshold = 145;

struct Rule {
    int at = 0;
    int first = -1;
    int last = -1;
    int dark = 0;
    int centered = 0;

    int length() const { return last - first + 1; }
    bool present() const { return first >= 0; }
};

int bandRadius(int span)
{
    return std::clamp(span / 55, 2, 9);
}

QList<Rule> findRules(const QImage &ink, Qt::Orientation orientation)
{
    const bool vertical = orientation == Qt::Vertical;
    const int across = vertical ? ink.width() : ink.height();
    const int along = vertical ? ink.height() : ink.width();
    const int stride = across + 1;
    const int radius = bandRadius(across);
    const int minimumSpan = qMax(24, along * 52 / 100);
    const int toleratedGap = qMax(3, along / 30);
    std::vector<int> prefix(stride * along, 0);
    for (int step = 0; step < along; ++step) {
        int *sums = prefix.data() + step * stride;
        for (int offset = 0; offset < across; ++offset) {
            const int pixel = vertical ? ink.constScanLine(step)[offset] : ink.constScanLine(offset)[step];
            sums[offset + 1] = sums[offset] + (pixel < inkThreshold);
        }
    }

    QList<Rule> rules;
    Rule representative;
    int previous = -2;
    for (int at = 0; at < across; ++at) {
        const int left = qMax(0, at - radius);
        const int right = qMin(across, at + radius + 1);
        Rule current, best;
        const auto endRun = [&] {
            if (current.present() && current.length() >= minimumSpan
                && current.dark >= current.length() * .82
                && (!best.present() || current.dark > best.dark))
                best = current;
            current = {};
        };
        for (int step = 0; step < along; ++step) {
            const int *sums = prefix.data() + step * stride;
            if (sums[right] != sums[left]) {
                if (current.present() && step - current.last > toleratedGap + 1)
                    endRun();
                if (!current.present())
                    current.first = step;
                current.last = step;
                ++current.dark;
                current.centered += sums[at + 1] != sums[at];
            }
        }
        endRun();
        if (!best.present())
            continue;
        best.at = at;
        if (at > previous + 1 && representative.present()) {
            rules.append(representative);
            representative = {};
        }
        if (!representative.present() || best.centered > representative.centered
            || (best.centered == representative.centered && best.dark > representative.dark))
            representative = best;
        previous = at;
    }
    if (representative.present())
        rules.append(representative);
    return rules;
}

bool consistentRules(const QList<Rule> &rules, const QList<Rule> &cross, int minimumGap)
{
    const int first = cross.constFirst().at;
    const int last = cross.constLast().at;
    const int endTolerance = qMax(4, (last - first) / 14);
    int previous = rules.constFirst().at - minimumGap;
    for (const Rule &rule : rules) {
        if (rule.at - previous < minimumGap || rule.first > first + endTolerance
            || rule.last < last - endTolerance)
            return false;
        previous = rule.at;
    }
    return true;
}
}

QList<QList<QRectF>> MixedPageTable::cells(const QImage &image, const QRectF &selection)
{
    if (image.isNull() || selection.width() <= 0 || selection.height() <= 0
        || !QRectF(0, 0, 1, 1).contains(selection))
        return {};
    const QRect pixels = QRectF(selection.x() * image.width(), selection.y() * image.height(),
        selection.width() * image.width(), selection.height() * image.height()).toAlignedRect().intersected(image.rect());
    if (pixels.width() < 44 || pixels.height() < 38)
        return {};

    QImage ink = image.copy(pixels).convertToFormat(QImage::Format_RGB32);
    if (qMax(ink.width(), ink.height()) > maximumSide)
        ink = ink.scaled(maximumSide, maximumSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ink = ink.convertToFormat(QImage::Format_Grayscale8);
    const auto columns = findRules(ink, Qt::Vertical);
    if (columns.size() < 3 || columns.size() > maximumColumns + 1)
        return {};
    const auto rows = findRules(ink, Qt::Horizontal);
    if (rows.size() < 2 || rows.size() > maximumRows + 1)
        return {};

    const int xInset = qMax(2, bandRadius(ink.width()) / 2);
    const int yInset = qMax(2, bandRadius(ink.height()) / 2);
    if (!consistentRules(columns, rows, qMax(14, 4 * xInset))
        || !consistentRules(rows, columns, qMax(14, 4 * yInset)))
        return {};

    const double xScale = double(pixels.width()) / (double(ink.width()) * image.width());
    const double yScale = double(pixels.height()) / (double(ink.height()) * image.height());
    const double xOffset = double(pixels.x()) / image.width();
    const double yOffset = double(pixels.y()) / image.height();
    QList<QList<QRectF>> result;
    for (qsizetype row = 1; row < rows.size(); ++row) {
        QList<QRectF> line;
        const int top = rows.at(row - 1).at + yInset;
        const int bottom = rows.at(row).at - yInset;
        for (qsizetype column = 1; column < columns.size(); ++column) {
            const int left = columns.at(column - 1).at + xInset;
            const int right = columns.at(column).at - xInset;
            line.append(QRectF(xOffset + left * xScale, yOffset + top * yScale,
                (right - left) * xScale, (bottom - top) * yScale));
        }
        result.append(line);
    }
    return result;
}

QVariantList MixedPageTable::cellInk(const QList<QList<QRectF>> &grid, const QVariantList &strokes,
                                   const QPainterPath &contour)
{
    if (grid.isEmpty() || grid.first().isEmpty())
        return {};
    const qsizetype columns = grid.first().size();
    QList<QRectF> interiors;
    for (const auto &row : grid)
        interiors.append(row);

    QList<QVariantList> grouped(interiors.size());
    for (const auto &entry : strokes) {
        const auto stroke = entry.toMap();
        const auto tool = stroke.value(QStringLiteral("tool")).toString();
        if (!tool.isEmpty() && tool != QStringLiteral("pen") && tool != QStringLiteral("pencil")
            && tool != QStringLiteral("marker"))
            continue;
        const auto points = storedInkPoints(stroke);
        if (points.isEmpty())
            continue;
        const auto cell = std::find_if(interiors.cbegin(), interiors.cend(), [&points](const QRectF &inside) {
            return inside.contains(points.first().position);
        });
        if (cell == interiors.cend() || !std::all_of(points.cbegin(), points.cend(), [&](const InkPoint &point) {
            return cell->contains(point.position) && contour.contains(point.position);
        }))
            continue;
        grouped[cell - interiors.cbegin()].append(entry);
    }

    QVariantList jobs;
    for (qsizetype index = 0; index < grouped.size(); ++index) {
        if (!grouped.at(index).isEmpty()) {
            jobs.append(QVariantMap{{QStringLiteral("row"), int(index / columns)},
                {QStringLiteral("column"), int(index % columns)}, {QStringLiteral("strokes"), grouped.at(index)}});
        }
    }
    return jobs;
}
