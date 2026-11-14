#include "chartrecognizer.h"

#include "inkrender.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLineF>
#include <QPolygonF>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr qreal pageAspect = 4.0 / 3.0;
constexpr qsizetype maxChartPoints = 128;
constexpr qsizetype maxStrokes = 120;
constexpr qsizetype maxInkPoints = 100000;
constexpr qsizetype maxCandidateFrames = 32;
constexpr double maxAxisMagnitude = 1e12;

using Path = QList<QPointF>;

struct Axis {
    QPointF start;
    QPointF end;
    int stroke = -1;
};

struct Frame {
    QPointF origin;
    QPointF xAxis;
    QPointF yAxis;
    int xStroke = -1;
    int yStroke = -1;
    qreal score = 0;

    QPointF map(QPointF point) const;
};

struct Series {
    QString kind;
    Path points;
};

qreal length(QPointF point)
{
    return std::hypot(point.x(), point.y());
}

qreal cross(QPointF a, QPointF b)
{
    return a.x() * b.y() - a.y() * b.x();
}

QPointF Frame::map(QPointF point) const
{
    const QPointF fromOrigin = point - origin;
    const qreal area = cross(xAxis, yAxis);
    return {cross(fromOrigin, yAxis) / area, cross(xAxis, fromOrigin) / area};
}

qreal distanceToSegment(QPointF point, QPointF start, QPointF end)
{
    const QPointF delta = end - start;
    const qreal square = QPointF::dotProduct(delta, delta);
    if (square <= 0)
        return length(point - start);
    const qreal t = std::clamp(QPointF::dotProduct(point - start, delta) / square, 0.0, 1.0);
    return length(point - (start + t * delta));
}

Path simplify(const Path &path, qreal tolerance)
{
    if (path.size() < 3)
        return path;

    QList<QPair<qsizetype, qsizetype>> remaining{{0, path.size() - 1}};
    QList<bool> retained(path.size(), false);
    retained[0] = true;
    retained[path.size() - 1] = true;
    while (!remaining.isEmpty()) {
        const auto [start, end] = remaining.takeLast();
        qreal farthest = tolerance;
        qsizetype index = -1;
        for (qsizetype pos = start + 1; pos < end; ++pos) {
            const qreal error = distanceToSegment(path.at(pos), path.at(start), path.at(end));
            if (error > farthest) {
                farthest = error;
                index = pos;
            }
        }
        if (index >= 0) {
            retained[index] = true;
            remaining.append({start, index});
            remaining.append({index, end});
        }
    }

    Path result;
    for (qsizetype pos = 0; pos < path.size(); ++pos) {
        if (retained.at(pos))
            result.append(path.at(pos));
    }
    return result;
}

Path limitSamples(const Path &path, qsizetype limit)
{
    if (path.size() <= limit)
        return path;
    Path result;
    result.reserve(limit);
    for (qsizetype index = 0; index < limit; ++index)
        result.append(path.at(index * (path.size() - 1) / (limit - 1)));
    return result;
}

bool straight(const Path &path, qreal tolerance)
{
    const QPointF a = path.constFirst();
    const QPointF b = path.constLast();
    const qreal chord = length(b - a);
    if (chord < 0.035)
        return false;

    qreal travelled = 0;
    QPointF previous = a;
    for (const QPointF &point : path) {
        if (distanceToSegment(point, a, b) > tolerance)
            return false;
        travelled += length(point - previous);
        previous = point;
    }
    return travelled <= 1.20 * chord;
}

int orientation(QPointF delta)
{
    if (std::abs(delta.y()) <= 0.16 * std::abs(delta.x()))
        return 1;
    if (std::abs(delta.x()) <= 0.16 * std::abs(delta.y()))
        return -1;
    return 0;
}

void addAxis(QPointF first, QPointF second, int index, QList<Axis> &horizontal, QList<Axis> &vertical)
{
    const QPointF delta = second - first;
    if (length(delta) < 0.065)
        return;
    if (orientation(delta) == 1)
        horizontal.append(delta.x() > 0 ? Axis{first, second, index} : Axis{second, first, index});
    else if (orientation(delta) == -1)
        vertical.append(delta.y() < 0 ? Axis{first, second, index} : Axis{second, first, index});
}

