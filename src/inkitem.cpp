#include "inkitem.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPointingDevice>
#include <QPolygonF>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QtMath>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace {
QPointF pagePosition(QSizeF size, const InkPoint &point)
{
    return {point.position.x() * size.width(), point.position.y() * size.height()};
}

bool insideAncestorClips(const QQuickItem &item, QPointF position)
{
    for (const QQuickItem *parent = item.parentItem(); parent; parent = parent->parentItem()) {
        if (parent->clip() && !parent->clipRect().contains(parent->mapFromItem(&item, position)))
            return false;
    }
    return true;
}

qreal distanceSquared(QPointF point, QPointF start, QPointF end)
{
    const QPointF direction = end - start;
    const qreal lengthSquared = QPointF::dotProduct(direction, direction);
    const qreal position = lengthSquared > 0
        ? std::clamp(QPointF::dotProduct(point - start, direction) / lengthSquared, 0.0, 1.0)
        : 0;
    const QPointF delta = point - start - position * direction;
    return QPointF::dotProduct(delta, delta);
}

bool segmentsAreNear(QPointF from, QPointF to, QPointF start, QPointF end, qreal radius)
{
    if (QLineF(from, to).intersects(QLineF(start, end)) == QLineF::BoundedIntersection)
        return true;
    return std::min({distanceSquared(from, start, end), distanceSquared(to, start, end),
                     distanceSquared(start, from, to), distanceSquared(end, from, to)}) <= radius * radius;
}

QVariantMap strokeData(const QList<InkPoint> &points, QColor color, qreal width, InkTool tool)
{
    QVariantList coordinates;
    coordinates.reserve(points.size());
    for (const InkPoint &point : points) {
        coordinates.append(QVariantMap{{QStringLiteral("x"), point.position.x()},
                                       {QStringLiteral("y"), point.position.y()},
                                       {QStringLiteral("p"), point.pressure}});
    }
    QVariantMap stroke{{QStringLiteral("color"), color.name(QColor::HexRgb)},
                       {QStringLiteral("width"), width},
                       {QStringLiteral("points"), coordinates}};
    if (tool != InkTool::Pen)
        stroke.insert(QStringLiteral("tool"), inkToolName(tool));
    return stroke;
}
}

#ifdef Q_OS_LINUX
#include <QDir>
#include <QFile>
#include <QSocketNotifier>

#include <array>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

class PaperProMarker
{
public:
    explicit PaperProMarker(InkItem &ink) : m_ink(ink)
    {
        const QDir input(QStringLiteral("/dev/input"));
        const auto devices = input.entryList({QStringLiteral("event*")},
                                             QDir::System | QDir::Files);
        for (const QString &device : devices) {
            const QByteArray path = QFile::encodeName(input.filePath(device));
            m_fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (m_fd < 0)
                continue;
            if (isMarker() && readState())
                break;
            ::close(m_fd);
            m_fd = -1;
        }

        if (available()) {
            m_notifier = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read);
            QObject::connect(m_notifier.get(), &QSocketNotifier::activated,
                             &m_ink, [this] { readEvents(); });
        }
    }

    ~PaperProMarker()
    {
        m_notifier.reset();
        if (available())
            ::close(m_fd);
    }

    bool available() const { return m_fd >= 0; }

