#include "mixedpagegeometry.h"

#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QVariantList>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {
constexpr int maximumSide = 1100;
constexpr int maximumNodes = 64;
constexpr int maximumEdges = 128;

struct Mask {
    int width = 0;
    int height = 0;
    std::vector<quint8> bits;
};

struct Shape {
    QRect bounds;
    QString kind;
};

Mask inkMask(const QImage &source)
{
    QImage image = source;
    if (qMax(image.width(), image.height()) > maximumSide)
        image = image.scaled(maximumSide, maximumSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    image = image.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();
    if (width < 30 || height < 30)
        return {};

    Mask mask{width, height, std::vector<quint8>(width * height)};
    for (int y = 0; y < height; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            const int alpha = qAlpha(line[x]);
            const int luminance = (qGray(line[x]) * alpha + 255 * (255 - alpha)) / 255;
            mask.bits[y * width + x] = luminance < 145;
        }
    }
    return mask;
}

// A sliding count in each axis grows dark pixels without a radius-squared
// search. This reconnects outline corners separated by short pen lifts.
Mask growInk(const Mask &mask, int radius)
{
    const int width = mask.width;
    const int height = mask.height;
    Mask grown{width, height, std::vector<quint8>(mask.bits.size())};
    std::vector<quint8> horizontal(mask.bits.size());
    for (int y = 0; y < height; ++y) {
        const int offset = y * width;
        int count = 0;
        for (int x = -radius; x < width; ++x) {
            if (x + radius < width)
                count += mask.bits[offset + x + radius];
            if (x - radius > 0)
                count -= mask.bits[offset + x - radius - 1];
            if (x >= 0)
                horizontal[offset + x] = count > 0;
        }
    }
    for (int x = 0; x < width; ++x) {
        int count = 0;
        for (int y = -radius; y < height; ++y) {
            if (y + radius < height)
                count += horizontal[(y + radius) * width + x];
            if (y - radius > 0)
                count -= horizontal[(y - radius - 1) * width + x];
            if (y >= 0)
                grown.bits[y * width + x] = count > 0;
        }
    }
    return grown;
}

template<typename Visit>
void components(Mask mask, quint8 value, Visit visit)
{
    const int width = mask.width;
    const int height = mask.height;
    std::vector<int> pixels;
    const auto add = [&](int index) {
        if (mask.bits[index] == value) {
            mask.bits[index] ^= 1;
            pixels.push_back(index);
        }
    };
    for (int start = 0; start < int(mask.bits.size()); ++start) {
        if (mask.bits[start] != value)
            continue;
        pixels.clear();
        add(start);
        int left = start % width;
        int right = left;
        int top = start / width;
        int bottom = top;
        for (std::size_t cursor = 0; cursor < pixels.size(); ++cursor) {
            const int offset = pixels[cursor];
            const int x = offset % width;
            const int y = offset / width;
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = qMax(bottom, y);
            if (x > 0)
                add(offset - 1);
            if (x + 1 < width)
                add(offset + 1);
            if (y > 0)
                add(offset - width);
            if (y + 1 < height)
                add(offset + width);
        }
        visit(QRect(QPoint(left, top), QPoint(right, bottom)), pixels);
    }
}

double area(const QRect &rect)
{
    return double(rect.width()) * rect.height();
}

bool almostSameShape(const QRect &a, const QRect &b)
{
    const double smaller = qMin(area(a), area(b));
    return qMax(area(a), area(b)) < smaller * 1.65 && area(a.intersected(b)) > smaller * .8;
}

QString shapeKind(const QRect &hole, const std::vector<int> &pixels, int imageWidth)
{
    const std::array rows{hole.height() / 10, hole.height() / 5, hole.height() / 2,
        hole.height() * 4 / 5, hole.height() * 9 / 10};
    std::array<int, rows.size()> widths{};
    for (const int pixel : pixels) {
        const int relative = pixel / imageWidth - hole.y();
        for (std::size_t index = 0; index < rows.size(); ++index) {
            if (relative == rows[index])
                ++widths[index];
        }
    }
    const double density = double(pixels.size()) / area(hole);
    const double middle = widths[2];
    const double endWidth = (widths[1] + widths[3]) / 2.;
    const int widerEnd = qMax(widths[1], widths[3]);
    if (density < .68 && endWidth < middle * .72 && widerEnd < middle * .79)
        return QStringLiteral("decision");
    const int widerCorner = qMax(widths[0], widths[4]);
    if (density < .94 && widerEnd < middle * .97 && widerCorner < middle * .90)
        return QStringLiteral("terminal");
    return QStringLiteral("process");
}

