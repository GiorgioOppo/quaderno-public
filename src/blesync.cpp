#include "blesync.h"

#include "notebook.h"

#include <QDateTime>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>

#include <utility>
#include <limits>

namespace {
constexpr int maxPins = 20;
constexpr int maxBaselines = 256;
constexpr int transferTimeout = 15 * 60 * 1000;
constexpr int reviewTimeout = 5 * 60 * 1000;
constexpr int resumeTimeout = 2 * 60 * 1000;
constexpr int automaticDelay = 1700;
constexpr int pendingRetryDelay = 1900;
constexpr int editWorkDelay = 400;
constexpr int maxAutomaticRetries = 3;
constexpr qsizetype mebibyte = 1024 * 1024;
constexpr qsizetype bluetoothPayloadLimit = 8 * mebibyte;

const QString previewType = QStringLiteral("preview");
const QString applyType = QStringLiteral("apply");
const QString previewReplyType = QStringLiteral("previewReply");
const QString applyReplyType = QStringLiteral("applyReply");
const QString cancelType = QStringLiteral("cancel");
const QString resumeType = QStringLiteral("resume");
const QString resumeReplyType = QStringLiteral("resumeReply");
const QString resumeCancelType = QStringLiteral("resumeCancel");

bool hexString(const QString &value, qsizetype size)
{
    const QByteArray hex = value.toLatin1();
    return hex.size() == size && QByteArray::fromHex(hex).toHex() == hex;
}

bool validBook(const QString &id)
{
    return !id.isEmpty() && id.size() <= 256;
}

bool validAction(const QString &action)
{
    return action == QStringLiteral("create") || action == QStringLiteral("replace")
        || action == QStringLiteral("unchanged") || action == QStringLiteral("conflict");
}

QVariantMap problem(const QString &message)
{
    return {{QStringLiteral("error"), message}};
}

QString displayText(const QVariant &value)
{
    static const QRegularExpression controls(QStringLiteral("[\\x{0000}-\\x{001f}\\x{007f}-\\x{009f}\\x{202a}-\\x{202e}\\x{2066}-\\x{2069}]"));
    return value.toString().left(360).remove(controls);
}
}

QSettings BleSync::syncSettings() const
{
    if (!m_stateDirectory.isEmpty())
        return QSettings(m_stateDirectory + QStringLiteral("/preferences.ini"), QSettings::IniFormat);
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
}

BleSync::BleSync(Notebook *notebook, QObject *parent, QString stateDirectory)
    : QObject(parent), m_notebook(notebook), m_delta([this](const QString &id) {
        return m_notebook && m_notebook->hasBookId(id) ? m_notebook->exportBookForTransfer(id) : QByteArray{};
    }, this, stateDirectory.isEmpty() ? QString{} : stateDirectory + QStringLiteral("/objects")),
      m_stateDirectory(std::move(stateDirectory))
{
    m_retryClock.start();
    connect(&m_delta, &DeltaTransfer::sendMessage, this, &BleSync::sendMessage);
    connect(&m_delta, &DeltaTransfer::previewReady, this, [this](const QVariantMap &metadata, const QByteArray &payload) {
        receivePreview(metadata.value(QStringLiteral("request")).toString(), metadata, payload, true);
    });
    connect(&m_delta, &DeltaTransfer::failed, this, [this](const QString &request, const QString &error) {
        if (request == m_request)
            finishOutgoing(error);
    });
    connect(&m_delta, &DeltaTransfer::progressChanged, this, [this] {
        if (m_stage == Stage::Preview)
            m_outgoingTimer.start(transferTimeout);
        emit changed();
    });
    m_outgoingTimer.setSingleShot(true);
    m_incomingTimer.setSingleShot(true);
    m_autoTimer.setSingleShot(true);
    m_routingStateTimer.setSingleShot(true);
    m_pendingTransferTimer.setSingleShot(true);
    m_deferredTimer.setSingleShot(true);
    m_bookStatusTimer.setSingleShot(true);
    m_outgoingResumeTimer.setSingleShot(true);
    m_incomingResumeTimer.setSingleShot(true);
    connect(&m_outgoingTimer, &QTimer::timeout, this, [this] {
        const QString request = m_request;
        const bool wasApplying = applying();
        if (m_stage == Stage::Preview) queueAutomaticRetry();
        finishOutgoing(wasApplying ? tr("Il trasferimento è scaduto. Controlla l’altro dispositivo prima di riprovare.")
                                   : tr("L’anteprima del trasferimento è scaduta."));
        if (m_connected && !wasApplying)
            emit sendMessage({{QStringLiteral("type"), cancelType}, {QStringLiteral("request"), request}}, {});
    });
    connect(&m_incomingTimer, &QTimer::timeout, this, &BleSync::resetIncoming);
    connect(&m_autoTimer, &QTimer::timeout, this, &BleSync::runAutoSync);
    connect(&m_delta, &DeltaTransfer::receivingChanged, this, [this] { scheduleRoutingState(); });
    connect(&m_routingStateTimer, &QTimer::timeout, this, [this] {
        const bool busy = transportSelectionBusy();
        if (m_notifiedTransportBusy != busy) {
            m_notifiedTransportBusy = busy;
            emit transportSelectionBusyChanged();
        }
        const bool work = hasSyncWork();
        if (m_notifiedSyncWork != work) {
            m_notifiedSyncWork = work;
            emit syncWorkChanged();
        }
    });
    connect(&m_pendingTransferTimer, &QTimer::timeout, this, [this] {
        m_retryPendingOnConnection = false;
        retryPendingTransfer();
        scheduleRoutingState();
        scheduleAutoSync();
    });
    connect(&m_deferredTimer, &QTimer::timeout, this, &BleSync::dispatchDeferredTransfer);
    connect(&m_bookStatusTimer, &QTimer::timeout, this, &BleSync::updateBookStatus);
    connect(&m_outgoingResumeTimer, &QTimer::timeout, this, [this] {
        finishResume(tr("La richiesta di ripresa è scaduta. Controlla l’altro dispositivo."), true);
    });
    connect(&m_incomingResumeTimer, &QTimer::timeout, this, [this] {
        replyToResume(QStringLiteral("expired"));
    });
    if (notebook) {
        connect(notebook, &Notebook::booksChanged, this, &BleSync::noteBookChanges);
        connect(notebook, &Notebook::documentChanged, this, &BleSync::noteBookChanges);
        connect(notebook, &Notebook::pageContentChanged, this, &BleSync::noteBookChanges);
        connect(notebook, &Notebook::strokesChanged, this, &BleSync::noteBookChanges);
        connect(notebook, &Notebook::layersChanged, this, &BleSync::noteBookChanges);
    }
}