QList<Frame> framesFor(const QList<Path> &paths)
{
    QList<Axis> horizontal;
    QList<Axis> vertical;
    for (int index = 0; index < paths.size(); ++index) {
        const Path &path = paths.at(index);
        if (path.size() < 2)
            continue;
        const qreal chord = length(path.constLast() - path.constFirst());
        if (straight(path, std::max(0.006, chord * 0.025))) {
            addAxis(path.constFirst(), path.constLast(), index, horizontal, vertical);
            continue;
        }

        const Path corners = simplify(path, 0.009);
        if (corners.size() != 3 || length(corners.at(1) - corners.at(0)) < 0.065
            || length(corners.at(2) - corners.at(1)) < 0.065)
            continue;
        const int first = orientation(corners.at(1) - corners.at(0));
        const int second = orientation(corners.at(2) - corners.at(1));
        if (first == 0 || first != -second)
            continue;
        addAxis(corners.at(0), corners.at(1), index, horizontal, vertical);
        addAxis(corners.at(1), corners.at(2), index, horizontal, vertical);
    }

    QList<Frame> frames;
    for (const Axis &horizontalAxis : horizontal) {
        for (const Axis &verticalAxis : vertical) {
            const QPointF xDelta = horizontalAxis.end - horizontalAxis.start;
            const QPointF yDelta = verticalAxis.end - verticalAxis.start;
            const qreal xLength = length(xDelta);
            const qreal yLength = length(yDelta);
            if (std::abs(QPointF::dotProduct(xDelta, yDelta)) > 0.22 * xLength * yLength)
                continue;

            const QPointF starts = verticalAxis.start - horizontalAxis.start;
            const qreal determinant = cross(xDelta, yDelta);
            const qreal alongX = cross(starts, yDelta) / determinant;
            const qreal alongY = cross(starts, xDelta) / determinant;
            // This import expects positive axes with their starts near the common origin.
            if (std::abs(alongX) > 0.14 || std::abs(alongY) > 0.14)
                continue;
            const QPointF origin = horizontalAxis.start + alongX * xDelta;
            frames.append({origin, horizontalAxis.end - origin, verticalAxis.end - origin,
                           horizontalAxis.stroke, verticalAxis.stroke,
                           xLength * yLength * (1 - std::abs(alongX) - std::abs(alongY))});
        }
    }
    std::sort(frames.begin(), frames.end(), [](const Frame &a, const Frame &b) { return a.score > b.score; });
    if (frames.size() > maxCandidateFrames)
        frames.resize(maxCandidateFrames);
    return frames;
}

QRectF bounds(const Path &path)
{
    return QPolygonF(path).boundingRect();
}

bool isAxisMark(const QRectF &box)
{
    return (box.width() <= 0.085 && box.height() <= 0.09)
        && (box.left() <= 0.05 || box.top() <= 0.05 || box.right() >= 0.975 || box.bottom() >= 0.975);
}

QPointF chartPoint(QPointF position)
{
    return {std::clamp(position.x(), 0.0, 1.0), std::clamp(position.y(), 0.0, 1.0)};
}

bool findBar(const Path &path, QPointF &bar)
{
    const QRectF box = bounds(path);
    if (box.width() < 0.025 || box.width() > 0.55 || box.height() < 0.07 || std::abs(box.top()) > 0.07)
        return false;

    const qreal xTolerance = std::max(0.010, box.width() * 0.13);
    const qreal yTolerance = std::max(0.013, box.height() * 0.06);
    const Path outline = simplify(path, 0.008);
    bool left = false;
    bool right = false;
    bool top = false;
    for (qsizetype index = 1; index < outline.size(); ++index) {
        const QPointF a = outline.at(index - 1);
        const QPointF b = outline.at(index);
        const qreal height = std::abs(b.y() - a.y());
        const qreal width = std::abs(b.x() - a.x());
        if (height >= box.height() * 0.65) {
            left |= std::abs(a.x() - box.left()) < xTolerance && std::abs(b.x() - box.left()) < xTolerance;
            right |= std::abs(a.x() - box.right()) < xTolerance && std::abs(b.x() - box.right()) < xTolerance;
        }
        if (width >= box.width() * 0.65)
            top |= std::abs(a.y() - box.bottom()) < yTolerance && std::abs(b.y() - box.bottom()) < yTolerance;
    }
    if (!left || !right || !top)
        return false;
    bar = chartPoint({box.center().x(), box.bottom()});
    return true;
}