void completeOutlines(const Mask &ink, QList<Shape> &shapes, int radius)
{
    const double canvasArea = double(ink.width) * ink.height;
    components(growInk(ink, radius), 1, [&](const QRect &grown, const std::vector<int> &pixels) {
        const QRect bounds = grown.adjusted(radius, radius, -radius, -radius);
        if (bounds.width() < 30 || bounds.height() < 30 || area(bounds) < canvasArea * .025
            || area(bounds) > canvasArea * .8 || pixels.size() > area(bounds) * .44)
            return;
        const auto nested = std::count_if(shapes.cbegin(), shapes.cend(), [&](const Shape &shape) {
            return area(shape.bounds) < area(bounds) / 4 && bounds.contains(shape.bounds.center());
        });
        if (nested < 2)
            return;

        // A broken enclosure still has evidence along most of its perimeter.
        // Lines joining child nodes do not have four supported sides.
        const int tolerance = qMax(radius + 2, qMin(bounds.width(), bounds.height()) / 10);
        std::vector<quint8> top(grown.width()), bottom(grown.width());
        std::vector<quint8> left(grown.height()), right(grown.height());
        for (const int pixel : pixels) {
            const int x = pixel % ink.width;
            const int y = pixel / ink.width;
            if (y <= bounds.top() + tolerance)
                top[x - grown.x()] = 1;
            if (y >= bounds.bottom() - tolerance)
                bottom[x - grown.x()] = 1;
            if (x <= bounds.left() + tolerance)
                left[y - grown.y()] = 1;
            if (x >= bounds.right() - tolerance)
                right[y - grown.y()] = 1;
        }
        const auto coverage = [](const std::vector<quint8> &side) {
            return double(std::count(side.cbegin(), side.cend(), 1)) / side.size();
        };
        const std::array sides{coverage(top), coverage(bottom), coverage(left), coverage(right)};
        if (std::count_if(sides.cbegin(), sides.cend(), [](double side) { return side >= .55; }) < 3
            || *std::min_element(sides.cbegin(), sides.cend()) < .18)
            return;

        auto matching = std::find_if(shapes.begin(), shapes.end(), [&](const Shape &shape) {
            const double enclosed = area(bounds.intersected(shape.bounds));
            return bounds.width() <= shape.bounds.width() * 1.33
                && area(shape.bounds) >= area(bounds) * .43
                && enclosed >= area(shape.bounds) * .8;
        });
        if (matching != shapes.end()) {
            matching->bounds = matching->bounds.united(bounds);
            matching->kind = QStringLiteral("process");
        } else if (shapes.size() < maximumNodes) {
            shapes.append({bounds, QStringLiteral("process")});
        }
    });
}

void expandEnclosures(const Mask &ink, QList<Shape> &shapes)
{
    const QRect frame(0, 0, ink.width, ink.height);
    for (Shape &shape : shapes) {
        const QRect original = shape.bounds;
        if (area(original) < area(frame) * .025)
            continue;
        const auto nested = std::count_if(shapes.cbegin(), shapes.cend(), [&](const Shape &child) {
            return area(child.bounds) < area(original) / 4 && original.contains(child.bounds.center());
        });
        if (nested < 2)
            continue;
        shape.kind = QStringLiteral("process");

        // A horizontal divider sometimes makes the lower half of an enclosure
        // the only closed cavity. Recover the upper edge when both sidewalls
        // and a wide top line independently extend beyond that divider.
        const int band = qMax(3, qMin(original.width(), original.height()) / 12);
        const int limit = qMin(ink.height / 9, original.height() * 4 / 5);
        const auto dark = [&ink](int x, int y) { return ink.bits[y * ink.width + x] != 0; };
        for (int y = qMax(band, original.top() - limit); y < original.top() - band * 2; y += 2) {
            int roof = 0;
            for (int x = original.left(); x <= original.right(); x += 2) {
                for (int test = y - band; test <= y + band; ++test) {
                    if (dark(x, test)) {
                        ++roof;
                        break;
                    }
                }
            }
            if (roof < original.width() * .36)
                continue;
            const auto wall = [&](int center) {
                int present = 0;
                for (int row = y + band; row < original.top(); row += 2) {
                    for (int x = qMax(0, center - band * 2); x <= qMin(frame.right(), center + band * 2); ++x) {
                        if (dark(x, row)) {
                            ++present;
                            break;
                        }
                    }
                }
                return present;
            };
            const int needed = (original.top() - y - band) / 4;
            if (wall(original.left()) >= needed && wall(original.right()) >= needed) {
                shape.bounds.setTop(y);
                break;
            }
        }
    }
}