void BleSync::setPeerIdentity(const QString &id)
{
    if (id == m_peer || (!id.isEmpty() && !hexString(id, 32)))
        return;
    const QStringList previousPins = m_pins;
    setConnected(false);
    m_peer = id;
    m_pins.clear();
    m_baselines.clear();
    m_lastSuccess.clear();
    m_applyReceipts.clear();
    m_conflictCopies.clear();
    m_uncertainAutomaticBooks.clear();
    m_automaticQueue.clear();
    m_batchRevisions.clear();
    m_pendingTransferBook.clear();
    m_pendingTransferRevision.clear();
    m_deferredPreviewBook.clear();
    m_deferredResume = {};
    m_retryPendingOnConnection = false;
    m_pendingTransferAutomatic = false;
    m_pendingTransferApplying = false;
    m_bookIssues.clear();
    m_autoAttempts.clear();
    m_autoSync = false;
    m_status.clear();
    m_resumeStatus.clear();
    if (!id.isEmpty()) {
        auto settings = syncSettings();
        settings.beginGroup(QStringLiteral("nearbySync/") + id);
        m_autoSync = settings.value(QStringLiteral("automatic"), false).toBool();
        for (const QString &pin : settings.value(QStringLiteral("pinnedBooks")).toStringList()) {
            if (validBook(pin) && !m_pins.contains(pin) && m_pins.size() < maxPins)
                m_pins.append(pin);
        }
        const auto stored = settings.value(QStringLiteral("baselines")).toMap();
        const auto dates = settings.value(QStringLiteral("lastSuccess")).toMap();
        for (const QString &book : settings.value(QStringLiteral("uncertainAutomaticBooks")).toStringList())
            if (validBook(book) && !m_uncertainAutomaticBooks.contains(book) && m_uncertainAutomaticBooks.size() < maxBaselines)
                m_uncertainAutomaticBooks.append(book);
        const auto copied = settings.value(QStringLiteral("conflictCopies")).toMap();
        for (auto it = copied.cbegin(); it != copied.cend() && m_conflictCopies.size() < maxBaselines; ++it)
            if (validBook(it.key()) && hexString(it.value().toString(), 64)) m_conflictCopies.insert(it.key(), it.value());
        const QString pending = settings.value(QStringLiteral("pendingTransferBook")).toString();
        const QString pendingRevision = settings.value(QStringLiteral("pendingTransferRevision")).toString();
        if (validBook(pending)) {
            m_pendingTransferBook = pending;
            m_pendingTransferAutomatic = settings.value(QStringLiteral("pendingTransferAutomatic"), false).toBool();
            const bool checkpointKnown = settings.contains(QStringLiteral("pendingTransferApplying"))
                && hexString(pendingRevision, 64);
            m_pendingTransferApplying = !checkpointKnown
                || settings.value(QStringLiteral("pendingTransferApplying"), false).toBool();
            // A crash may occur after the receiver commits but before its reply.
            // Legacy checkpoints lack a phase/revision. Treat those as unknown,
            // including manual transfers: automatic catch-up must not repeat them.
            if (m_pendingTransferApplying
                && !m_uncertainAutomaticBooks.contains(pending)
                && m_uncertainAutomaticBooks.size() < maxBaselines)
                m_uncertainAutomaticBooks.append(pending);
        }
        const auto batch = settings.value(QStringLiteral("automaticBatchRevisions")).toMap();
        for (const QString &book : settings.value(QStringLiteral("automaticQueue")).toStringList()) {
            const QString revision = batch.value(book).toString();
            if (!m_pins.contains(book) || m_automaticQueue.contains(book)
                || m_uncertainAutomaticBooks.contains(book) || !hexString(revision, 64)) continue;
            m_automaticQueue.append(book);
            m_batchRevisions.insert(book, revision);
        }
        // The active preview is removed from the queue while it runs. Restore
        // only its exact saved revision; changed content needs a new trigger.
        if (hexString(pendingRevision, 64)) m_pendingTransferRevision = pendingRevision;
        if (m_pendingTransferAutomatic && !m_pendingTransferApplying && m_pins.contains(pending)
            && !m_uncertainAutomaticBooks.contains(pending) && hexString(pendingRevision, 64)
            && !m_automaticQueue.contains(pending)) {
            m_automaticQueue.prepend(pending);
            m_batchRevisions.insert(pending, pendingRevision);
        }
        const auto receipts = settings.value(QStringLiteral("applyReceipts")).toMap();
        for (auto receipt = receipts.cbegin(); receipt != receipts.cend() && m_applyReceipts.size() < 64; ++receipt) {
            const auto stored = receipt.value().toMap();
            if (hexString(receipt.key(), 32) && hexString(stored.value(QStringLiteral("token")).toString(), 64)
                && !stored.value(QStringLiteral("result")).toMap().isEmpty())
                m_applyReceipts.insert(receipt.key(), stored);
        }
        for (auto entry = stored.cbegin(); entry != stored.cend() && m_baselines.size() < maxBaselines; ++entry) {
            if (validBook(entry.key()) && hexString(entry.value().toString(), 64)) {
                m_baselines.insert(entry.key(), entry.value());
                const QDateTime date = dates.value(entry.key()).toDateTime();
                if (date.isValid())
                    m_lastSuccess.insert(entry.key(), date);
            }
        }
    }
    updateBookStatus();
    scheduleRoutingState();
    emit resumeChanged();
    // Automation reads this list while synchronously updating transfer gates.
    // A policy/status notification must not invalidate that input binding.
    if (previousPins != m_pins) emit pinnedBooksChanged();
    emit changed();
}

void BleSync::setPayloadLimit(qsizetype bytes)
{
    if (!m_connected)
        m_payloadLimit = qBound(qsizetype{1}, bytes, qsizetype{64 * 1024 * 1024});
}

bool BleSync::transportSelectionBusy() const
{
    // The approval token and the in-progress preview belong to this channel.
    // A deferred local intent has not sent anything and can follow a new link.
    return m_stage != Stage::Idle || !m_incomingRequest.isEmpty() || m_delta.receiving()
        || !m_outgoingResume.bookId.isEmpty() || !m_incomingResume.request.isEmpty();
}

bool BleSync::setTransportSelectionPaused(bool paused)
{
    if (paused && transportSelectionBusy())
        return false;
    if (m_transportSelectionPaused == paused)
        return true;
    m_transportSelectionPaused = paused;
    if (paused) {
        m_autoTimer.stop();
        m_pendingTransferTimer.stop();
        m_deferredTimer.stop();
    } else if (m_connected) {
        if (hasDeferredTransfer()) m_deferredTimer.start(0);
        schedulePendingRetry();
        scheduleAutoSync();
    }
    return true;
}

void BleSync::cancelDeferredTransfer()
{
    if (!hasDeferredTransfer())
        return;
    m_deferredTimer.stop();
    if (!m_deferredResume.bookId.isEmpty()) {
        finishResume({});
        return;
    }
    m_deferredPreviewBook.clear();
    m_status.clear();
    scheduleRoutingState();
    scheduleAutoSync();
    emit changed();
}

