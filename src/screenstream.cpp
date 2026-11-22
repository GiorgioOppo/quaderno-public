#include "screenstream.h"

#include "screencapture.h"
#include "wirelesssync.h"

#include <QBuffer>
#include <QImageReader>
#include <QMutex>
#include <QPromise>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QScreen>
#include <QThreadPool>
#include <QUuid>
#include <algorithm>
#include <limits>
#include <utility>

struct ScreenStreamFrame {
    QMutex mutex;
    QImage image;
    int orientation = 0;
    quint64 revision = 0;
};

namespace {
const auto viewerMessage = QStringLiteral("screen-viewer");
const auto startMessage = QStringLiteral("screen-start");
const auto stopMessage = QStringLiteral("screen-stop");
const auto frameMessage = QStringLiteral("screen-frame");
const auto ackMessage = QStringLiteral("screen-ack");
const auto pingMessage = QStringLiteral("screen-ping");
const auto pongMessage = QStringLiteral("screen-pong");
const auto navigationPermissionMessage = QStringLiteral("screen-navigation-permission");
const auto tapMessage = QStringLiteral("screen-tap");
const auto keyMessage = QStringLiteral("screen-key");
const auto sectionMessage = QStringLiteral("screen-section");
const auto remoteNavigationField = QStringLiteral("remoteNavigation");
const auto navigationPermissionIdField = QStringLiteral("permissionId");
const auto sectionField = QStringLiteral("section");
const auto sequenceField = QStringLiteral("sequence");
const auto orientationField = QStringLiteral("orientation");
const auto xField = QStringLiteral("x");
const auto yField = QStringLiteral("y");
const auto actionField = QStringLiteral("action");
const auto textField = QStringLiteral("text");
constexpr int captureIntervalMs = 333;
constexpr int ackTimeoutMs = 6000;
constexpr int heartbeatIntervalMs = 2000;
constexpr int livenessTimeoutMs = 8000;
constexpr int maxImageEdge = 1728;
constexpr int maxKeyTextLength = 256;

class StreamImageProvider final : public QQuickImageProvider
{
public:
    explicit StreamImageProvider(std::shared_ptr<ScreenStreamFrame> frame)
        : QQuickImageProvider(QQuickImageProvider::Image), m_frame(std::move(frame)) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        QMutexLocker lock(&m_frame->mutex);
        if (id != QString::number(m_frame->revision))
            return {};
        if (size)
            *size = m_frame->image.size();
        return m_frame->image;
    }

private:
    const std::shared_ptr<ScreenStreamFrame> m_frame;
};