private:
    static constexpr int bitsPerWord = sizeof(unsigned long) * CHAR_BIT;
    using KeyBits = std::array<unsigned long, (KEY_CNT + bitsPerWord - 1) / bitsPerWord>;

    static bool hasKey(const KeyBits &keys, int code)
    {
        return (keys[code / bitsPerWord] & (1UL << (code % bitsPerWord))) != 0;
    }

    bool isMarker() const
    {
        char name[256] = {};
        KeyBits keys = {};
        return ::ioctl(m_fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0
            && QByteArray(name).toLower().contains("elan marker")
            && ::ioctl(m_fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys.data()) >= 0
            && hasKey(keys, BTN_TOOL_PEN) && hasKey(keys, BTN_TOUCH);
    }

    bool readState()
    {
        KeyBits keys = {};
        if (::ioctl(m_fd, EVIOCGKEY(sizeof(keys)), keys.data()) < 0)
            return false;

        constexpr std::array<int, 3> codes = {ABS_X, ABS_Y, ABS_PRESSURE};
        for (size_t index = 0; index < codes.size(); ++index) {
            input_absinfo &axis = m_axes[index];
            if (::ioctl(m_fd, EVIOCGABS(codes[index]), &axis) < 0
                || axis.maximum <= axis.minimum)
                return false;
        }

        m_tipDown = hasKey(keys, BTN_TOUCH);
        m_penPresent = hasKey(keys, BTN_TOOL_PEN);
        m_eraserPresent = hasKey(keys, BTN_TOOL_RUBBER);
        m_waitForTipUp = m_tipDown;
        m_drawing = false;
        return true;
    }

    static qreal normalized(const input_absinfo &axis)
    {
        return std::clamp((qreal(axis.value) - axis.minimum)
                              / (qreal(axis.maximum) - axis.minimum),
                          0.0, 1.0);
    }

    void readEvents()
    {
        std::array<input_event, 64> events;
        while (available()) {
            const ssize_t bytes = ::read(m_fd, events.data(), sizeof(events));
            if (bytes > 0 && bytes % sizeof(input_event) == 0) {
                for (size_t index = 0; index < size_t(bytes) / sizeof(input_event); ++index) {
                    handleEvent(events[index]);
                    if (!available())
                        return;
                }
                continue;
            }
            if (bytes < 0 && errno == EINTR)
                continue;
            if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                return;
            disconnectDevice();
        }
    }

    void disconnectDevice()
    {
        m_notifier->setEnabled(false);
        ::close(m_fd);
        m_fd = -1;
        m_ink.cancelMarker();
        emit m_ink.penAvailableChanged();
    }

    void handleEvent(const input_event &event)
    {
        if (event.type == EV_SYN && event.code == SYN_DROPPED) {
            m_desynchronized = true;
            m_ink.cancelMarker();
        } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
            if (m_desynchronized) {
                m_desynchronized = false;
                if (!readState())
                    disconnectDevice();
            } else {
                report();
            }
        } else if (!m_desynchronized && event.type == EV_ABS) {
            if (event.code == ABS_X)
                m_axes[0].value = event.value;
            else if (event.code == ABS_Y)
                m_axes[1].value = event.value;
            else if (event.code == ABS_PRESSURE)
                m_axes[2].value = event.value;
        } else if (!m_desynchronized && event.type == EV_KEY) {
            if (event.code == BTN_TOOL_PEN)
                m_penPresent = event.value != 0;
            else if (event.code == BTN_TOOL_RUBBER)
                m_eraserPresent = event.value != 0;
            else if (event.code == BTN_TOUCH)
                m_tipDown = event.value != 0;
        }
    }

    void report()
    {
        if (m_waitForTipUp) {
            m_waitForTipUp = m_tipDown;
            return;
        }
        const bool wasDrawing = m_drawing;
        m_drawing = m_tipDown && (m_penPresent || m_eraserPresent);
        if (wasDrawing && m_tipDown && !m_drawing) {
            m_waitForTipUp = true;
            m_ink.cancelMarker();
            return;
        }
        QQuickWindow *window = m_ink.window();
        if ((!m_drawing && !wasDrawing) || !window || !window->isVisible())
            return;

        const auto phase = m_drawing ? (wasDrawing ? InkItem::Phase::Move : InkItem::Phase::Press)
                                     : InkItem::Phase::Release;
        // The device can reset its axes on tip-up. Keep the last contact position.
        if (m_drawing) {
            m_contactPosition = {normalized(m_axes[0]) * qMax(0, window->width() - 1),
                                 normalized(m_axes[1]) * qMax(0, window->height() - 1)};
        }
        m_ink.dispatchMarker(phase, m_contactPosition,
                             m_drawing ? normalized(m_axes[2]) : 0, m_eraserPresent);
    }

    InkItem &m_ink;
    int m_fd = -1;
    std::unique_ptr<QSocketNotifier> m_notifier;
    std::array<input_absinfo, 3> m_axes = {};
    QPointF m_contactPosition;
    bool m_penPresent = false;
    bool m_eraserPresent = false;
    bool m_tipDown = false;
    bool m_drawing = false;
    bool m_desynchronized = false;
    bool m_waitForTipUp = false;
};
#endif

InkItem::InkItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setFillColor(Qt::transparent);
    connect(this, &QQuickItem::windowChanged, this, &InkItem::observeWindow);
    observeWindow(window());
    m_shapeHold.setSingleShot(true);
    m_shapeHold.setInterval(550);
    connect(&m_shapeHold, &QTimer::timeout, this, &InkItem::previewHeldShape);

    m_epaper = QGuiApplication::platformName() == QStringLiteral("epaper");