void BleSync::forgetPeer()
{
    if (m_peer.isEmpty())
        return;
    const QString forgotten = m_peer;
    m_delta.forgetPeer(forgotten);
    // Disconnecting persists interrupted work. Remove the group only after
    // that cleanup, otherwise it could recreate a forgotten peer's queue.
    setPeerIdentity({});
    auto settings = syncSettings();
    settings.remove(QStringLiteral("nearbySync/") + forgotten);
    settings.sync();
}

void BleSync::setConnected(bool connected)
{
    if (m_connected == connected || (connected && m_peer.isEmpty()))
        return;
    const bool wasApplying = applying();
    if (!connected && m_automatic) {
        if (!wasApplying && m_triggeredAutomaticSync && m_pins.contains(m_bookId) && !m_automaticQueue.contains(m_bookId)) {
            m_automaticQueue.prepend(m_bookId);
            m_batchRevisions.insert(m_bookId, m_revision);
        }
    }
    m_connected = connected;
    m_retryPendingOnConnection = connected && !m_pendingTransferAutomatic && hasPendingTransfer();
    // A successful reconnect must permit a fresh preview, but must not erase
    // a retry budget or its backoff while the radio repeatedly disconnects.
    if (!connected && m_automatic && !wasApplying) queueAutomaticRetry();
    resetIncoming();
    m_autoTimer.stop();
    m_pendingTransferTimer.stop();
    m_deferredTimer.stop();
    if (!connected && !m_outgoingResume.bookId.isEmpty())
        finishResume(tr("Connessione interrotta prima della conferma di ripresa."));
    clearResumeOffer();
    finishOutgoing(!connected && wasApplying
        ? tr("Connessione interrotta durante l’invio. Controlla l’altro dispositivo prima di riprovare.") : QString{});
    m_delta.setConnected(connected, m_peer);
    if (connected && !m_transportSelectionPaused) {
        if (hasDeferredTransfer()) m_deferredTimer.start(0);
        schedulePendingRetry();
    }
}

void BleSync::retryPendingTransfer()
{
    if ((!m_connected && !m_transportSelectionPaused) || !m_notebook || m_editorBusy
        || m_stage != Stage::Idle || m_pendingTransferBook.isEmpty())
        return;
    // Reconstruct a new preview from the current content. A previous approval is
    // never replayed after a connection loss or an application restart.
    preview(m_pendingTransferBook);
}

void BleSync::preview(const QString &bookId)
{
    if ((!m_connected && !m_transportSelectionPaused) || m_peer.isEmpty() || !m_notebook || busy())
        return;
    if (m_transportSelectionPaused && (!validBook(bookId) || !m_notebook->hasBookId(bookId)))
        return;
    if (!m_deferredResume.bookId.isEmpty())
        finishResume({});
    if (m_transportSelectionPaused) {
        m_deferredPreviewBook = bookId;
        m_status = tr("Scelta del collegamento per la sincronizzazione…");
        scheduleRoutingState();
        emit changed();
        return;
    }
    m_deferredPreviewBook.clear();
    if (m_stage == Stage::Review)
        cancel();
    beginPreview(bookId, m_notebook->bookTransferRevision(bookId), false);
}

void BleSync::beginPreview(const QString &bookId, const QString &revision, bool automatic)
{
    m_pendingTransferTimer.stop();
    m_retryPendingOnConnection = false;
    m_completed = false;
    m_bookId = bookId;
    m_revision = revision;
    if (!validBook(bookId) || !hexString(revision, 64)) {
        finishOutgoing(tr("Scegli un quaderno disponibile da sincronizzare."));
        return;
    }
    m_payload = m_notebook->exportBookForTransfer(bookId);
    if (m_payload.isEmpty() || m_payload.size() > SyncPackage::defaultMaximumBytes) {
        const QString error = m_notebook->error();
        if (!error.isEmpty())
            finishOutgoing(error);
        else
            finishOutgoing(tr("Il quaderno supera il limite di trasferimento di 64 MiB."));
        return;
    }
    m_stage = Stage::Preview;
    m_request = QUuid::createUuid().toString(QUuid::Id128);
    m_automatic = automatic;
    m_pendingTransferBook = bookId;
    m_pendingTransferRevision = revision;
    m_pendingTransferAutomatic = automatic;
    m_pendingTransferApplying = false;
    savePreferences();
    m_status = tr("Controllo della versione sull’altro dispositivo…");
    m_outgoingTimer.start(transferTimeout);
    updateBookStatus();
    scheduleRoutingState();
    emit changed();
    if (m_stage == Stage::Preview) {
        const QByteArray payload = m_payload;
        m_delta.sendPreview({{QStringLiteral("type"), previewType}, {QStringLiteral("request"), m_request},
                            {QStringLiteral("bookId"), bookId}, {QStringLiteral("base"), m_baselines.value(bookId).toString()}},
                           payload, m_payloadLimit);
    }
}

void BleSync::apply()
{
    if (!m_connected || m_stage != Stage::Review || !m_notebook)
        return;
    if (m_automatic && (!m_autoSync || !m_automaticAllowed || m_editorBusy || !m_pins.contains(m_bookId)))
        return;
    if (m_notebook->exportBookForTransfer(m_bookId) != m_payload) {
        const QString request = m_request;
        finishOutgoing(tr("Il quaderno è cambiato. Controlla una nuova anteprima."));
        emit sendMessage({{QStringLiteral("type"), cancelType}, {QStringLiteral("request"), request}}, {});
        return;
    }
    m_pendingTransferApplying = true;
    if (!savePreferences()) {
        m_pendingTransferApplying = false;
        m_status = tr("Impossibile salvare lo stato del trasferimento. Libera spazio o controlla i permessi, poi riprova dall’anteprima.");
        emit changed();
        return;
    }
    m_stage = Stage::Apply; // The uncertain-commit checkpoint is durable before sending.
    m_status = tr("Invio del quaderno…");
    m_outgoingTimer.start(transferTimeout);
    updateBookStatus();
    emit changed();
    if (applying()) {
        emit sendMessage({{QStringLiteral("type"), applyType}, {QStringLiteral("request"), m_request},
                          {QStringLiteral("token"), m_token}}, {});
    }
}

void BleSync::cancel()
{
    if (applying())
        return;
    const QString request = m_request;
    m_pendingTransferTimer.stop();
    m_retryPendingOnConnection = false;
    m_deferredPreviewBook.clear();
    m_pendingTransferBook.clear();
    m_pendingTransferRevision.clear();
    m_pendingTransferAutomatic = false;
    m_pendingTransferApplying = false;
    savePreferences();
    finishOutgoing({});
    if (m_connected && !request.isEmpty())
        emit sendMessage({{QStringLiteral("type"), cancelType}, {QStringLiteral("request"), request}}, {});
}