void removeEnclosurePanels(QList<Shape> &shapes)
{
    const QList<Shape> candidates = shapes;
    shapes.removeIf([&](const Shape &shape) {
        const QRect &panel = shape.bounds;
        return std::any_of(candidates.cbegin(), candidates.cend(), [&](const Shape &other) {
            const QRect &box = other.bounds;
            if (area(panel) >= area(box) * .68)
                return false;
            const QRect overlap = box.intersected(panel);
            const bool horizontal = panel.width() >= box.width() * .8 && area(overlap) >= area(panel) * .9
                && (qAbs(panel.top() - box.top()) <= box.height() * .10
                    || qAbs(panel.bottom() - box.bottom()) <= box.height() * .10);
            const bool edgeStrip = panel.width() <= box.width() * .045 && panel.height() >= box.height() * .36
                && overlap.height() >= panel.height() * .75
                && qMin(qAbs(panel.center().x() - box.left()), qAbs(panel.center().x() - box.right())) <= box.width() * .04;
            if (!horizontal && !edgeStrip)
                return false;
            return std::count_if(candidates.cbegin(), candidates.cend(), [&](const Shape &child) {
                return child.bounds != panel && area(child.bounds) < area(box) / 4
                    && box.contains(child.bounds.center());
            }) >= 2;
        });
    });
}

bool hasStripEdges(const Mask &ink, const QRect &box)
{
    const int tolerance = std::clamp(box.width() / 3, 3, 14);
    const auto support = [&](bool horizontal, bool last) {
        const int start = horizontal ? box.left() : box.top();
        const int end = horizontal ? box.right() : box.bottom();
        const int edge = horizontal ? (last ? box.bottom() : box.top()) : (last ? box.right() : box.left());
        const int extent = horizontal ? ink.height : ink.width;
        int supported = 0;
        for (int along = start; along <= end; ++along) {
            for (int cross = qMax(0, edge - tolerance); cross <= qMin(extent - 1, edge + tolerance); ++cross) {
                const int index = horizontal ? cross * ink.width + along : along * ink.width + cross;
                if (ink.bits[index]) {
                    ++supported;
                    break;
                }
            }
        }
        return double(supported) / (end - start + 1);
    };
    return support(false, false) >= .67 && support(false, true) >= .67
        && support(true, false) >= .55 && support(true, true) >= .55;
}

QRect extendStripToCaps(const Mask &ink, QRect strip)
{
    // A letter across a tight enclosure may cut its white cavity short.
    // Search a short distance beyond either end for the actual closing bar.
    const int left = strip.left() + strip.width() / 6;
    const int right = strip.right() - strip.width() / 6;
    const int search = qMin(strip.width(), strip.height() / 6);
    const auto cap = [&](int edge, int direction) {
        for (int distance = 1; distance <= search; ++distance) {
            const int row = edge + distance * direction;
            if (row < 2 || row + 2 >= ink.height)
                break;
            int supported = 0;
            for (int x = left; x <= right; ++x) {
                for (int y = row - 2; y <= row + 2; ++y) {
                    if (ink.bits[y * ink.width + x]) {
                        ++supported;
                        break;
                    }
                }
            }
            if (supported >= (right - left + 1) * .68)
                return row;
        }
        return edge;
    };
    strip.setTop(cap(strip.top(), -1));
    strip.setBottom(cap(strip.bottom(), 1));
    return strip;
}