Path singleTrend(QList<Path> series)
{
    for (Path &path : series) {
        if (path.constFirst().x() > path.constLast().x())
            std::reverse(path.begin(), path.end());
    }
    std::sort(series.begin(), series.end(), [](const Path &a, const Path &b) {
        return a.constFirst().x() < b.constFirst().x();
    });

    Path trend;
    for (const Path &path : std::as_const(series)) {
        if (!trend.isEmpty() && length(path.constFirst() - trend.constLast()) > 0.055)
            return {};
        trend += path;
    }
    if (trend.isEmpty() || trend.constLast().x() - trend.constFirst().x() < 0.14)
        return {};

    qreal lastX = trend.constFirst().x();
    qreal totalBacktracking = 0;
    for (QPointF &point : trend) {
        if (point.x() < lastX) {
            const qreal backwards = lastX - point.x();
            if (backwards > 0.035)
                return {};
            totalBacktracking += backwards;
            point.setX(lastX);
        } else {
            lastX = point.x();
        }
        point = chartPoint(point);
    }
    if (totalBacktracking > 0.23)
        return {};
    trend = limitSamples(trend, 4096);
    Path result = simplify(trend, 0.006);
    for (qreal tolerance = 0.009; result.size() > maxChartPoints; tolerance *= 1.5)
        result = simplify(trend, tolerance);
    return result;
}

Series findSeries(const QList<Path> &paths, const Frame &frame)
{
    QList<Path> series;
    for (int index = 0; index < paths.size(); ++index) {
        if (index == frame.xStroke || index == frame.yStroke)
            continue;

        Path mapped;
        mapped.reserve(paths.at(index).size());
        for (const QPointF &point : paths.at(index))
            mapped.append(frame.map(point));
        if (mapped.isEmpty())
            continue;
        const QRectF box = bounds(mapped);
        if (isAxisMark(box))
            continue;
        if (mapped.size() < 2 || box.left() < -0.08 || box.right() > 1.08 || box.top() < -0.08 || box.bottom() > 1.08)
            return {};
        series.append(std::move(mapped));
    }
    if (series.isEmpty())
        return {};

    Path bars;
    for (const Path &path : std::as_const(series)) {
        QPointF bar;
        if (findBar(path, bar))
            bars.append(bar);
    }
    if (bars.size() == series.size()) {
        std::sort(bars.begin(), bars.end(), [](QPointF a, QPointF b) { return a.x() < b.x(); });
        for (qsizetype pos = 1; pos < bars.size(); ++pos) {
            if (bars.at(pos).x() - bars.at(pos - 1).x() < 0.025)
                return {};
        }
        return {QStringLiteral("bars"), bars};
    }
    if (!bars.isEmpty())
        return {};

    const Path line = singleTrend(std::move(series));
    return line.isEmpty() ? Series{} : Series{QStringLiteral("line"), line};
}

QVariantMap error(const QString &message)
{
    return {{QStringLiteral("error"), message}};
}

bool readNumber(const QVariant &value, double &number)
{
    bool ok = false;
    number = value.toDouble(&ok);
    return ok && std::isfinite(number);
}

QString csvField(QString label, const QString &fallback)
{
    label = label.trimmed();
    if (label.isEmpty())
        label = fallback;
    if (QStringLiteral("=+-@").contains(label.front()))
        label.prepend(QLatin1Char('\''));
    label.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + label + QLatin1Char('"');
}

QString csvAxisValue(double fraction, double minimum, double maximum)
{
    const double value = fraction == 1 ? maximum : minimum + fraction * (maximum - minimum);
    return QString::number(value, 'g', std::numeric_limits<double>::digits10);
}
}