void BleSync::setPinned(const QString &bookId, bool pinned)
{
    if (m_peer.isEmpty() || m_pins.contains(bookId) == pinned)
        return;
    if (pinned) {
        if (!validBook(bookId) || !m_notebook || m_notebook->bookTransferRevision(bookId).isEmpty())
            return;
        if (m_pins.size() >= maxPins) {
            m_status = tr("Puoi selezionare al massimo %1 quaderni per la sincronizzazione automatica.").arg(maxPins);
            emit changed();
            return;
        }
        m_pins.append(bookId);
    } else {
        m_pins.removeAll(bookId);
        m_autoAttempts.remove(bookId);
        m_automaticQueue.removeAll(bookId);
        m_batchRevisions.remove(bookId);
        if (m_automatic && m_bookId == bookId && !applying()) cancel();
    }
    savePreferences();
    emit pinnedBooksChanged();
    emit changed();
    updateAutomaticState();
    scheduleRoutingState();
    scheduleAutoSync();
}

void BleSync::setAutoSync(bool enabled)
{
    if (m_peer.isEmpty() || m_autoSync == enabled)
        return;
    m_autoSync = enabled;
    if (enabled) m_autoAttempts.clear();
    if (!enabled) {
        m_autoTimer.stop();
        m_automaticQueue.clear();
        m_batchRevisions.clear();
        if (m_automatic && !applying())
            cancel();
    }
    savePreferences();
    emit changed();
    updateAutomaticState();
    scheduleRoutingState();
    scheduleAutoSync();
}

void BleSync::setAutomaticAllowed(bool allowed)
{
    if (m_automaticAllowed == allowed) return;
    m_automaticAllowed = allowed;
    if (!allowed) {
        m_autoTimer.stop();
        pauseAutomaticPreview();
    }
    emit changed();
    scheduleRoutingState();
    scheduleAutoSync();
}

void BleSync::setTriggeredAutomaticSync(bool triggered)
{
    if (m_triggeredAutomaticSync == triggered) return;
    m_triggeredAutomaticSync = triggered;
    m_autoTimer.stop();
    pauseAutomaticPreview();
    if (!triggered) {
        m_automaticQueue.clear();
        m_batchRevisions.clear();
    }
    savePreferences();
    emit changed();
    updateAutomaticState();
    scheduleRoutingState();
    scheduleAutoSync();
}

void BleSync::setConflictPolicy(const QString &policy)
{
    if ((policy != QStringLiteral("ask") && policy != QStringLiteral("keepBoth")) || m_conflictPolicy == policy) return;
    m_conflictPolicy = policy;
    emit changed();
}

bool BleSync::requestAutomaticSync()
{
    if (!m_autoSync || !m_automaticAllowed || !m_connected || !m_notebook || m_editorBusy || m_stage != Stage::Idle)
        return false;
    // Requests merge into one bounded batch. Only a new opening/interval trigger
    // refreshes its snapshot; subsequent edits do not become continuous sync.
    if (m_triggeredAutomaticSync) {
        for (const QString &bookId : m_pins) {
            const QString revision = automaticRevision(bookId);
            if (revision.isEmpty()) continue;
            const auto attempt = m_autoAttempts.constFind(bookId);
            // Startup/reconnect/interval triggers are not explicit permission
            // to restart an exhausted same-revision retry budget.
            if (attempt != m_autoAttempts.cend() && attempt->revision == revision && !attempt->retry) continue;
            if (!m_automaticQueue.contains(bookId)) m_automaticQueue.append(bookId);
            m_batchRevisions.insert(bookId, revision);
            if (attempt != m_autoAttempts.cend() && attempt->revision != revision)
                m_autoAttempts.remove(bookId);
        }
    }
    savePreferences();
    updateAutomaticState();
    scheduleAutoSync();
    return true;
}

void BleSync::pauseAutomaticPreview()
{
    if (!m_automatic || applying()) return;
    const QString bookId = m_bookId;
    const QString revision = m_revision;
    if (m_triggeredAutomaticSync && m_pins.contains(bookId) && !m_automaticQueue.contains(bookId)) {
        m_automaticQueue.prepend(bookId);
        m_batchRevisions.insert(bookId, revision);
    }
    AutoAttempt &attempt = m_autoAttempts[bookId];
    if (attempt.revision != revision) attempt = {revision};
    attempt.retry = true;
    attempt.retryAt = qMax(attempt.retryAt, m_retryClock.elapsed());
    cancel();
}

void BleSync::receiveMessage(const QVariantMap &message, const QByteArray &payload)
{
    if (!m_connected || !m_notebook)
        return;
    if (m_delta.receiveMessage(message, payload))
        return;
    const QString request = message.value(QStringLiteral("request")).toString();
    const QString type = message.value(QStringLiteral("type")).toString();
    if (!hexString(request, 32))
        return;
    if (type == previewType) {
        receivePreview(request, message, payload);
    } else if (payload.isEmpty()) {
        if (type == applyType)
            receiveApply(request, message);
        else if (type == cancelType) {
            m_delta.cancelIncoming(request);
            if (request == m_incomingRequest)
                resetIncoming();
        }
        else if (type == resumeType)
            receiveResume(request, message);
        else if (type == resumeReplyType)
            receiveResumeReply(request, message);
        else if (type == resumeCancelType && request == m_incomingResume.request)
            clearResumeOffer();
        else if (request == m_request && (type == previewReplyType || type == applyReplyType))
            receiveReply(type, message);
    }
}

void BleSync::receivePreview(const QString &request, const QVariantMap &message, const QByteArray &payload, bool incremental)
{
    const QString bookId = message.value(QStringLiteral("bookId")).toString();
    const QString base = message.value(QStringLiteral("base")).toString();
    if (payload.isEmpty() || payload.size() > (incremental ? SyncPackage::defaultMaximumBytes : m_payloadLimit)
        || !validBook(bookId) || (!base.isEmpty() && !hexString(base, 64))) {
        sendReply(previewReplyType, request, problem(tr("La richiesta di sincronizzazione non è valida.")));
        return;
    }
    const QString acknowledged = m_baselines.value(bookId).toString();
    const QByteArray agreed = base.isEmpty() || base != acknowledged ? QByteArray{} : QByteArray::fromHex(base.toLatin1());
    QVariantMap result = m_notebook->receiveBookTransfer(payload, {}, agreed);
    if (!result.contains(QStringLiteral("error"))) {
        if (result.value(QStringLiteral("bookId")).toString() != bookId) {
            sendReply(previewReplyType, request, problem(tr("La richiesta di sincronizzazione non è valida.")));
            return;
        }
        resetIncoming();
        m_incomingRequest = request;
        m_incomingToken = result.value(QStringLiteral("token")).toString();
        m_incomingAction = result.value(QStringLiteral("action")).toString();
        m_incomingBook = bookId;
        m_incomingPayload = payload;
        m_incomingBase = agreed;
        m_incomingTimer.start(reviewTimeout);
        scheduleRoutingState();
        if (m_incomingAction == QStringLiteral("conflict"))
            setBookIssue(bookId, m_notebook->bookTransferRevision(bookId), QStringLiteral("conflict"), {});
    }
    sendReply(previewReplyType, request, std::move(result));
}