#ifdef Q_OS_LINUX
    if (m_epaper)
        m_marker = std::make_unique<PaperProMarker>(*this);
#endif
}

InkItem::~InkItem()
{
    releaseMarkerWindow();
    disconnect(this, &QQuickItem::windowChanged, this, &InkItem::observeWindow);
    if (m_observedWindow)
        m_observedWindow->removeEventFilter(this);
}

bool InkItem::penAvailable() const
{
#ifdef Q_OS_LINUX
    if (m_epaper)
        return m_marker && m_marker->available();
#endif
    return !m_epaper;
}

void InkItem::dispatchMarker(Phase phase, QPointF windowPoint, qreal pressure, bool eraserTip)
{
    auto *mainWindow = window();
    if (!mainWindow)
        return;

    const QPointF globalPoint = mainWindow->mapToGlobal(windowPoint);
    if (phase == Phase::Press) {
        cancelMarker();
        auto *modal = QGuiApplication::modalWindow();
        if ((!modal || modal == mainWindow) &&
            handleInput(Source::Marker, phase, mapFromScene(windowPoint), pressure, eraserTip)) {
            m_markerContact = MarkerContact::Ink;
            return;
        }
        if (eraserTip)
            return;

        m_markerWindow = modal ? qobject_cast<QQuickWindow *>(modal) : mainWindow;
        if (!m_markerWindow || !m_markerWindow->isVisible()) {
            m_markerWindow.clear();
            return;
        }
        if (m_markerWindow != mainWindow)
            m_markerWindow->installEventFilter(this);
        if (!m_markerPointer) {
            m_markerPointer = std::make_unique<QPointingDevice>(QStringLiteral("reMarkable Marker"), 478665,
                QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
                QInputDevice::Capability::Position, 1, 1);
        }
        m_markerContact = MarkerContact::Control;
    }

    // Keep the same route for the whole contact, even when crossing the page edge.
    if (m_markerContact == MarkerContact::Ink) {
        if (phase == Phase::Release)
            m_markerContact = MarkerContact::None;
        handleInput(Source::Marker, phase, mapFromScene(windowPoint), pressure, eraserTip);
        return;
    }
    if (m_markerContact != MarkerContact::Control || !m_markerWindow)
        return;

    auto *target = m_markerWindow.data();
    const QPointF position = target->mapFromGlobal(globalPoint);
    if (phase == Phase::Move && position == m_markerPosition)
        return;
    m_markerPosition = position;
    const auto type = phase == Phase::Press ? QEvent::MouseButtonPress
                    : phase == Phase::Move ? QEvent::MouseMove : QEvent::MouseButtonRelease;
    QMouseEvent event(type, position, position, globalPoint,
                      phase == Phase::Move ? Qt::NoButton : Qt::LeftButton,
                      phase == Phase::Release ? Qt::NoButton : Qt::LeftButton,
                      QGuiApplication::keyboardModifiers(), Qt::MouseEventSynthesizedByApplication,
                      m_markerPointer.get());
    event.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if (phase == Phase::Release) {
        m_markerContact = MarkerContact::None;
        releaseMarkerWindow();
    }
    QCoreApplication::sendEvent(target, &event);
}

void InkItem::cancelMarker()
{
    const auto contact = std::exchange(m_markerContact, MarkerContact::None);
    if (contact == MarkerContact::Ink) {
        cancelDrawing();
        return;
    }
    if (contact != MarkerContact::Control)
        return;
    const QPointer<QQuickWindow> target = m_markerWindow;
    releaseMarkerWindow();
    if (!target)
        return;

    // Cancel only this device's grabs. A mouse release would click a pressed button.
    QMouseEvent probe(QEvent::MouseMove, m_markerPosition, m_markerPosition,
                      target->mapToGlobal(m_markerPosition), Qt::NoButton, Qt::LeftButton,
                      Qt::NoModifier, Qt::MouseEventSynthesizedByApplication, m_markerPointer.get());
    const auto &point = probe.point(0);
    auto grabbers = probe.passiveGrabbers(point);
    const QPointer<QObject> exclusive = probe.exclusiveGrabber(point);
    if (exclusive && !grabbers.contains(exclusive))
        grabbers.append(exclusive);
    for (const auto &grabber : grabbers) {
        if (grabber) {
            QTouchEvent cancel(QEvent::TouchCancel, m_markerPointer.get(), Qt::NoModifier, {point});
            QCoreApplication::sendEvent(grabber, &cancel);
        }
    }
    probe.setExclusiveGrabber(point, nullptr);
    probe.clearPassiveGrabbers(point);
}