void keepNarrowTitleStrips(const Mask &ink, const QList<QRect> &candidates, QList<Shape> &shapes)
{
    const auto frames = shapes;
    for (auto strip : candidates) {
        if (shapes.size() >= maximumNodes)
            return;
        const bool attached = std::any_of(frames.cbegin(), frames.cend(), [&](const Shape &parent) {
            const auto &box = parent.bounds;
            if (parent.kind != QStringLiteral("process") || box.width() < strip.width() * 3
                || box.height() < strip.height() * .68)
                return false;
            const int overlap = qMin(box.bottom(), strip.bottom()) - qMax(box.top(), strip.top()) + 1;
            const int gap = qMin(qAbs(box.left() - strip.right()), qAbs(box.right() - strip.left()));
            return overlap >= strip.height() * .65 && gap <= qMax(10., strip.width() * 1.6);
        });
        if (!attached)
            continue;
        if (!hasStripEdges(ink, strip)) {
            strip = extendStripToCaps(ink, strip);
            if (!hasStripEdges(ink, strip))
                continue;
        }
        if (std::any_of(shapes.cbegin(), shapes.cend(), [&](const Shape &shape) {
            return almostSameShape(shape.bounds, strip);
        }))
            continue;
        // Part of a strip can already look like a normal rectangle when
        // dark letters split its white cavity. Keep the verified full strip.
        shapes.removeIf([&](const Shape &shape) {
            const auto &part = shape.bounds;
            const int capGap = qMin(qAbs(part.top() - strip.top()), qAbs(part.bottom() - strip.bottom()));
            return part.width() >= strip.width() * .8 && area(part) < area(strip) * .72
                && area(part.intersected(strip)) >= area(part) * .88
                && capGap <= qMax(4, strip.width() / 6);
        });
        shapes.append({strip, QStringLiteral("process")});
    }
}

QList<Shape> findShapes(const Mask &ink)
{
    QList<Shape> shapes;
    QList<QRect> titleStrips;
    const int minSide = qMin(ink.width, ink.height);
    const int minExtent = qMax(9, minSide / 66);
    const int minArea = qMax(75, ink.width * ink.height / 2100);
    const int scale = qMax(1, minSide / 260);
    const QRect frame(0, 0, ink.width, ink.height);
    const std::array radii{1, scale, scale * 2, scale * 3};
    int previous = 0;
    for (const int radius : radii) {
        if (radius == previous)
            continue;
        previous = radius;
        components(growInk(ink, radius), 0, [&](const QRect &hole, const std::vector<int> &pixels) {
            const double density = double(pixels.size()) / area(hole);
            const bool largeGap = radius > scale * 2;
            if (hole.x() == 0 || hole.y() == 0 || hole.right() == frame.right() || hole.bottom() == frame.bottom()
                || hole.width() + radius * 2 < minExtent || hole.height() + radius * 2 < minExtent
                || pixels.size() < std::size_t(qMax(50, minArea / 3)) || area(hole) > area(frame) * .85 || density < .39)
                return;
            const QRect bounds = hole.adjusted(-radius - 1, -radius - 1, radius + 1, radius + 1).intersected(frame);
            const double aspect = double(bounds.width()) / bounds.height();
            if (aspect >= .08 && aspect < .43 && bounds.height() > minSide * .075
                && bounds.width() >= minExtent && density >= .44 && titleStrips.size() < maximumNodes) {
                titleStrips.append(bounds);
            }
            const bool compactOutline = density >= .75 && qMin(bounds.width(), bounds.height()) >= minExtent * 1.5;
            if ((largeGap && (density < .53 || area(hole) < area(frame) * .018))
                || area(bounds) < minArea * (compactOutline ? 1.05 : 1.22) || aspect < .29
                || (aspect < .43 && bounds.height() > minSide * .075)
                || (aspect > 5.5 && area(bounds) < area(frame) * .04))
                return;
            if (std::none_of(shapes.cbegin(), shapes.cend(), [&](const Shape &candidate) {
                    return almostSameShape(candidate.bounds, bounds);
                }) && shapes.size() < maximumNodes)
                shapes.append({bounds, shapeKind(hole, pixels, ink.width)});
        });
    }
    completeOutlines(ink, shapes, scale);
    expandEnclosures(ink, shapes);
    removeEnclosurePanels(shapes);
    keepNarrowTitleStrips(ink, titleStrips, shapes);
    std::stable_sort(shapes.begin(), shapes.end(), [](const Shape &a, const Shape &b) {
        // Draw enclosing nodes first so they never obscure the boxes within.
        return area(a.bounds) > area(b.bounds);
    });
    return shapes;
}