void BleSync::receiveApply(const QString &request, const QVariantMap &message)
{
    const QString token = message.value(QStringLiteral("token")).toString();
    const QVariantMap receipt = m_applyReceipts.value(request).toMap();
    if (hexString(token, 64) && receipt.value(QStringLiteral("token")).toString() == token) {
        sendReply(applyReplyType, request, receipt.value(QStringLiteral("result")).toMap());
        return;
    }
    if (request != m_incomingRequest || !hexString(token, 64) || token != m_incomingToken) {
        sendReply(applyReplyType, request, problem(tr("L’anteprima è scaduta. Controlla di nuovo prima di inviare.")));
        return;
    }
    if (m_editorBusy) {
        resetIncoming();
        sendReply(applyReplyType, request,
                  {{QStringLiteral("error"), tr("L’altro dispositivo sta modificando il quaderno. Riprova tra poco.")},
                   {QStringLiteral("errorCode"), QStringLiteral("stale")}});
        return;
    }
    const QVariantMap result = m_notebook->receiveBookTransfer(m_incomingPayload, token.toLatin1(), m_incomingBase);
    const QString action = m_incomingAction;
    const QString bookId = m_incomingBook;
    resetIncoming();
    if (!result.contains(QStringLiteral("error"))) {
        while (m_applyReceipts.size() >= 64)
            m_applyReceipts.erase(m_applyReceipts.begin());
        m_applyReceipts.insert(request, QVariantMap{{QStringLiteral("token"), token}, {QStringLiteral("result"), result}});
        savePreferences();
        if (action == QStringLiteral("conflict"))
            setBookIssue(bookId, m_notebook->bookTransferRevision(bookId), QStringLiteral("conflict"), {});
        else
            acknowledge(bookId, result.value(QStringLiteral("revision")).toString());
    }
    sendReply(applyReplyType, request, result);
}

void BleSync::receiveReply(const QString &type, const QVariantMap &message)
{
    const bool isPreview = type == previewReplyType;
    if ((isPreview && m_stage != Stage::Preview) || (!isPreview && !applying()))
        return;
    if (isPreview)
        m_delta.cancelOutgoing(m_request);
    const QString error = displayText(message.value(QStringLiteral("error")));
    if (!error.isEmpty()) {
        if (!isPreview && message.value(QStringLiteral("errorCode")).toString() == QStringLiteral("stale")) {
            m_pendingTransferApplying = false; // Explicitly rejected before commit.
            queueAutomaticRetry();
        }
        finishOutgoing(error);
        return;
    }
    const QString action = message.value(QStringLiteral("action")).toString();
    const QString token = message.value(QStringLiteral("token")).toString();
    if (!validAction(action) || !hexString(token, 64) || message.value(QStringLiteral("bookId")).toString() != m_bookId
        || message.value(QStringLiteral("revision")).toString() != m_revision
        || (!isPreview && (action != m_preview.value(QStringLiteral("action")).toString() || token != m_token))) {
        finishOutgoing(tr("La risposta dell’altro dispositivo non è valida."));
        return;
    }
    if (!isPreview) {
        const bool conflict = action == QStringLiteral("conflict");
        if (!conflict)
            acknowledge(m_bookId, m_revision);
        finishOutgoing(conflict ? tr("Quaderno inviato come copia separata.")
                      : action == QStringLiteral("unchanged") ? tr("Il quaderno è già aggiornato sull’altro dispositivo.")
                      : tr("Quaderno sincronizzato."), true, conflict);
        return;
    }

    const int sourcePages = message.value(QStringLiteral("sourcePages"), -1).toInt();
    const int targetPages = message.value(QStringLiteral("targetPages"), -1).toInt();
    const int changedPages = message.value(QStringLiteral("changedPages"), -1).toInt();
    if (sourcePages < 1 || targetPages < 0 || changedPages < 0 || changedPages > qMax(sourcePages, targetPages)) {
        finishOutgoing(tr("L’anteprima del trasferimento non è valida."));
        return;
    }
    m_preview = {{QStringLiteral("action"), action}, {QStringLiteral("bookId"), m_bookId},
                 {QStringLiteral("sourceTitle"), displayText(message.value(QStringLiteral("sourceTitle")))},
                 {QStringLiteral("targetTitle"), displayText(message.value(QStringLiteral("targetTitle")))},
                 {QStringLiteral("sourcePages"), sourcePages}, {QStringLiteral("targetPages"), targetPages},
                 {QStringLiteral("changedPages"), changedPages}, {QStringLiteral("automatic"), m_automatic}};
    m_token = token;
    m_stage = Stage::Review;
    m_status.clear();
    m_outgoingTimer.start(reviewTimeout);
    if (action == QStringLiteral("conflict"))
        setBookIssue(m_bookId, m_revision, QStringLiteral("conflict"), {});
    updateBookStatus();
    if (m_automatic && m_autoSync && m_automaticAllowed && !m_editorBusy && m_pins.contains(m_bookId)
        && (action == QStringLiteral("replace") || action == QStringLiteral("unchanged")
            || (action == QStringLiteral("conflict") && m_conflictPolicy == QStringLiteral("keepBoth")))) {
        apply();
    } else {
        emit changed();
    }
}

void BleSync::sendReply(const QString &type, const QString &request, QVariantMap result)
{
    result.insert(QStringLiteral("type"), type);
    result.insert(QStringLiteral("request"), request);
    // Keep metadata bounded by the message envelope even for legacy, unusually long titles.
    for (const auto &key : {QStringLiteral("sourceTitle"), QStringLiteral("targetTitle"), QStringLiteral("error")}) {
        if (result.contains(key))
            result[key] = displayText(result.value(key));
    }
    emit sendMessage(result, {});
}

void BleSync::resetOutgoing()
{
    m_delta.cancelOutgoing(m_request);
    m_outgoingTimer.stop();
    m_stage = Stage::Idle;
    m_automatic = false;
    m_request.clear();
    m_bookId.clear();
    m_revision.clear();
    m_token.clear();
    m_payload.clear();
    m_preview.clear();
}

void BleSync::resetIncoming()
{
    const bool hadPreview = !m_incomingRequest.isEmpty();
    m_incomingTimer.stop();
    m_incomingRequest.clear();
    m_incomingToken.clear();
    m_incomingAction.clear();
    m_incomingBook.clear();
    m_incomingPayload.clear();
    m_incomingBase.clear();
    if (hadPreview) scheduleRoutingState();
}