void InkItem::releaseMarkerWindow()
{
    if (m_markerWindow && m_markerWindow != m_observedWindow)
        m_markerWindow->removeEventFilter(this);
    m_markerWindow.clear();
}

void InkItem::setStrokes(const QVariantList &strokes)
{
    if (m_strokes == strokes)
        return;

    // Keep the live pixels when the model confirms exactly the stroke just rendered.
    const bool alreadyRendered = !m_pendingStroke.isEmpty()
        && strokes.size() == m_strokes.size() + 1
        && strokes.constLast().toMap() == m_pendingStroke
        && std::equal(m_strokes.cbegin(), m_strokes.cend(), strokes.cbegin());
    m_strokes = strokes;
    resetDrawing();
    m_pendingStroke.clear();
    if (!alreadyRendered)
        rebuildImage();
    emit strokesChanged();
}

void InkItem::setInkColor(QColor color)
{
    if (!color.isValid())
        return;
    color.setAlpha(255);
    if (m_inkColor == color)
        return;
    m_inkColor = color;
    emit inkColorChanged();
}

void InkItem::setInkWidth(qreal width)
{
    if (!std::isfinite(width) || width < 0.1 || width > 30 || m_inkWidth == width)
        return;
    m_inkWidth = width;
    emit inkWidthChanged();
}

QString InkItem::inkTool() const
{
    return inkToolName(m_inkTool);
}

void InkItem::setInkTool(const QString &name)
{
    const InkTool tool = inkToolFromName(name);
    if (tool == m_inkTool || inkToolName(tool) != name)
        return;
    cancelDrawing();
    m_inkTool = tool;
    emit inkToolChanged();
}

void InkItem::setSmartShapes(bool enabled)
{
    if (m_smartShapes == enabled)
        return;
    m_smartShapes = enabled;
    m_shapeHold.stop();
    restoreFreehand();
    emit smartShapesChanged();
}

void InkItem::setLassoMode(bool enabled)
{
    if (m_lassoMode == enabled)
        return;
    cancelDrawing();
    m_lassoMode = enabled;
    emit lassoModeChanged();
}

void InkItem::setErasableFrom(int first)
{
    first = qMax(0, first);
    if (m_erasableFrom == first)
        return;
    if (m_live.tool == InkTool::Eraser)
        cancelDrawing();
    m_erasableFrom = first;
    emit erasableRangeChanged();
}

void InkItem::setErasableCount(int count)
{
    count = qMax(-1, count);
    if (m_erasableCount == count)
        return;
    if (m_live.tool == InkTool::Eraser)
        cancelDrawing();
    m_erasableCount = count;
    emit erasableRangeChanged();
}

void InkItem::setAcceptsMouse(bool enabled)
{
    if (enabled == m_acceptsMouse)
        return;
    m_acceptsMouse = enabled;
    setAcceptedMouseButtons(enabled ? Qt::LeftButton : Qt::NoButton);
    if (!enabled && m_live.source == Source::Mouse)
        cancelDrawing();
    emit acceptsMouseChanged();
}

void InkItem::cancelDrawing()
{
    if (m_live.source == Source::None)
        return;
    resetDrawing();
    rebuildImage();
}

void InkItem::observeWindow(QQuickWindow *window)
{
    cancelMarker();
    if (m_observedWindow)
        m_observedWindow->removeEventFilter(this);
    cancelDrawing();
    m_observedWindow = window;
    if (window)
        window->installEventFilter(this);
}