bool validSession(const QString &session)
{
    return session.size() == 32 && std::all_of(session.cbegin(), session.cend(), [](QChar c) {
        return (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            || (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
    });
}

bool validSurface(const QString &surface)
{
    return surface == QStringLiteral("books") || surface == QStringLiteral("writing")
        || surface == QStringLiteral("charts") || surface == QStringLiteral("tables")
        || surface == QStringLiteral("diagrams") || surface == QStringLiteral("settings")
        || surface == QStringLiteral("book") || surface == QStringLiteral("writing-editor")
        || surface == QStringLiteral("chart-editor") || surface == QStringLiteral("table-editor")
        || surface == QStringLiteral("book-note") || surface == QStringLiteral("diagram-editor")
        || surface == QStringLiteral("editor") || surface == QStringLiteral("create");
}

bool validKey(const QString &action, const QString &text)
{
    if (action == QStringLiteral("text")) {
        return !text.isEmpty() && text.size() <= maxKeyTextLength && text.isValidUtf16()
            && std::none_of(text.cbegin(), text.cend(), [](QChar character) {
                const auto category = character.category();
                return category == QChar::Other_Control || category == QChar::Separator_Line
                    || category == QChar::Separator_Paragraph;
            });
    }
    return text.isEmpty() && (action == QStringLiteral("select-all") || action == QStringLiteral("backspace")
        || action == QStringLiteral("delete") || action == QStringLiteral("left") || action == QStringLiteral("right")
        || action == QStringLiteral("home") || action == QStringLiteral("end") || action == QStringLiteral("new-line"));
}

double wireNumber(const QVariant &value)
{
    switch (value.metaType().id()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Float:
    case QMetaType::Double:
        return value.toDouble();
    default:
        return std::numeric_limits<double>::quiet_NaN();
    }
}

bool normalizedCoordinate(qreal value)
{
    return value >= 0 && value < 1;
}

int rightAngle(const QVariant &value)
{
    const double angle = wireNumber(value);
    return angle == 0 || angle == 90 || angle == 180 || angle == 270 ? static_cast<int>(angle) : -1;
}

QByteArray encodeFrame(QImage image)
{
    if (qMax(image.width(), image.height()) > maxImageEdge)
        image = image.scaled(maxImageEdge, maxImageEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QBuffer encoded;
    if (!encoded.open(QIODevice::ReadWrite) || !image.save(&encoded, "JPEG", 76))
        return {};
    if (encoded.size() > WirelessSync::screenStreamPayloadLimit) {
        image = image.scaled(1152, 1152, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        encoded.buffer().clear();
        if (!encoded.seek(0) || !image.save(&encoded, "JPEG", 65))
            return {};
    }
    return encoded.size() <= WirelessSync::screenStreamPayloadLimit ? encoded.data() : QByteArray{};
}

QImage decodeFrame(const QByteArray &encoded)
{
    if (encoded.isEmpty() || encoded.size() > WirelessSync::screenStreamPayloadLimit)
        return {};
    QBuffer buffer;
    buffer.setData(encoded);
    if (!buffer.open(QIODevice::ReadOnly))
        return {};
    QImageReader reader(&buffer, "JPEG");
    reader.setAutoDetectImageFormat(false);
    const QSize size = reader.size();
    if (size.isEmpty() || size.width() > maxImageEdge || size.height() > maxImageEdge)
        return {};
    return reader.read();
}
}

ScreenStream::ScreenStream(WirelessSync *wireless, QObject *parent)
    : QObject(parent), m_wireless(wireless), m_tablet(wireless->tablet()),
      m_frame(std::make_shared<ScreenStreamFrame>())
{
    m_captureTimer.setInterval(captureIntervalMs);
    m_ackDeadline.setSingleShot(true);
    m_ackDeadline.setInterval(ackTimeoutMs);
    m_heartbeat.setInterval(heartbeatIntervalMs);
    m_livenessDeadline.setInterval(livenessTimeoutMs);
    m_livenessDeadline.setSingleShot(true);
    connect(&m_captureTimer, &QTimer::timeout, this, &ScreenStream::capture);
    connect(&m_encoder, &QFutureWatcher<QByteArray>::finished, this, &ScreenStream::sendCapturedFrame);
    connect(&m_ackDeadline, &QTimer::timeout, this, [this] {
        finish(tr("Il Mac non ha confermato la ricezione dello schermo."), true);
    });
    connect(&m_heartbeat, &QTimer::timeout, this, [this] { send(pingMessage, m_session); });
    connect(&m_livenessDeadline, &QTimer::timeout, this, [this] {
        finish(tr("Condivisione interrotta. Ricollega il Wi-Fi per riprovare."), true);
    });
    connect(m_wireless, &WirelessSync::screenStreamLinkChanged, this, &ScreenStream::linkChanged);
    connect(m_wireless, &WirelessSync::screenStreamMessageReceived, this, &ScreenStream::receive);
    linkChanged();
}

QQuickImageProvider *ScreenStream::createImageProvider() const
{
    return new StreamImageProvider(m_frame);
}

void ScreenStream::attach(QQuickWindow *window)
{
    if (!m_tablet || m_window == window)
        return;
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    if (active())
        finish({}, true);
    m_window = window;
    const int orientation = window ? std::max(0, rightAngle(window->property("appliedOrientation"))) : 0;
    m_synchronizedOrientation = orientation;
    m_presentedOrientation = orientation;
    if (window) {
        // Qt blocks the GUI thread during scene synchronization. Sample the applied
        // QML angle there, and publish it only when that scene reaches the frame buffer.
        connect(window, &QQuickWindow::beforeSynchronizing, this, [this, window] {
            m_synchronizedOrientation = std::max(0, rightAngle(window->property("appliedOrientation")));
        }, Qt::DirectConnection);
        connect(window, &QQuickWindow::frameSwapped, this, [this] {
            m_presentedOrientation = m_synchronizedOrientation.load();
        }, Qt::DirectConnection);
        connect(window, &QQuickWindow::frameSwapped, this, [this] { m_frameDirty = true; }, Qt::QueuedConnection);
        connect(window, &QQuickWindow::visibleChanged, this, [this] {
            if (active() && (!m_window || !m_window->isVisible()))
                finish({}, true);
            emit stateChanged();
        });
        connect(window, &QObject::destroyed, this, [this] { finish({}, true); });
    }
    emit stateChanged();
}

void ScreenStream::setBlocked(bool blocked)
{
    if (m_blocked == blocked)
        return;
    m_blocked = blocked;
    if (blocked)
        finish({}, true);
    publishViewer();
    emit stateChanged();
}

bool ScreenStream::available() const
{
    if (m_blocked || !m_linkAvailable)
        return false;
    return !m_tablet || (m_window && m_window->isVisible() && m_peerViewerOpen);
}

QString ScreenStream::status() const
{
    if (m_blocked)
        return tr("Condivisione sospesa mentre Quaderno è bloccato o in standby.");
    if (!m_status.isEmpty())
        return m_status;
    if (!m_linkAvailable)
        return tr("Collega il Paper Pro e il Mac via Wi-Fi da Sincronizzazione wireless.");
    if (active())
        return m_tablet ? tr("Schermo condiviso con il Mac.") : tr("Schermo del Paper Pro in diretta.");
    if (m_tablet)
        return m_peerViewerOpen ? tr("Il Mac è pronto a ricevere lo schermo.")
                                : tr("Apri la finestra Schermo del Paper Pro in Quaderno sul Mac.");
    return tr("In attesa che tu avvii la condivisione sul Paper Pro.");
}

QUrl ScreenStream::frameSource() const
{
    QMutexLocker lock(&m_frame->mutex);
    return m_frame->image.isNull() ? QUrl{} : QUrl(QStringLiteral("image://screen-stream/%1").arg(m_frame->revision));
}

int ScreenStream::frameOrientation() const
{
    QMutexLocker lock(&m_frame->mutex);
    return m_frame->orientation;
}

QRect ScreenStream::viewerAvailableArea(QWindow *window) const
{
    const auto *screen = window ? window->screen() : nullptr;
    return screen ? screen->availableGeometry().marginsRemoved(window->frameMargins()) : QRect{};
}

void ScreenStream::start()
{
    startSession(false);
}

void ScreenStream::startWithRemoteNavigation()
{
    startSession(true);
}

void ScreenStream::startSession(bool allowRemoteNavigation)
{
    if (!m_tablet || active() || !available())
        return;
    m_session = QUuid::createUuid().toString(QUuid::Id128);
    m_status.clear();
    m_frameDirty = true;
    QVariantMap fields;
    if (allowRemoteNavigation)
        fields.insert(remoteNavigationField, true);
    if (!send(startMessage, m_session, fields)) {
        finish(tr("Impossibile iniziare la condivisione via Wi-Fi."));
        return;
    }
    if (active()) {
        m_captureTimer.start();
        m_heartbeat.start();
        m_livenessDeadline.start();
        m_remoteNavigationAllowed = allowRemoteNavigation;
        if (m_remoteNavigationAllowed)
            emit navigationChanged();
    }
    emit stateChanged();
}

void ScreenStream::stop()
{
    if (!m_tablet) {
        setViewerOpen(false);
        return;
    }
    finish({}, true);
}

void ScreenStream::setViewerOpen(bool open)
{
    if (m_tablet || m_viewerOpen == open)
        return;
    m_viewerOpen = open;
    m_status.clear();
    if (!open)
        finish({}, true);
    publishViewer();
    emit stateChanged();
}

void ScreenStream::setRemoteNavigationAllowed(bool allowed)
{
    if (!m_tablet || !active() || !available() || m_remoteNavigationAllowed == allowed)
        return;
    // A fresh ID prevents queued commands from an earlier grant taking effect after re-enabling.
    const auto permissionId = QUuid::createUuid().toString(QUuid::Id128);
    if (!send(navigationPermissionMessage, m_session,
              {{remoteNavigationField, allowed}, {navigationPermissionIdField, permissionId}})) {
        if (active())
            finish(tr("Condivisione interrotta dal collegamento Wi-Fi."));
        return;
    }
    m_navigationPermissionId = permissionId;
    m_remoteNavigationAllowed = allowed;
    m_localSection.clear();
    emit navigationChanged();
}

bool ScreenStream::canSendNavigation() const
{
    return !m_tablet && m_viewerOpen && m_remoteNavigationAllowed && m_sequence > 0 && !m_remoteSection.isEmpty();
}

void ScreenStream::requestTap(qreal x, qreal y)
{
    if (!canSendNavigation() || !normalizedCoordinate(x) || !normalizedCoordinate(y))
        return;
    send(tapMessage, m_session, {{sectionField, m_remoteSection}, {sequenceField, m_sequence},
        {navigationPermissionIdField, m_navigationPermissionId}, {xField, x}, {yField, y}});
}

void ScreenStream::requestKey(const QString &action, const QString &text)
{
    if (!canSendNavigation() || !validKey(action, text))
        return;
    send(keyMessage, m_session, {{sectionField, m_remoteSection}, {navigationPermissionIdField, m_navigationPermissionId},
        {actionField, action}, {textField, text}});
}

void ScreenStream::reportSection(const QString &section)
{
    if (!m_tablet || !m_remoteNavigationAllowed || (!section.isEmpty() && !validSurface(section)))
        return;
    m_localSection = section;
    send(sectionMessage, m_session, {{sectionField, section}, {navigationPermissionIdField, m_navigationPermissionId}});
}

void ScreenStream::linkChanged()
{
    const bool previous = m_linkAvailable;
    m_linkAvailable = m_wireless->screenStreamAvailable();
    if (!m_linkAvailable) {
        m_peerViewerOpen = false;
        finish(active() ? tr("Condivisione interrotta. Ricollega il Wi-Fi per riprovare.") : QString{});
    } else if (!previous) {
        m_status.clear();
        publishViewer();
    }
    emit stateChanged();
}

bool ScreenStream::send(const QString &type, const QString &session, QVariantMap fields, const QByteArray &payload)
{
    fields.insert(QStringLiteral("type"), type);
    if (!session.isEmpty())
        fields.insert(QStringLiteral("session"), session);
    return m_wireless->sendScreenStreamMessage(fields, payload);
}

void ScreenStream::publishViewer()
{
    if (!m_tablet && m_linkAvailable)
        send(viewerMessage, {}, {{QStringLiteral("ready"), m_viewerOpen && !m_blocked}});
}

void ScreenStream::receive(const QVariantMap &message, const QByteArray &payload)
{
    if (!m_linkAvailable)
        return;
    const QString type = message.value(QStringLiteral("type")).toString();
    const QString session = message.value(QStringLiteral("session")).toString();
    if (type == viewerMessage && m_tablet && payload.isEmpty()) {
        const QVariant ready = message.value(QStringLiteral("ready"));
        if (ready.metaType().id() != QMetaType::Bool)
            return;
        m_peerViewerOpen = ready.toBool();
        m_status.clear();
        if (!m_peerViewerOpen && active())
            finish({}, true);
        emit stateChanged();
        return;
    }
    if (m_blocked || !validSession(session))
        return;
    if (type == startMessage && !m_tablet && payload.isEmpty()) {
        if (!m_viewerOpen || active()) {
            if (session != m_session)
                send(stopMessage, session);
            return;
        }
        m_session = session;
        m_status.clear();
        m_livenessDeadline.start();
        const QVariant permission = message.value(remoteNavigationField);
        m_remoteNavigationAllowed = permission.metaType().id() == QMetaType::Bool && permission.toBool();
        if (m_remoteNavigationAllowed)
            emit navigationChanged();
        emit stateChanged();
        return;
    }
    if (session != m_session)
        return;
    if (type == stopMessage && payload.isEmpty()) {
        finish();
        return;
    }
    if (!m_tablet && type == pingMessage && payload.isEmpty()) {
        m_livenessDeadline.start();
        send(pongMessage, session);
        return;
    }
    if (m_tablet && type == pongMessage && payload.isEmpty()) {
        m_livenessDeadline.start();
        return;
    }
    if (!m_tablet && type == navigationPermissionMessage && payload.isEmpty()) {
        const QVariant permission = message.value(remoteNavigationField);
        const QString permissionId = message.value(navigationPermissionIdField).toString();
        if (permission.metaType().id() != QMetaType::Bool || !validSession(permissionId)
            || permissionId == m_navigationPermissionId)
            return;
        m_navigationPermissionId = permissionId;
        m_remoteNavigationAllowed = permission.toBool();
        m_remoteSection.clear();
        emit navigationChanged();
        return;
    }
    if (type == tapMessage || type == keyMessage || type == sectionMessage) {
        const QVariant value = message.value(sectionField);
        if (!m_remoteNavigationAllowed || !payload.isEmpty() || value.metaType().id() != QMetaType::QString
            || message.value(navigationPermissionIdField).toString() != m_navigationPermissionId)
            return;
        const QString section = value.toString();
        if (type == sectionMessage) {
            if (!m_tablet && (section.isEmpty() || validSurface(section)) && section != m_remoteSection) {
                m_remoteSection = section;
                emit navigationChanged();
            }
            return;
        }
        if (!m_tablet || m_localSection.isEmpty() || section != m_localSection || m_sequence <= 0)
            return;
        if (type == tapMessage) {
            const auto x = wireNumber(message.value(xField));
            const auto y = wireNumber(message.value(yField));
            if (wireNumber(message.value(sequenceField)) == m_sequence && normalizedCoordinate(x) && normalizedCoordinate(y))
                emit remoteTapRequested(x, y);
            return;
        }
        const QVariant action = message.value(actionField);
        const QVariant text = message.value(textField);
        if (action.metaType().id() == QMetaType::QString && text.metaType().id() == QMetaType::QString
            && validKey(action.toString(), text.toString()))
            emit remoteKeyRequested(action.toString(), text.toString());
        return;
    }

    bool number = false;
    const double seq = message.value(QStringLiteral("sequence")).toDouble(&number);
    if (!number)
        return;
    if (m_tablet && type == ackMessage && payload.isEmpty() && seq == m_sequence) {
        m_ackDeadline.stop();
        return;
    }
    if (m_tablet || type != frameMessage || !m_viewerOpen || m_sequence == std::numeric_limits<int>::max()
        || seq != m_sequence + 1)
        return;
    const int orientation = message.contains(orientationField) ? rightAngle(message.value(orientationField)) : 0;
    auto image = orientation >= 0 ? decodeFrame(payload) : QImage{};
    if (image.isNull()) {
        finish(tr("Il fotogramma ricevuto non è valido. Avvia di nuovo la condivisione."), true);
        return;
    }
    ++m_sequence;
    {
        QMutexLocker lock(&m_frame->mutex);
        m_frame->image = std::move(image);
        m_frame->orientation = orientation;
        ++m_frame->revision;
    }
    send(ackMessage, session, {{QStringLiteral("sequence"), m_sequence}});
    emit frameChanged();
}

void ScreenStream::capture()
{
    if (m_blocked || !m_linkAvailable || !m_window || !active() || m_encoding
        || m_ackDeadline.isActive() || !m_frameDirty)
        return;
    auto frame = captureScreen(m_window);
    if (frame.isNull()) {
        if (++m_emptyCaptures >= 15)
            finish(tr("Quaderno non riesce a leggere il fotogramma del Paper Pro."), true);
        return;
    }
    m_emptyCaptures = 0;
    m_frameDirty = false;
    // The E Ink window is read on the GUI thread. Resize and compress its detached copy elsewhere.
    m_encoding = true;
    m_encodingSession = m_session;
    m_encodingOrientation = m_presentedOrientation.load();
    auto promise = std::make_shared<QPromise<QByteArray>>();
    promise->start();
    m_encoder.setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise, frame = std::move(frame)]() mutable {
        promise->addResult(encodeFrame(std::move(frame)));
        promise->finish();
    });
}

void ScreenStream::sendCapturedFrame()
{
    m_encoding = false;
    if (m_blocked || !active() || m_encodingSession != m_session)
        return;
    const auto encoded = m_encoder.result();
    if (encoded.isEmpty()) {
        finish(tr("Impossibile preparare l’immagine dello schermo."), true);
        return;
    }
    if (encoded == m_lastCapture && m_encodingOrientation == m_lastCaptureOrientation)
        return;
    if (m_sequence == std::numeric_limits<int>::max()) {
        finish({}, true);
        return;
    }
    m_lastCapture = encoded;
    m_lastCaptureOrientation = m_encodingOrientation;
    ++m_sequence;
    m_ackDeadline.start();
    if (!send(frameMessage, m_session, {{sequenceField, m_sequence}, {orientationField, m_encodingOrientation}}, encoded))
        finish(tr("Condivisione interrotta dal collegamento Wi-Fi."));
}

void ScreenStream::finish(const QString &message, bool notifyPeer)
{
    const auto previous = std::exchange(m_session, {});
    const bool hadRemoteNavigation = std::exchange(m_remoteNavigationAllowed, false);
    m_navigationPermissionId.clear();
    m_localSection.clear();
    m_remoteSection.clear();
    m_captureTimer.stop();
    m_ackDeadline.stop();
    m_heartbeat.stop();
    m_livenessDeadline.stop();
    m_sequence = 0;
    m_emptyCaptures = 0;
    m_frameDirty = false;
    m_lastCapture.clear();
    m_lastCaptureOrientation = 0;
    m_status = message;
    bool hadImage = false;
    {
        QMutexLocker lock(&m_frame->mutex);
        hadImage = !m_frame->image.isNull();
        m_frame->image = {};
        m_frame->orientation = 0;
        ++m_frame->revision;
    }
    if (hadImage)
        emit frameChanged();
    if (notifyPeer && !previous.isEmpty())
        send(stopMessage, previous);
    if (hadRemoteNavigation)
        emit navigationChanged();
    emit stateChanged();
}