void BleSync::finishOutgoing(const QString &status, bool completed, bool conflict)
{
    const QString bookId = m_bookId;
    const QString revision = m_revision;
    const bool followWithResume = !m_outgoingResume.bookId.isEmpty() && m_outgoingResume.request.isEmpty()
        && m_outgoingResume.bookId == bookId;
    if (m_automatic && m_triggeredAutomaticSync && !completed && m_autoAttempts.value(bookId).retry
        && m_pins.contains(bookId) && !m_automaticQueue.contains(bookId)) {
        m_automaticQueue.prepend(bookId);
        m_batchRevisions.insert(bookId, revision);
    }
    // A lost apply acknowledgement may already have created a conflict copy.
    // Require a manual review instead of silently issuing a second copy request.
    if (applying() && m_pendingTransferApplying && !completed
        && validBook(bookId) && !m_uncertainAutomaticBooks.contains(bookId)
        && m_uncertainAutomaticBooks.size() < maxBaselines) {
        m_uncertainAutomaticBooks.append(bookId);
        savePreferences();
    }
    if (completed) m_uncertainAutomaticBooks.removeAll(bookId);
    if (completed && conflict && validBook(bookId) && hexString(revision, 64)
        && (m_conflictCopies.contains(bookId) || m_conflictCopies.size() < maxBaselines))
        m_conflictCopies.insert(bookId, revision);
    resetOutgoing();
    m_status = status;
    m_completed = completed;
    if (completed) {
        m_pendingTransferBook.clear();
        m_pendingTransferRevision.clear();
        m_pendingTransferAutomatic = false;
        m_pendingTransferApplying = false;
    }
    savePreferences();
    if (!status.isEmpty() && (conflict || !completed))
        setBookIssue(bookId, revision, conflict ? QStringLiteral("conflict") : QStringLiteral("error"), status);
    updateBookStatus();
    scheduleRoutingState();
    emit changed();
    if (followWithResume && resumeBusy()) {
        if (completed && !conflict)
            dispatchResume();
        else if (conflict)
            finishResume(tr("La copia in conflitto non consente di riprendere il quaderno originale."));
        else
            finishResume(status);
    }
    scheduleAutoSync();
}

void BleSync::scheduleRoutingState(int delay)
{
    if (!m_routingStateTimer.isActive() || m_routingStateTimer.remainingTime() > delay)
        m_routingStateTimer.start(delay);
}

void BleSync::dispatchDeferredTransfer()
{
    if (!m_connected || m_transportSelectionPaused || !m_notebook || m_stage != Stage::Idle)
        return;
    if (!m_deferredResume.bookId.isEmpty()) {
        const ResumeRequest resume = std::exchange(m_deferredResume, {});
        if (m_notebook->canResumeBook(resume.bookId, resume.revision, resume.pageId))
            sendResume(resume.bookId, resume.pageId);
        else
            finishResume(tr("La pagina è cambiata. Riaprila e riprova."));
    } else if (!m_deferredPreviewBook.isEmpty()) {
        preview(std::exchange(m_deferredPreviewBook, {}));
    }
    scheduleRoutingState();
    scheduleAutoSync();
}

void BleSync::schedulePendingRetry()
{
    if (m_retryPendingOnConnection && m_connected && !m_transportSelectionPaused && !m_editorBusy)
        m_pendingTransferTimer.start(pendingRetryDelay);
}

void BleSync::scheduleAutoSync()
{
    if (m_connected && !m_transportSelectionPaused && !hasDeferredTransfer() && !pendingRetryHasPriority()
        && m_autoSync && m_automaticAllowed && !m_editorBusy && m_notebook && m_stage == Stage::Idle
        && !m_pins.isEmpty() && (!m_triggeredAutomaticSync || !m_automaticQueue.isEmpty())) {
        int delay = automaticDelay;
        bool onlyRetries = true;
        qint64 earliestRetry = std::numeric_limits<qint64>::max();
        const auto candidates = m_triggeredAutomaticSync ? m_automaticQueue : m_pins;
        for (const QString &book : candidates) {
            const auto attempt = m_autoAttempts.constFind(book);
            if (attempt == m_autoAttempts.cend() || !attempt->retry) onlyRetries = false;
            else earliestRetry = qMin(earliestRetry, attempt->retryAt);
        }
        if (onlyRetries && earliestRetry != std::numeric_limits<qint64>::max())
            delay = int(qBound(qint64{0}, earliestRetry - m_retryClock.elapsed(), qint64{8000}));
        m_autoTimer.start(delay);
    } else
        m_autoTimer.stop();
}

void BleSync::setEditorBusy(bool busy)
{
    if (m_editorBusy == busy)
        return;
    m_editorBusy = busy;
    if (busy) {
        m_autoTimer.stop();
        m_pendingTransferTimer.stop();
        pauseAutomaticPreview();
    } else {
        schedulePendingRetry();
        scheduleAutoSync();
    }
    if (!m_resumeOffer.isEmpty())
        emit resumeChanged();
    scheduleRoutingState();
    emit changed();
}

void BleSync::runAutoSync()
{
    if (!m_connected || m_transportSelectionPaused || hasDeferredTransfer() || pendingRetryHasPriority()
        || !m_autoSync || !m_automaticAllowed || m_editorBusy || !m_notebook || m_stage != Stage::Idle)
        return;
    const QStringList candidates = m_triggeredAutomaticSync ? m_automaticQueue : m_pins;
    for (const QString &bookId : candidates) {
        const QString revision = automaticRevision(bookId);
        if (!m_pins.contains(bookId) || revision.isEmpty()
            || (m_triggeredAutomaticSync && revision != m_batchRevisions.value(bookId))) {
            m_automaticQueue.removeAll(bookId);
            m_batchRevisions.remove(bookId);
            m_autoAttempts.remove(bookId);
            continue;
        }
        AutoAttempt &attempt = m_autoAttempts[bookId];
        if (attempt.revision != revision)
            attempt = {revision};
        else if (!attempt.retry) {
            m_automaticQueue.removeAll(bookId);
            m_batchRevisions.remove(bookId);
            continue;
        } else if (attempt.retryAt > m_retryClock.elapsed()) {
            continue;
        }
        m_automaticQueue.removeAll(bookId);
        attempt.retry = false;
        beginPreview(bookId, revision, true);
        return;
    }
    savePreferences();
    updateAutomaticState();
    if (automaticRetryPending()) scheduleAutoSync();
}

QString BleSync::automaticRevision(const QString &bookId) const
{
    if (m_uncertainAutomaticBooks.contains(bookId))
        return {};
    const QString revision = m_notebook->bookTransferRevision(bookId);
    if (revision == m_baselines.value(bookId).toString() || revision == m_conflictCopies.value(bookId).toString())
        return {};
    return revision;
}

bool BleSync::hasSyncWork() const
{
    if (!m_notebook || m_peer.isEmpty())
        return false;
    if (hasDeferredTransfer())
        return true;
    if (m_editorBusy || m_stage != Stage::Idle)
        return false;
    if ((!m_connected || m_retryPendingOnConnection) && !m_pendingTransferAutomatic
        && hasPendingTransfer() && m_notebook->hasBookId(m_pendingTransferBook))
        return true;
    if (!m_autoSync || !m_automaticAllowed)
        return false;
    const auto &candidates = m_triggeredAutomaticSync ? m_automaticQueue : m_pins;
    for (const QString &bookId : candidates) {
        const QString revision = automaticRevision(bookId);
        if (revision.isEmpty() || (m_triggeredAutomaticSync && revision != m_batchRevisions.value(bookId)))
            continue;
        const auto attempt = m_autoAttempts.constFind(bookId);
        if (attempt == m_autoAttempts.cend() || attempt->revision != revision || attempt->retry)
            return true;
    }
    return false;
}