bool InkItem::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Hide:
    case QEvent::WindowDeactivate:
        cancelDrawing();
        cancelMarker();
        break;
    case QEvent::Leave:
        if (m_live.source == Source::Tablet)
            cancelDrawing();
        break;
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease: {
        if (m_epaper)
            break;
        auto *tablet = static_cast<QTabletEvent *>(event);
        const bool eraserTip = tablet->pointerType() == QPointingDevice::PointerType::Eraser;
        Phase phase = Phase::Move;
        if (event->type() == QEvent::TabletPress) {
            if (tablet->button() != Qt::LeftButton)
                break;
            phase = Phase::Press;
        } else if (event->type() == QEvent::TabletRelease) {
            if (tablet->button() != Qt::LeftButton)
                break;
            phase = Phase::Release;
        }
        if (handleInput(Source::Tablet, phase, mapFromScene(tablet->position()),
                        tablet->pressure(), eraserTip)) {
            tablet->accept();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QQuickPaintedItem::eventFilter(watched, event);
}

void InkItem::mousePressEvent(QMouseEvent *event)
{
    const bool accepted = m_acceptsMouse && event->button() == Qt::LeftButton
        && event->source() == Qt::MouseEventNotSynthesized
        && handleInput(Source::Mouse, Phase::Press, event->position(), 0.65);
    event->setAccepted(accepted);
}

void InkItem::mouseMoveEvent(QMouseEvent *event)
{
    event->setAccepted(handleInput(Source::Mouse, Phase::Move, event->position(), 0.65));
}

void InkItem::mouseReleaseEvent(QMouseEvent *event)
{
    event->setAccepted(handleInput(Source::Mouse, Phase::Release, event->position(), 0.65));
}

void InkItem::mouseUngrabEvent()
{
    if (m_live.source == Source::Mouse)
        cancelDrawing();
}

void InkItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickPaintedItem::itemChange(change, data);
    if ((change == ItemVisibleHasChanged || change == ItemEnabledHasChanged) && !data.boolValue)
        cancelDrawing();
}

void InkItem::geometryChange(const QRectF &geometry, const QRectF &previousGeometry)
{
    QQuickPaintedItem::geometryChange(geometry, previousGeometry);
    if (geometry.size() != previousGeometry.size()) {
        resetDrawing();
        rebuildImage();
    }
}

QString InkItem::textItemAt(QPointF position) const
{
    if (!isVisible() || !contains(position) || !insideAncestorClips(*this, position)
        || width() <= 0 || height() <= 0)
        return {};

    const QPointF point(position.x() / width(), position.y() / height());
    for (auto it = m_pageItems.crbegin(); it != m_pageItems.crend(); ++it) {
        const QVariantMap item = it->toMap();
        const QRectF bounds(item.value(QStringLiteral("x")).toDouble(), item.value(QStringLiteral("y")).toDouble(),
                            item.value(QStringLiteral("width")).toDouble(), item.value(QStringLiteral("height")).toDouble());
        if (bounds.contains(point))
            return item.value(QStringLiteral("type")) == QStringLiteral("text")
                ? item.value(QStringLiteral("id")).toString() : QString{};
    }
    return {};
}

bool InkItem::handleInput(Source source, Phase phase, QPointF position, qreal pressure, bool eraserTip)
{
    if (phase == Phase::Press) {
        if (m_live.source != Source::None || !isVisible() || !isEnabled()
            || m_image.isNull() || !contains(position) || (m_lassoMode && eraserTip)
            || !insideAncestorClips(*this, position))
            return false;
        m_live.source = source;
        m_live.color = m_inkColor;
        m_live.width = m_inkWidth;
        m_live.tool = eraserTip ? InkTool::Eraser : m_inkTool;
        emit drawingChanged();
        if (!m_lassoMode && m_live.tool == InkTool::Eraser)
            prepareEraser();
        if (!m_lassoMode && (m_live.tool == InkTool::Pen || m_live.tool == InkTool::Pencil || m_live.tool == InkTool::Marker))
            m_live.textTapId = textItemAt(position);
        if (m_live.textTapId.isEmpty()) {
            addPoint(position, pressure);
        } else {
            m_live.textTapStart = position;
            m_live.textTapPressure = pressure;
            m_live.textTapTime.start();
        }
        emit strokeStarted();
        return true;
    }

    if (m_live.source != source)
        return false;

    if (!m_live.textTapId.isEmpty()) {
        const auto *hints = QGuiApplication::styleHints();
        const bool stillTap = QLineF(position, m_live.textTapStart).length() < hints->startDragDistance()
            && m_live.textTapTime.elapsed() < hints->mousePressAndHoldInterval()
            && textItemAt(position) == m_live.textTapId;
        if (stillTap) {
            if (phase == Phase::Release) {
                const QString id = m_live.textTapId;
                resetDrawing();
                emit textItemTapped(id);
            }
            return true;
        }
        m_live.textTapId.clear();
        addPoint(m_live.textTapStart, m_live.textTapPressure);
    }

    if (!insideAncestorClips(*this, position)) {
        finishStroke();
        return true;
    }

    const bool shape = isInkShape(m_live.tool) || m_lassoMode;
    if (phase == Phase::Move)
        addPoint(position, pressure);
    else if (contains(position) || shape)
        addPoint(position, pressure > 0 ? pressure : m_live.points.constLast().pressure);

    if (phase == Phase::Release || (!shape && !contains(position)))
        finishStroke();
    return true;
}