QVariantMap ChartRecognizer::recognize(const QVariantList &strokes) const
{
    if (strokes.size() > maxStrokes)
        return error(tr("Seleziona un solo grafico alla volta."));

    qsizetype sampleCount = 0;
    QList<Path> paths;
    for (const QVariant &value : strokes) {
        const QVariantMap stroke = value.toMap();
        const QVariantList samples = stroke.value(QStringLiteral("points")).toList();
        sampleCount += samples.size();
        if (sampleCount > maxInkPoints)
            return error(tr("Seleziona una porzione più piccola del grafico."));

        const InkTool tool = inkToolFromName(stroke.value(QStringLiteral("tool")).toString());
        if (tool == InkTool::Highlighter || tool == InkTool::Eraser)
            continue;
        QList<InkPoint> ink;
        ink.reserve(samples.size());
        for (const QVariant &entry : samples) {
            const QVariantMap point = entry.toMap();
            double x = 0;
            double y = 0;
            if (!readNumber(point.value(QStringLiteral("x")), x) || !readNumber(point.value(QStringLiteral("y")), y)
                || x < 0 || x > 1 || y < 0 || y > 1)
                return error(tr("La selezione contiene tratti non validi."));
            ink.append({{x, y}, 1});
        }
        if (isInkShape(tool))
            ink = shapeOutline(ink, tool);
        Path path;
        for (const InkPoint &point : std::as_const(ink)) {
            const QPointF scaled(point.position.x(), point.position.y() * pageAspect);
            if (path.isEmpty() || path.constLast() != scaled)
                path.append(scaled);
        }
        if (!path.isEmpty())
            paths.append(limitSamples(path, 384));
    }

    const QList<Frame> frames = framesFor(paths);
    if (frames.isEmpty())
        return error(tr("Non ho trovato due assi che si incontrano in basso a sinistra. Seleziona gli assi insieme alla serie."));

    Series match;
    qreal bestScore = 0;
    for (const Frame &frame : frames) {
        if (bestScore > 0 && frame.score < bestScore * 0.78)
            break;
        const Series candidate = findSeries(paths, frame);
        if (candidate.points.isEmpty())
            continue;
        if (!match.points.isEmpty())
            return error(tr("Ho trovato più di una coppia di assi possibile. Seleziona un grafico più semplice."));
        match = candidate;
        bestScore = frame.score;
    }
    if (match.points.isEmpty())
        return error(tr("Non ho riconosciuto una singola curva o barre verticali. Prova a escludere scritte e tratti sovrapposti."));

    QVariantList points;
    points.reserve(match.points.size());
    for (QPointF point : std::as_const(match.points))
        points.append(QVariantMap{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}});
    const QVariantMap chart{{QStringLiteral("type"), QStringLiteral("chart")},
                            {QStringLiteral("chartKind"), match.kind},
                            {QStringLiteral("title"), QString{}},
                            {QStringLiteral("xLabel"), QString{}},
                            {QStringLiteral("yLabel"), QString{}},
                            {QStringLiteral("points"), points}};
    return {{QStringLiteral("item"), chart}};
}

QString ChartRecognizer::csv(const QVariantMap &chart) const
{
    double xMax = 0;
    double yMax = 0;
    const QVariantList points = chart.value(QStringLiteral("points")).toList();
    if (!readNumber(chart.value(QStringLiteral("xMax")), xMax) || !readNumber(chart.value(QStringLiteral("yMax")), yMax)
        || xMax <= 0 || xMax > maxAxisMagnitude || yMax <= 0 || yMax > maxAxisMagnitude
        || points.isEmpty() || points.size() > maxChartPoints)
        return {};

    double xMin = 0;
    double yMin = 0;
    if (chart.contains(QStringLiteral("minimum"))) {
        const QVariantMap minimum = chart.value(QStringLiteral("minimum")).toMap();
        if (!readNumber(minimum.value(QStringLiteral("x")), xMin) || !readNumber(minimum.value(QStringLiteral("y")), yMin)
            || xMin < -maxAxisMagnitude || xMin > 0 || yMin < -maxAxisMagnitude || yMin > 0)
            return {};
    }

    QString result = csvField(chart.value(QStringLiteral("xLabel")).toString(), QStringLiteral("X"))
        + QLatin1Char(',') + csvField(chart.value(QStringLiteral("yLabel")).toString(), QStringLiteral("Y"))
        + QStringLiteral("\r\n");
    for (const QVariant &entry : points) {
        const QVariantMap point = entry.toMap();
        double x = 0;
        double y = 0;
        if (!readNumber(point.value(QStringLiteral("x")), x) || !readNumber(point.value(QStringLiteral("y")), y)
            || x < 0 || x > 1 || y < 0 || y > 1)
            return {};
        result += csvAxisValue(x, xMin, xMax) + QLatin1Char(',') + csvAxisValue(y, yMin, yMax)
            + QStringLiteral("\r\n");
    }
    return result;
}

bool ChartRecognizer::copyCsv(const QVariantMap &chart) const
{
    const QString data = csv(chart);
    QClipboard *const clipboard = QGuiApplication::clipboard();
    if (!clipboard || data.isEmpty())
        return false;
    clipboard->setText(data);
    return clipboard->text() == data;
}