bool BleSync::queueAutomaticRetry()
{
    if (!m_automatic || !m_pins.contains(m_bookId)) return false;
    auto attempt = m_autoAttempts.find(m_bookId);
    if (attempt == m_autoAttempts.end() || attempt->retries >= maxAutomaticRetries) return false;
    attempt->retryAt = m_retryClock.elapsed() + (2000 << attempt->retries);
    ++attempt->retries;
    attempt->retry = true;
    return true;
}

int BleSync::pendingAutomaticCount() const
{
    return m_automaticQueue.size() + (m_automatic && !m_automaticQueue.contains(m_bookId) ? 1 : 0);
}

bool BleSync::automaticRetryPending() const
{
    if (!m_autoSync) return false;
    for (auto it = m_autoAttempts.cbegin(); it != m_autoAttempts.cend(); ++it)
        if (it->retry && m_pins.contains(it.key())
            && (!m_triggeredAutomaticSync || m_automaticQueue.contains(it.key()))) return true;
    return false;
}

void BleSync::updateAutomaticState()
{
    const int count = pendingAutomaticCount();
    const bool retry = automaticRetryPending();
    if (count == m_notifiedAutomaticCount && retry == m_notifiedAutomaticRetry) return;
    m_notifiedAutomaticCount = count;
    m_notifiedAutomaticRetry = retry;
    scheduleRoutingState();
    emit automaticStateChanged();
}