void InkItem::addPoint(QPointF position, qreal pressure)
{
    const InkPoint point = {
        QPointF(std::clamp(position.x() / width(), 0.0, 1.0),
                std::clamp(position.y() / height(), 0.0, 1.0)),
        std::clamp(pressure, 0.0, 1.0)
    };
    if (m_lassoMode) {
        addLassoPoint(point);
        return;
    }
    if (isInkShape(m_live.tool)) {
        updateShapeAnchor(point);
        return;
    }

    const bool firstPoint = m_live.points.isEmpty();
    if (!prepareShapeSample(pagePosition(size(), point)))
        return;
    const InkPoint previous = firstPoint ? point : m_live.points.constLast();
    if (!firstPoint && previous.position == point.position && previous.pressure == point.pressure)
        return;
    m_live.points.append(point);

    if (m_live.tool == InkTool::Eraser) {
        eraseNear(previous, point);
        return;
    }

    if (m_live.tool == InkTool::Highlighter) {
        const qreal padding = inkRadius(width(), m_live.width, 1, m_live.tool) + 2;
        const QRect dirty = QRectF(pagePosition(size(), previous), pagePosition(size(), point))
            .normalized().adjusted(-padding, -padding, padding, padding).toAlignedRect();
        update(dirty.intersected(m_image.rect()));
        return;
    }

    QPainter painter(&m_image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect dirty = drawInkSegment(painter, size(), previous, point, m_live.color, m_live.width, m_live.tool)
                            .intersected(m_image.rect());
    update(dirty);
}

void InkItem::addLassoPoint(const InkPoint &point)
{
    const QPointF current = pagePosition(size(), point);
    const QPointF previous = m_live.points.isEmpty() ? current : pagePosition(size(), m_live.points.constLast());
    if (!m_live.points.isEmpty() && QLineF(previous, current).length() < 1.5)
        return;

    m_live.points.append(point);
    const QRect dirty = QRectF(previous, current).normalized().adjusted(-3, -3, 3, 3).toAlignedRect();
    update(dirty.intersected(m_image.rect()));
}

bool InkItem::prepareShapeSample(QPointF position)
{
    if (!m_smartShapes || m_live.tool == InkTool::Eraser || m_live.tool == InkTool::Highlighter)
        return true;

    const qreal movement = QLineF(m_holdPosition, position).length();
    if (m_shapePreview && movement <= qMax(5.0, width() * 0.006))
        return false;
    if (m_shapePreview || m_live.points.isEmpty() || movement > qMax(2.0, width() * 0.0025)) {
        restoreFreehand();
        m_holdPosition = position;
        m_shapeHold.start();
    }
    return true;
}

void InkItem::previewHeldShape()
{
    if (m_live.source == Source::None || m_live.points.size() < 2)
        return;
    m_shapePreview = recognizeInkShape(m_live.points, size());
    if (m_shapePreview)
        rebuildImage();
}

void InkItem::restoreFreehand()
{
    if (!m_shapePreview)
        return;
    m_shapePreview.reset();
    QPainter painter(&m_image);
    painter.setRenderHint(QPainter::Antialiasing);
    InkPoint previous = m_live.points.constFirst();
    for (const InkPoint &point : std::as_const(m_live.points)) {
        drawInkSegment(painter, size(), previous, point, m_live.color, m_live.width, m_live.tool);
        previous = point;
    }
    update();
}

void InkItem::finishLasso()
{
    QVariantList polygon;
    if (m_live.points.size() >= 3) {
        polygon.reserve(m_live.points.size());
        for (const InkPoint &point : std::as_const(m_live.points)) {
            polygon.append(QVariantMap{{QStringLiteral("x"), point.position.x()},
                                       {QStringLiteral("y"), point.position.y()}});
        }
    }
    resetDrawing();
    update();
    if (!polygon.isEmpty())
        emit lassoCompleted(polygon);
}

void InkItem::updateShapeAnchor(InkPoint point)
{
    point.pressure = 1;
    if (m_live.points.isEmpty()) {
        m_live.points.append(point);
        return;
    }
    if (m_live.tool == InkTool::Underline) {
        const QPointF start = m_live.points.constFirst().position;
        point.position.setY(start.y());
        if (std::abs(point.position.x() - start.x()) * width() < qMax(2.0, width() * 0.0025))
            point.position.setX(start.x());
    }

    const QPointF origin = pagePosition(size(), m_live.points.constFirst());
    const InkPoint previous = m_live.points.constLast();
    if (previous.position == point.position)
        return;
    if (m_live.points.size() == 1)
        m_live.points.append(point);
    else
        m_live.points.last() = point;

    const QPointF oldCorner = pagePosition(size(), previous);
    const QPointF newCorner = pagePosition(size(), point);
    const qreal left = std::min({origin.x(), oldCorner.x(), newCorner.x()});
    const qreal right = std::max({origin.x(), oldCorner.x(), newCorner.x()});
    const qreal top = std::min({origin.y(), oldCorner.y(), newCorner.y()});
    const qreal bottom = std::max({origin.y(), oldCorner.y(), newCorner.y()});
    const qreal padding = inkRadius(width(), m_live.width, 1, m_live.tool) + 2;
    const QRect dirty = QRectF(QPointF(left, top), QPointF(right, bottom))
        .adjusted(-padding, -padding, padding, padding).toAlignedRect();
    update(dirty.intersected(m_image.rect()));
}

void InkItem::finishStroke()
{
    if (m_lassoMode) {
        finishLasso();
        return;
    }
    if (m_live.tool == InkTool::Eraser) {
        QList<int> erased = m_erased.values();
        resetDrawing();
        if (erased.isEmpty())
            return;

        std::sort(erased.begin(), erased.end());
        QVariantList indexes;
        indexes.reserve(erased.size());
        for (int index : erased)
            indexes.append(index);
        const QVariantList previous = m_strokes;
        emit strokesErased(indexes);
        if (m_strokes == previous)
            rebuildImage();
        return;
    }

    const bool shape = isInkShape(m_live.tool);
    if (shape && (m_live.points.size() != 2
                  || m_live.points.constFirst().position == m_live.points.constLast().position)) {
        resetDrawing();
        return;
    }

    // Underlines use the existing line format for older versions of Quaderno.
    const InkTool storedTool = m_live.tool == InkTool::Underline ? InkTool::Line : m_live.tool;
    const QVariantMap stroke = strokeData(m_live.points, m_live.color, m_live.width, storedTool);
    if (m_shapePreview) {
        const QVariantMap corrected = strokeData(m_shapePreview->anchors, m_live.color, m_live.width, m_shapePreview->tool);
        {
            QPainter painter(&m_image);
            painter.setRenderHint(QPainter::Antialiasing);
            drawInkShape(painter, size(), m_shapePreview->anchors, m_live.color, m_live.width, m_shapePreview->tool);
        }
        resetDrawing();
        m_pendingStroke = corrected;
        m_pendingStroke.insert(QStringLiteral("original"), stroke);
        emit correctedStrokeCompleted(corrected, stroke);
        if (!m_pendingStroke.isEmpty()) {
            m_pendingStroke.clear();
            rebuildImage();
        }
        return;
    }
    if (m_live.tool == InkTool::Highlighter || shape) {
        QPainter painter(&m_image);
        painter.setRenderHint(QPainter::Antialiasing);
        if (shape)
            drawInkShape(painter, size(), m_live.points, m_live.color, m_live.width, m_live.tool);
        else
            drawHighlighterStroke(painter, size(), m_live.points, m_live.color, m_live.width);
    }
    resetDrawing();
    m_pendingStroke = stroke;
    emit strokeCompleted(stroke);
    if (!m_pendingStroke.isEmpty()) {
        m_pendingStroke.clear();
        rebuildImage();
    }
}

void InkItem::prepareEraser()
{
    m_eraserOffset = qMin<qsizetype>(m_erasableFrom, m_strokes.size());
    const qsizetype available = m_strokes.size() - m_eraserOffset;
    const qsizetype count = m_erasableCount == -1 ? available : qMin<qsizetype>(m_erasableCount, available);
    m_erasable.reserve(count);
    for (qsizetype index = m_eraserOffset; index < m_eraserOffset + count; ++index) {
        const QVariantMap stroke = m_strokes.at(index).toMap();
        const auto tool = inkToolFromName(stroke.value(QStringLiteral("tool")).toString());
        const auto anchors = storedInkPoints(stroke);
        const auto points = isInkShape(tool) ? shapeOutline(anchors, tool) : anchors;
        const qreal strokeWidth = stroke.value(QStringLiteral("width")).toReal();

        ErasableStroke hit;
        hit.points.reserve(points.size());
        hit.radii.reserve(points.size());
        if (!points.isEmpty()) {
            QPointF first = pagePosition(size(), points.constFirst());
            QPointF minimum = first;
            QPointF maximum = first;
            qreal radius = 0;
            for (const InkPoint &point : points) {
                const QPointF position = pagePosition(size(), point);
                const qreal pointRadius = inkRadius(width(), strokeWidth, point.pressure, tool);
                hit.points.append(position);
                hit.radii.append(pointRadius);
                radius = qMax(radius, pointRadius);
                minimum = {qMin(minimum.x(), position.x()), qMin(minimum.y(), position.y())};
                maximum = {qMax(maximum.x(), position.x()), qMax(maximum.y(), position.y())};
            }
            hit.bounds = QRectF(minimum, maximum).adjusted(-radius, -radius, radius, radius);
        }
        m_erasable.append(std::move(hit));
    }
}

void InkItem::eraseNear(const InkPoint &from, const InkPoint &to)
{
    const QPointF a = pagePosition(size(), from);
    const QPointF b = pagePosition(size(), to);
    const qreal radius = qMax(5.0, width() / 95);
    const QRectF bounds = QRectF(a, b).normalized().adjusted(-radius, -radius, radius, radius);
    const qsizetype alreadyErased = m_erased.size();
    for (qsizetype localIndex = 0; localIndex < m_erasable.size(); ++localIndex) {
        const int index = m_eraserOffset + localIndex;
        const ErasableStroke &stroke = m_erasable.at(localIndex);
        if (m_erased.contains(index) || !bounds.intersects(stroke.bounds))
            continue;

        for (qsizetype point = 0; point < stroke.points.size(); ++point) {
            const qsizetype previous = qMax<qsizetype>(0, point - 1);
            const qreal reach = radius + qMax(stroke.radii.at(previous), stroke.radii.at(point));
            if (segmentsAreNear(a, b, stroke.points.at(previous), stroke.points.at(point), reach)) {
                m_erased.insert(index);
                break;
            }
        }
    }
    if (m_erased.size() != alreadyErased)
        rebuildImage();
}

void InkItem::resetDrawing()
{
    const bool wasDrawing = drawing();
    m_shapeHold.stop();
    m_shapePreview.reset();
    m_live = {};
    m_erased.clear();
    m_erasable.clear();
    m_eraserOffset = 0;
    if (wasDrawing)
        emit drawingChanged();
}

bool InkItem::drawing() const
{
    return m_live.source != Source::None;
}

void InkItem::rebuildImage()
{
    m_image = QImage(qCeil(width()), qCeil(height()), QImage::Format_ARGB32_Premultiplied);
    if (!m_image.isNull()) {
        m_image.fill(Qt::transparent);
        QPainter painter(&m_image);
        paintStoredStrokes(painter, size(), m_strokes, m_erased);
    }
    update();
}

void InkItem::paint(QPainter *painter)
{
    painter->drawImage(QPoint(0, 0), m_image);
    if (m_lassoMode && m_live.source != Source::None) {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(QColor(QStringLiteral("#155cc2")), qMax(1.5, width() * 0.002), Qt::DashLine));
        QPolygonF path;
        path.reserve(m_live.points.size());
        for (const InkPoint &point : std::as_const(m_live.points))
            path.append(pagePosition(size(), point));
        painter->drawPolyline(path);
    } else if (m_shapePreview) {
        painter->setRenderHint(QPainter::Antialiasing);
        drawInkShape(*painter, size(), m_shapePreview->anchors, m_live.color, m_live.width, m_shapePreview->tool);
    } else if (m_live.tool == InkTool::Highlighter) {
        painter->setRenderHint(QPainter::Antialiasing);
        drawHighlighterStroke(*painter, size(), m_live.points, m_live.color, m_live.width);
    } else if (isInkShape(m_live.tool)) {
        painter->setRenderHint(QPainter::Antialiasing);
        drawInkShape(*painter, size(), m_live.points, m_live.color, m_live.width, m_live.tool);
    }
}
