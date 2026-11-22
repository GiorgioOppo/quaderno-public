#include "mixedpagetextink.h"

#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QtAlgorithms>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {
constexpr int maximumSide = 1200;
constexpr int maximumRows = 48;
constexpr int maximumStrokesPerRow = 120; // The local recognizer accepts up to 128.
constexpr int maximumPointsPerStroke = 120;
constexpr int luminanceThreshold = 145;
constexpr quint8 horizontalRule = 1;
constexpr quint8 verticalRule = 2;

// Clockwise from north, shared by thinning and the stroke graph.
constexpr std::array<QPoint, 8> directions{{{0, -1}, {1, -1}, {1, 0}, {1, 1},
                                             {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};

struct Mask {
    int width;
    int height;
    int stride;
    std::array<int, 8> offsets;
    std::vector<quint8> pixels;

    QPoint point(int index) const { return {index % stride - 1, index / stride - 1}; }

    unsigned int neighbors(int position) const
    {
        unsigned int bits = 0;
        for (int direction = 0; direction < int(offsets.size()); ++direction)
            bits |= unsigned(pixels[position + offsets[direction]]) << direction;
        return bits;
    }
};

struct Component {
    QRect bounds;
    std::vector<int> pixels;
};

struct Row {
    QRect bounds;
    std::vector<std::size_t> components;
    double center = 0;
    int letters = 0;
};

Mask inkMask(const QImage &source)
{
    QImage image = source;
    if (std::max(image.width(), image.height()) > maximumSide)
        image = image.scaled(maximumSide, maximumSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    image = image.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();
    const int stride = width + 2;
    Mask mask{width, height, stride, {}, std::vector<quint8>(stride * (height + 2))};
    for (int i = 0; i < int(directions.size()); ++i)
        mask.offsets[i] = directions[i].x() + directions[i].y() * stride;
    for (int y = 0; y < height; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            const int alpha = qAlpha(line[x]);
            const int luminance = (qGray(line[x]) * alpha + 255 * (255 - alpha)) / 255;
            mask.pixels[(y + 1) * stride + x + 1] = luminance < luminanceThreshold;
        }
    }
    return mask;
}

std::vector<quint8> removeRules(Mask &mask, int minimumLength)
{
    // A narrow band tolerates the drift of a ruled line drawn by hand. Detect
    // both axes before changing pixels so table intersections remain visible.
    constexpr int radius = 2;
    std::vector<quint8> discard(mask.pixels.size());
    const auto scan = [&](int length, int lines, int pitch, int linePitch, quint8 direction) {
        std::vector<int> support(length + 1);
        for (int side = 1; side <= std::min(lines, radius); ++side) {
            for (int i = 1; i <= length; ++i)
                support[i] += mask.pixels[side * linePitch + i * pitch];
        }
        for (int line = 1; line <= lines; ++line) {
            const int enter = line + radius;
            const int leave = line - radius - 1;
            for (int i = 1; i <= length; ++i) {
                if (enter <= lines)
                    support[i] += mask.pixels[enter * linePitch + i * pitch];
                if (leave > 0)
                    support[i] -= mask.pixels[leave * linePitch + i * pitch];
            }
            int first = 0;
            int last = 0;
            for (int position = 1; position <= length + 3; ++position) {
                if (position <= length && support[position]) {
                    if (!first)
                        first = position;
                    last = position;
                } else if (first && position - last > 2) {
                    if (last - first + 1 >= minimumLength) {
                        for (int i = first; i <= last; ++i)
                            discard[line * linePitch + i * pitch] |= direction;
                    }
                    first = 0;
                }
            }
        }
    };
    scan(mask.width, mask.height, 1, mask.stride, horizontalRule);
    scan(mask.height, mask.width, mask.stride, 1, verticalRule);
    for (int i = 0; i < int(discard.size()); ++i) {
        if (discard[i])
            mask.pixels[i] = 0;
    }
    return discard;
}

bool thinningCandidate(unsigned int neighbors, int phase)
{
    const auto count = qPopulationCount(neighbors);
    if (count < 2 || count > 6)
        return false;
    const unsigned int next = (neighbors >> 1) | ((neighbors & 1) << 7);
    if (qPopulationCount(~neighbors & next) != 1)
        return false;
    const unsigned int first = phase == 0 ? 0x15 : 0x45; // N E S, or N E W.
    const unsigned int second = phase == 0 ? 0x54 : 0x51; // E S W, or N S W.
    return (neighbors & first) != first && (neighbors & second) != second;
}

void thin(Mask &mask)
{
    std::vector<int> ink;
    std::vector<int> discard;
    for (int i = 0; i < int(mask.pixels.size()); ++i) {
        if (mask.pixels[i])
            ink.push_back(i);
    }
    // The two alternating Zhang–Suen passes preserve connected letter shapes.
    for (int round = 0; round < 24; ++round) {
        const auto before = ink.size();
        for (int phase = 0; phase < 2; ++phase) {
            discard.clear();
            for (const int index : ink) {
                if (thinningCandidate(mask.neighbors(index), phase))
                    discard.push_back(index);
            }
            for (const int index : discard)
                mask.pixels[index] = 0;
            ink.erase(std::remove_if(ink.begin(), ink.end(), [&](int index) { return !mask.pixels[index]; }), ink.end());
        }
        if (ink.size() == before)
            break;
    }
}

std::vector<quint8> strokeGraph(const Mask &mask)
{
    std::vector<quint8> graph(mask.pixels.size());
    for (int i = 0; i < int(mask.pixels.size()); ++i) {
        if (!mask.pixels[i])
            continue;
        const auto neighbors = mask.neighbors(i);
        auto edges = neighbors;
        // At a right angle a diagonal shortcut would create a false branch.
        for (int direction = 1; direction < 8; direction += 2) {
            const auto shortcuts = (1u << (direction - 1)) | (1u << ((direction + 1) % 8));
            if (neighbors & shortcuts)
                edges &= ~(1u << direction);
        }
        graph[i] = quint8(edges);
    }
    return graph;
}

bool residualRule(const Mask &mask, const std::vector<quint8> &rules, const QRect &bounds, int longer, int shorter)
{
    // A bent endpoint of a ruled line can survive the long-run detector.
    // Require a removed rule along the same axis, near the endpoint, so an
    // isolated vertical glyph or a cell with a single digit remains valid.
    if (longer < 4 || longer < 4 * shorter)
        return false;
    const bool vertical = bounds.height() > bounds.width();
    const int length = vertical ? mask.height : mask.width;
    const int lines = vertical ? mask.width : mask.height;
    const int first = vertical ? bounds.top() : bounds.left();
    const int last = vertical ? bounds.bottom() : bounds.right();
    const int alignment = vertical ? bounds.center().x() : bounds.center().y();
    const int pitch = vertical ? mask.stride : 1;
    const int linePitch = vertical ? 1 : mask.stride;
    const quint8 direction = vertical ? verticalRule : horizontalRule;
    const auto extends = [&](int endpoint, int step) {
        for (int side = std::max(0, alignment - 2); side <= std::min(lines - 1, alignment + 2); ++side) {
            int count = 0;
            for (int distance = 0; distance < 16; ++distance) {
                const int position = endpoint + distance * step;
                if (position < 0 || position >= length)
                    break;
                const int index = (side + 1) * linePitch + (position + 1) * pitch;
                count = (rules[index] & direction) ? count + 1 : 0;
                if (count >= 8)
                    return true;
            }
        }
        return false;
    };
    return extends(first, -1) || extends(last, 1);
}

std::vector<Component> letterComponents(const Mask &mask, const std::vector<quint8> &graph,
                                        const std::vector<quint8> &rules, int ruleLength)
{
    std::vector<quint8> visited(mask.pixels.size());
    std::vector<Component> components;
    Component candidate;
    for (int start = 0; start < int(mask.pixels.size()); ++start) {
        if (!mask.pixels[start] || visited[start])
            continue;
        visited[start] = 1;
        candidate.bounds = QRect(mask.point(start), QSize(1, 1));
        candidate.pixels.clear();
        candidate.pixels.push_back(start);
        for (std::size_t i = 0; i < candidate.pixels.size(); ++i) {
            const int index = candidate.pixels[i];
            candidate.bounds |= QRect(mask.point(index), QSize(1, 1));
            for (unsigned int edges = graph[index]; edges; edges &= edges - 1) {
                const int next = index + mask.offsets[qCountTrailingZeroBits(edges)];
                if (visited[next])
                    continue;
                visited[next] = 1;
                candidate.pixels.push_back(next);
            }
        }
        const int width = candidate.bounds.width();
        const int height = candidate.bounds.height();
        const int longer = std::max(width, height);
        const int shorter = std::min(width, height);
        if (longer >= ruleLength && (longer > 5 * shorter
            || (shorter > ruleLength && longer > 2 * ruleLength)))
            continue;
        if (residualRule(mask, rules, candidate.bounds, longer, shorter))
            continue;
        components.push_back(candidate);
    }
    return components;
}

std::vector<Row> textRows(const std::vector<Component> &components)
{
    std::vector<int> heights;
    for (const auto &component : components) {
        if (component.bounds.height() >= 4)
            heights.push_back(component.bounds.height());
    }
    if (heights.empty())
        return {};
    const auto middle = heights.begin() + heights.size() / 2;
    std::nth_element(heights.begin(), middle, heights.end());
    const double letterHeight = *middle;
    const double tolerance = std::max(4.0, letterHeight * .78);
    std::vector<std::size_t> letters;
    std::vector<std::size_t> marks;
    for (std::size_t i = 0; i < components.size(); ++i) {
        auto &list = components[i].bounds.height() >= std::max(4.0, letterHeight * .38) ? letters : marks;
        list.push_back(i);
    }
    const auto center = [&](std::size_t i) { return QRectF(components[i].bounds).center().y(); };
    std::stable_sort(letters.begin(), letters.end(), [&](std::size_t first, std::size_t second) {
        return center(first) < center(second);
    });

    std::vector<Row> rows;
    for (const auto index : letters) {
        if (rows.empty() || center(index) - rows.back().center > tolerance)
            rows.push_back({{}, {}, center(index), 0});
        auto &row = rows.back();
        row.center += (center(index) - row.center) / ++row.letters;
        row.bounds |= components[index].bounds;
        row.components.push_back(index);
    }
    // Small marks do not set a baseline. Attach nearby dots, punctuation and
    // accents after the lines are known, and drop isolated raster speckles.
    for (const auto index : marks) {
        const auto &bounds = components[index].bounds;
        auto nearest = rows.end();
        double best = std::numeric_limits<double>::infinity();
        for (auto row = rows.begin(); row != rows.end(); ++row) {
            const double gap = std::max({row->bounds.left() - bounds.right(), bounds.left() - row->bounds.right(), 0});
            const double distance = std::abs(center(index) - row->center);
            if (gap > letterHeight || distance > letterHeight * 1.2 || distance + .5 * gap >= best)
                continue;
            nearest = row;
            best = distance + .5 * gap;
        }
        if (nearest != rows.end()) {
            nearest->bounds |= bounds;
            nearest->components.push_back(index);
        }
    }
    return rows;
}

int nextDirection(unsigned int edges, int previous)
{
    if (previous < 0)
        return qCountTrailingZeroBits(edges);
    for (const int turn : {0, 1, 7, 2, 6, 3, 5, 4}) {
        const int direction = (previous + turn) % 8;
        if (edges & (1u << direction))
            return direction;
    }
    return -1;
}

QVariantMap penStroke(const Mask &mask, const std::vector<int> &path)
{
    const auto first = mask.point(path.front());
    const auto last = mask.point(path.back());
    const bool reverse = std::abs(first.x() - last.x()) >= std::abs(first.y() - last.y())
        ? first.x() > last.x() : first.y() > last.y();
    const int count = std::min(maximumPointsPerStroke, int(path.size()));
    QVariantList points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int sample = count == 1 ? 0 : i * (int(path.size()) - 1) / (count - 1);
        const auto point = mask.point(path[reverse ? path.size() - 1 - sample : sample]);
        points.append(QVariantMap{
            {QStringLiteral("x"), (point.x() + .5) / mask.width},
            {QStringLiteral("y"), (point.y() + .5) / mask.height},
            {QStringLiteral("p"), .55},
        });
    }
    return {{QStringLiteral("tool"), QStringLiteral("pen")}, {QStringLiteral("points"), points}};
}

void appendStrokes(const Mask &mask, std::vector<quint8> &graph, std::vector<int> &pixels, QVariantList &strokes)
{
    if (pixels.size() == 1) {
        strokes.append(penStroke(mask, pixels));
        return;
    }
    std::stable_sort(pixels.begin(), pixels.end(), [&](int first, int second) {
        const auto a = mask.point(first);
        const auto b = mask.point(second);
        return a.x() != b.x() ? a.x() < b.x() : a.y() < b.y();
    });
    std::vector<int> path;
    const auto trace = [&](int start) {
        path.clear();
        path.push_back(start);
        int previous = -1;
        int index = start;
        while (graph[index]) {
            const int direction = nextDirection(graph[index], previous);
            const int next = index + mask.offsets[direction];
            graph[index] &= ~(1u << direction);
            graph[next] &= ~(1u << ((direction + 4) % 8));
            path.push_back(next);
            index = next;
            previous = direction;
        }
        if (path.size() > 2 || pixels.size() <= 2)
            strokes.append(penStroke(mask, path));
    };
    // Start open strokes at endpoints, then recover any closed loops and
    // remaining branches. Each graph edge is visited at most once.
    for (const int index : pixels) {
        if (strokes.size() >= maximumStrokesPerRow)
            return;
        if (qPopulationCount(graph[index]) == 1)
            trace(index);
    }
    for (const int index : pixels) {
        while (graph[index] && strokes.size() < maximumStrokesPerRow)
            trace(index);
    }
}
}