bool BleSync::savePreferences() const
{
    if (m_peer.isEmpty())
        return false;
    auto settings = syncSettings();
    settings.beginGroup(QStringLiteral("nearbySync/") + m_peer);
    settings.setValue(QStringLiteral("automatic"), m_autoSync);
    settings.setValue(QStringLiteral("pinnedBooks"), m_pins);
    settings.setValue(QStringLiteral("baselines"), m_baselines);
    settings.setValue(QStringLiteral("lastSuccess"), m_lastSuccess);
    settings.setValue(QStringLiteral("pendingTransferBook"), m_pendingTransferBook);
    settings.setValue(QStringLiteral("pendingTransferRevision"), m_pendingTransferRevision);
    settings.setValue(QStringLiteral("pendingTransferAutomatic"), m_pendingTransferAutomatic);
    settings.setValue(QStringLiteral("pendingTransferApplying"), m_pendingTransferApplying);
    settings.setValue(QStringLiteral("automaticQueue"), m_automaticQueue);
    QVariantMap batch;
    for (auto it = m_batchRevisions.cbegin(); it != m_batchRevisions.cend(); ++it) batch.insert(it.key(), it.value());
    settings.setValue(QStringLiteral("automaticBatchRevisions"), batch);
    settings.setValue(QStringLiteral("applyReceipts"), m_applyReceipts);
    settings.setValue(QStringLiteral("conflictCopies"), m_conflictCopies);
    settings.setValue(QStringLiteral("uncertainAutomaticBooks"), m_uncertainAutomaticBooks);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void BleSync::acknowledge(const QString &bookId, const QString &revision)
{
    if (!validBook(bookId) || !hexString(revision, 64))
        return;
    if (!m_baselines.contains(bookId) && m_baselines.size() >= maxBaselines)
        return;
    m_baselines.insert(bookId, revision);
    m_lastSuccess.insert(bookId, QDateTime::currentDateTimeUtc());
    m_bookIssues.remove(bookId);
    m_autoAttempts.remove(bookId);
    savePreferences();
    updateBookStatus();
    scheduleRoutingState();
}

QVariantMap BleSync::bookStatus(const QString &bookId) const
{
    const auto stateKey = QStringLiteral("state");
    QVariantMap result{{stateKey, QStringLiteral("neutral")}};
    if (m_peer.isEmpty() || !m_notebook || !validBook(bookId))
        return result;

    const QString baseline = m_baselines.value(bookId).toString();
    const auto issue = m_bookIssues.constFind(bookId);
    const auto timestamp = m_lastSuccess.constFind(bookId);
    if (timestamp != m_lastSuccess.constEnd())
        result.insert(QStringLiteral("lastSuccess"), timestamp.value());

    if (m_bookId == bookId && busy()) {
        result[stateKey] = QStringLiteral("sending");
    } else if (!baseline.isEmpty() || issue != m_bookIssues.constEnd()) {
        const QString revision = m_notebook->bookTransferRevision(bookId);
        if (revision.isEmpty())
            return result;
        if (issue != m_bookIssues.constEnd() && issue->revision == revision) {
            result[stateKey] = issue->state;
            if (!issue->detail.isEmpty())
                result.insert(QStringLiteral("detail"), issue->detail);
        } else if (!baseline.isEmpty()) {
            result[stateKey] = revision == baseline ? QStringLiteral("upToDate") : QStringLiteral("localChanges");
        }
    }
    return result;
}

void BleSync::noteBookChanges()
{
    scheduleAutoSync();
    // Strokes can arrive in bursts. Avoid repeatedly hashing pinned books
    // while pen input keeps arriving, but notify before the automatic send.
    if (m_routingStateTimer.remainingTime() != 0)
        m_routingStateTimer.start(editWorkDelay);
    if (!m_bookStatusTimer.isActive())
        m_bookStatusTimer.start(250);
}

void BleSync::updateBookStatus()
{
    m_bookStatusTimer.stop();
    updateAutomaticState();
    ++m_bookStatusVersion;
    emit bookStatusChanged();
}

void BleSync::setBookIssue(const QString &bookId, const QString &revision, const QString &state, const QString &detail)
{
    if (!validBook(bookId) || !hexString(revision, 64))
        return;
    const auto existing = m_bookIssues.constFind(bookId);
    if (existing != m_bookIssues.constEnd() && existing->revision == revision && existing->state == QStringLiteral("conflict")
        && state == QStringLiteral("error"))
        return;
    if (!m_bookIssues.contains(bookId) && m_bookIssues.size() >= maxBaselines)
        m_bookIssues.erase(m_bookIssues.begin());
    m_bookIssues.insert(bookId, {state, revision, detail});
    updateBookStatus();
}

void BleSync::sendResume(const QString &bookId, const QString &pageId)
{
    if ((!m_connected && !m_transportSelectionPaused) || m_peer.isEmpty() || !m_notebook || resumeBusy())
        return;
    const QString revision = m_notebook->bookTransferRevision(bookId);
    const int page = m_notebook->bookPageNumber(bookId, pageId);
    if (!validBook(bookId) || !validBook(pageId) || !hexString(revision, 64) || page < 1
        || !m_notebook->canResumeBook(bookId, revision, pageId)) {
        finishResume(tr("La pagina da riprendere non è disponibile."));
        return;
    }
    if (m_stage != Stage::Idle && (m_bookId != bookId || m_revision != revision)) {
        finishResume(tr("Attendi la conclusione del trasferimento in corso."));
        return;
    }
    m_deferredPreviewBook.clear();
    if (m_transportSelectionPaused) {
        m_deferredResume = {bookId, revision, pageId, {}, page};
        m_resumeStatus = tr("Scelta del collegamento per la sincronizzazione…");
        scheduleRoutingState();
        emit resumeChanged();
        return;
    }
    m_outgoingResume = {bookId, revision, pageId, {}};
    scheduleRoutingState();
    if (m_stage == Stage::Idle && m_baselines.value(bookId).toString() == revision) {
        dispatchResume();
        return;
    }
    m_resumeStatus = tr("Conferma il trasferimento del quaderno per inviare la pagina.");
    emit resumeChanged();
    if (resumeBusy() && m_stage == Stage::Idle)
        beginPreview(bookId, revision, false);
}

void BleSync::dispatchResume()
{
    const ResumeRequest resume = m_outgoingResume;
    const int page = m_notebook ? m_notebook->bookPageNumber(resume.bookId, resume.pageId) : 0;
    if (!m_connected || page < 1 || !m_notebook->canResumeBook(resume.bookId, resume.revision, resume.pageId)) {
        finishResume(tr("La pagina è cambiata. Riaprila e riprova."));
        return;
    }
    const QString request = QUuid::createUuid().toString(QUuid::Id128);
    m_outgoingResume.request = request;
    m_resumeStatus = tr("In attesa che la pagina venga aperta sull’altro dispositivo…");
    m_outgoingResumeTimer.start(resumeTimeout);
    emit resumeChanged();
    if (m_outgoingResume.request == request) {
        emit sendMessage({{QStringLiteral("type"), resumeType}, {QStringLiteral("request"), request},
                          {QStringLiteral("bookId"), resume.bookId}, {QStringLiteral("revision"), resume.revision},
                          {QStringLiteral("pageId"), resume.pageId}, {QStringLiteral("pageNumber"), page}}, {});
    }
}

void BleSync::receiveResume(const QString &request, const QVariantMap &message)
{
    const auto reject = [&](const QString &status) {
        sendReply(resumeReplyType, request, {{QStringLiteral("result"), status}});
    };
    if (!m_incomingResume.request.isEmpty()) {
        if (m_incomingResume.request != request)
            reject(QStringLiteral("busy"));
        return;
    }
    const QString bookId = message.value(QStringLiteral("bookId")).toString();
    const QString revision = message.value(QStringLiteral("revision")).toString();
    const QString pageId = message.value(QStringLiteral("pageId")).toString();
    const int pageNumber = QJsonValue::fromVariant(message.value(QStringLiteral("pageNumber"))).toInt();
    if (!validBook(bookId) || !validBook(pageId) || !hexString(revision, 64)
        || pageNumber < 1 || !m_notebook->canResumeBook(bookId, revision, pageId, pageNumber)) {
        reject(QStringLiteral("different"));
        return;
    }
    const QString title = displayText(m_notebook->bookDisplayTitle(bookId));
    m_incomingResume = {bookId, revision, pageId, request, pageNumber};
    m_resumeOffer = {{QStringLiteral("bookId"), bookId}, {QStringLiteral("title"), title},
                     {QStringLiteral("pageId"), pageId}, {QStringLiteral("page"), pageNumber}};
    m_incomingResumeTimer.start(resumeTimeout);
    scheduleRoutingState();
    emit resumeChanged();
}

bool BleSync::acceptResume()
{
    if (!m_connected || !m_notebook || !canAcceptResume())
        return false;
    const ResumeRequest resume = m_incomingResume;
    if (!m_notebook->canResumeBook(resume.bookId, resume.revision, resume.pageId, resume.page)) {
        m_resumeStatus = tr("La pagina non corrisponde più alla versione ricevuta.");
        replyToResume(QStringLiteral("different"));
        return false;
    }
    if (!m_notebook->openBookAtRevision(resume.bookId, resume.revision, resume.pageId, resume.page)) {
        m_resumeStatus = tr("Impossibile aprire la pagina ricevuta.");
        replyToResume(QStringLiteral("failed"));
        return false;
    }
    if (!resumeBusy())
        m_resumeStatus.clear();
    replyToResume(QStringLiteral("opened"));
    return true;
}

void BleSync::dismissResume()
{
    if (!m_incomingResume.request.isEmpty())
        replyToResume(QStringLiteral("dismissed"));
}

void BleSync::cancelResume()
{
    if (!resumeBusy())
        return;
    const bool cancellingPreview = m_outgoingResume.request.isEmpty() && m_outgoingResume.bookId == m_bookId && !applying();
    finishResume({}, true);
    if (cancellingPreview)
        cancel();
}

void BleSync::receiveResumeReply(const QString &request, const QVariantMap &message)
{
    if (request != m_outgoingResume.request)
        return;
    const QString result = message.value(QStringLiteral("result")).toString();
    if (result == QStringLiteral("opened"))
        finishResume(tr("Pagina aperta sull’altro dispositivo."));
    else if (result == QStringLiteral("different"))
        finishResume(tr("La pagina sull’altro dispositivo è diversa. Sincronizza il quaderno prima di riprovare."));
    else if (result == QStringLiteral("busy"))
        finishResume(tr("L’altro dispositivo ha già una richiesta di ripresa in attesa."));
    else if (result == QStringLiteral("dismissed"))
        finishResume(tr("L’altro dispositivo ha ignorato la richiesta di ripresa."));
    else if (result == QStringLiteral("expired"))
        finishResume(tr("La richiesta di ripresa è scaduta. Controlla l’altro dispositivo."));
    else
        finishResume(tr("L’altro dispositivo non ha potuto aprire la pagina."));
}

void BleSync::finishResume(const QString &status, bool notifyPeer)
{
    const QString request = m_outgoingResume.request;
    m_outgoingResumeTimer.stop();
    m_outgoingResume = {};
    m_deferredResume = {};
    m_resumeStatus = status;
    scheduleRoutingState();
    emit resumeChanged();
    if (notifyPeer && m_connected && !request.isEmpty())
        emit sendMessage({{QStringLiteral("type"), resumeCancelType}, {QStringLiteral("request"), request}}, {});
    scheduleAutoSync();
}

void BleSync::replyToResume(const QString &status)
{
    const QString request = m_incomingResume.request;
    clearResumeOffer();
    if (m_connected && !request.isEmpty())
        sendReply(resumeReplyType, request, {{QStringLiteral("result"), status}});
}

void BleSync::clearResumeOffer()
{
    m_incomingResumeTimer.stop();
    m_incomingResume = {};
    if (m_resumeOffer.isEmpty())
        return;
    m_resumeOffer.clear();
    scheduleRoutingState();
    emit resumeChanged();
}