double pointDistance(const QPointF &a, const QPointF &b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

double boundaryDistance(const QPointF &point, const QRect &rect)
{
    const double dx = std::max({0., rect.left() - point.x(), point.x() - rect.right()});
    const double dy = std::max({0., rect.top() - point.y(), point.y() - rect.bottom()});
    if (dx || dy)
        return std::hypot(dx, dy);
    return std::min({point.x() - rect.left(), rect.right() - point.x(),
                     point.y() - rect.top(), rect.bottom() - point.y()});
}

int closestShape(const QPointF &point, const QList<Shape> &shapes, double threshold)
{
    int selected = -1;
    for (int index = 0; index < shapes.size(); ++index) {
        const double distance = boundaryDistance(point, shapes[index].bounds);
        if (distance < threshold) {
            selected = index;
            threshold = distance;
        }
    }
    return selected;
}

void clearOutline(Mask &mask, const QRect &shape, int radius)
{
    const QRect expanded = shape.adjusted(-radius, -radius, radius, radius)
        .intersected(QRect(0, 0, mask.width, mask.height));
    const QRect interior = shape.adjusted(radius, radius, -radius, -radius);
    for (int y = expanded.top(); y <= expanded.bottom(); ++y) {
        for (int x = expanded.left(); x <= expanded.right(); ++x) {
            if (!interior.contains(x, y))
                mask.bits[y * mask.width + x] = 0;
        }
    }
}

struct Band {
    double minimum = std::numeric_limits<double>::max();
    double maximum = -std::numeric_limits<double>::max();
    void add(double value)
    {
        minimum = qMin(minimum, value);
        maximum = qMax(maximum, value);
    }
    double span() const { return qMax(0., maximum - minimum); }
};

constexpr int arrowSlices = 12;

bool broadArrowHead(const std::array<Band, arrowSlices> &slices, bool atStart, double length)
{
    const auto band = [&](int offset) -> const Band & {
        return slices[atStart ? offset : arrowSlices - 1 - offset];
    };
    const Band *peak = &band(1);
    for (int offset = 2; offset <= 4; ++offset) {
        if (band(offset).span() > peak->span())
            peak = &band(offset);
    }
    double shaftWidth = 0;
    for (int offset = 6; offset < arrowSlices; ++offset)
        shaftWidth = qMax(shaftWidth, band(offset).span());
    if (shaftWidth <= 0 || peak->span() < qMax(8., shaftWidth * 2)
        || peak->span() > length * .85 || band(0).span() > peak->span() * .8)
        return false;
    const double shaftCenter = (band(7).minimum + band(7).maximum) / 2;
    if (!(peak->minimum < shaftCenter - shaftWidth * .6 && peak->maximum > shaftCenter + shaftWidth * .6))
        return false;
    int broadSlices = 0;
    for (int offset = 1; offset <= 4; ++offset) {
        if (band(offset).span() >= qMax(7., shaftWidth * 1.5))
            ++broadSlices;
    }
    return broadSlices >= 2;
}

QVariantList findEdges(Mask ink, const QList<Shape> &shapes)
{
    const int width = ink.width;
    const int height = ink.height;
    const int minSide = qMin(width, height);
    if (shapes.size() < 2)
        return {};
    for (const Shape &shape : shapes)
        clearOutline(ink, shape.bounds, qMax(2, minSide / 200));

    QVariantList edges;
    QSet<QPair<int, int>> connections;
    components(growInk(ink, qMax(1, minSide / 650)), 1, [&](const QRect &extent, const std::vector<int> &pixels) {
        if (qMax(extent.width(), extent.height()) < qMax(15, minSide / 37)
            || pixels.size() > std::size_t(width * height / 30) || edges.size() >= maximumEdges)
            return;
        double xSum = 0;
        double ySum = 0;
        for (const int pixel : pixels) {
            xSum += pixel % width;
            ySum += pixel / width;
        }
        const QPointF center(xSum / pixels.size(), ySum / pixels.size());
        double xx = 0;
        double yy = 0;
        double xy = 0;
        for (const int pixel : pixels) {
            const double x = pixel % width - center.x();
            const double y = pixel / width - center.y();
            xx += x * x;
            yy += y * y;
            xy += x * y;
        }
        const double angle = std::atan2(2 * xy, xx - yy) / 2;
        const QPointF axis(std::cos(angle), std::sin(angle));
        const QPointF perpendicular(-axis.y(), axis.x());
        const double concentration = std::hypot(xx - yy, 2 * xy) / qMax(1., xx + yy);
        if (concentration < .46)
            return;
        Band length;
        for (const int pixel : pixels)
            length.add(QPointF::dotProduct(QPointF(pixel % width, pixel / width) - center, axis));
        if (length.span() < 1)
            return;
        Band first, middle, last;
        std::array<Band, arrowSlices> slices;
        for (const int pixel : pixels) {
            const QPointF delta = QPointF(pixel % width, pixel / width) - center;
            const double longitudinal = (QPointF::dotProduct(delta, axis) - length.minimum) / length.span();
            const double lateral = QPointF::dotProduct(delta, perpendicular);
            slices[std::clamp(int(longitudinal * arrowSlices), 0, arrowSlices - 1)].add(lateral);
            if (longitudinal < .26)
                first.add(lateral);
            else if (longitudinal > .74)
                last.add(lateral);
            else if (longitudinal > .36 && longitudinal < .64)
                middle.add(lateral);
        }
        const double head = qMax(first.span(), last.span());
        const double tail = qMin(first.span(), last.span());
        bool pointsForward = last.span() > first.span();
        const Band &tip = pointsForward ? last : first;
        const bool clearHead = head >= qMax(7., middle.span() * 1.25) && head >= tail * 1.24
            && middle.span() <= length.span() * .30
            && tip.minimum < middle.minimum - 1. && tip.maximum > middle.maximum + 1.;
        if (!clearHead) {
            // A hand-drawn triangular tip can occupy nearly half a short
            // connector. Compare its local width with the rest of the shaft.
            const bool startsWithHead = broadArrowHead(slices, true, length.span());
            const bool endsWithHead = broadArrowHead(slices, false, length.span());
            if (startsWithHead == endsWithHead)
                return;
            pointsForward = endsWithHead;
        }
        const QPointF start = center + axis * length.minimum;
        const QPointF end = center + axis * length.maximum;
        if (pointDistance(start, end) < qMax(16., minSide / 29.))
            return;
        const double endpointGap = qMin(minSide * .065, qMax(minSide * .018, length.span() * .38));
        const int from = closestShape(pointsForward ? start : end, shapes, endpointGap);
        const int to = closestShape(pointsForward ? end : start, shapes, endpointGap);
        if (from < 0 || to < 0 || from == to || connections.contains({from, to})
            || shapes[from].bounds.contains(shapes[to].bounds.center())
            || shapes[to].bounds.contains(shapes[from].bounds.center()))
            return;
        connections.insert({from, to});
        edges.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("e%1").arg(edges.size() + 1)},
                                 {QStringLiteral("from"), QStringLiteral("n%1").arg(from + 1)},
                                 {QStringLiteral("to"), QStringLiteral("n%1").arg(to + 1)},
                                 {QStringLiteral("text"), QString{}}});
    });
    return edges;
}
}

QVariantMap MixedPageGeometry::diagram(const QImage &image)
{
    const Mask ink = inkMask(image);
    if (ink.bits.empty())
        return {};
    const auto shapes = findShapes(ink);
    if (shapes.isEmpty())
        return {};
    QVariantList nodes;
    nodes.reserve(shapes.size());
    for (int index = 0; index < shapes.size(); ++index) {
        const QRectF box(shapes[index].bounds);
        nodes.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("n%1").arg(index + 1)},
                                 {QStringLiteral("shape"), shapes[index].kind},
                                 {QStringLiteral("text"), QString{}},
                                 {QStringLiteral("x"), box.x() / ink.width},
                                 {QStringLiteral("y"), box.y() / ink.height},
                                 {QStringLiteral("width"), box.width() / ink.width},
                                 {QStringLiteral("height"), box.height() / ink.height}});
    }
    return {{QStringLiteral("type"), QStringLiteral("diagram")},
            {QStringLiteral("x"), 0.0}, {QStringLiteral("y"), 0.0},
            {QStringLiteral("width"), 1.0}, {QStringLiteral("height"), 1.0},
            {QStringLiteral("nodes"), nodes},
            {QStringLiteral("edges"), findEdges(ink, shapes)}};
}