QVariantList MixedPageTextInk::lines(const QImage &source)
{
    if (source.isNull())
        return {};
    auto mask = inkMask(source);
    if (mask.width < 3 || mask.height < 3)
        return {};
    const int ruleLength = std::max(36, std::min(mask.width, mask.height) / 16);
    const auto rules = removeRules(mask, ruleLength);
    thin(mask);
    auto graph = strokeGraph(mask);
    auto components = letterComponents(mask, graph, rules, ruleLength);
    auto rows = textRows(components);

    QVariantList result;
    for (auto &row : rows) {
        if (result.size() >= maximumRows)
            break;
        std::stable_sort(row.components.begin(), row.components.end(), [&](std::size_t a, std::size_t b) {
            return components[a].bounds.left() < components[b].bounds.left();
        });
        QVariantList strokes;
        for (const auto index : row.components) {
            if (strokes.size() >= maximumStrokesPerRow)
                break;
            appendStrokes(mask, graph, components[index].pixels, strokes);
        }
        const QRectF bounds(double(row.bounds.x()) / mask.width, double(row.bounds.y()) / mask.height,
                            double(row.bounds.width()) / mask.width, double(row.bounds.height()) / mask.height);
        if (!strokes.isEmpty())
            result.append(QVariantMap{{QStringLiteral("strokes"), strokes}, {QStringLiteral("bounds"), bounds}});
    }
    return result;
}
