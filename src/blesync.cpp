#include "blesync.h"

#include "notebook.h"

#include <QDateTime>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>

#include <utility>

namespace {
constexpr int maxPins = 20;
constexpr int maxBaselines = 256;
constexpr int transferTimeout = 15 * 60 * 1000;
constexpr int reviewTimeout = 5 * 60 * 1000;
constexpr int resumeTimeout = 2 * 60 * 1000;
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

QSettings syncSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
}

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

BleSync::BleSync(Notebook *notebook, QObject *parent) : QObject(parent), m_notebook(notebook)
{
    m_outgoingTimer.setSingleShot(true);
    m_incomingTimer.setSingleShot(true);
    m_autoTimer.setSingleShot(true);
    m_bookStatusTimer.setSingleShot(true);
    m_outgoingResumeTimer.setSingleShot(true);
    m_incomingResumeTimer.setSingleShot(true);
    connect(&m_outgoingTimer, &QTimer::timeout, this, [this] {
        const QString request = m_request;
        const bool wasApplying = applying();
        finishOutgoing(wasApplying ? tr("Il trasferimento è scaduto. Controlla l’altro dispositivo prima di riprovare.")
                                   : tr("L’anteprima del trasferimento è scaduta."));
        if (m_connected && !wasApplying)
            emit sendMessage({{QStringLiteral("type"), cancelType}, {QStringLiteral("request"), request}}, {});
    });
    connect(&m_incomingTimer, &QTimer::timeout, this, &BleSync::resetIncoming);
    connect(&m_autoTimer, &QTimer::timeout, this, &BleSync::runAutoSync);
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
    setConnected(false);
    m_peer = id;
    m_pins.clear();
    m_baselines.clear();
    m_lastSuccess.clear();
    m_bookIssues.clear();
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
    emit resumeChanged();
    emit changed();
}

void BleSync::setPayloadLimit(qsizetype bytes)
{
    if (!m_connected)
        m_payloadLimit = qBound(qsizetype{1}, bytes, qsizetype{64 * 1024 * 1024});
}

void BleSync::forgetPeer()
{
    if (m_peer.isEmpty())
        return;
    auto settings = syncSettings();
    settings.remove(QStringLiteral("nearbySync/") + m_peer);
    setPeerIdentity({});
}

void BleSync::setConnected(bool connected)
{
    if (m_connected == connected || (connected && m_peer.isEmpty()))
        return;
    const bool wasApplying = applying();
    m_connected = connected;
    m_autoAttempts.clear();
    resetIncoming();
    m_autoTimer.stop();
    if (!connected && resumeBusy())
        finishResume(tr("Connessione interrotta prima della conferma di ripresa."));
    clearResumeOffer();
    finishOutgoing(!connected && wasApplying
        ? tr("Connessione interrotta durante l’invio. Controlla l’altro dispositivo prima di riprovare.") : QString{});
}

void BleSync::preview(const QString &bookId)
{
    if (!m_connected || !m_notebook || busy())
        return;
    if (m_stage == Stage::Review)
        cancel();
    beginPreview(bookId, m_notebook->bookTransferRevision(bookId), false);
}

void BleSync::beginPreview(const QString &bookId, const QString &revision, bool automatic)
{
    m_completed = false;
    m_bookId = bookId;
    m_revision = revision;
    if (!validBook(bookId) || !hexString(revision, 64)) {
        finishOutgoing(tr("Scegli un quaderno disponibile da sincronizzare."));
        return;
    }
    m_payload = m_notebook->exportBookForTransfer(bookId);
    if (m_payload.isEmpty() || m_payload.size() > m_payloadLimit) {
        const QString error = m_notebook->error();
        if (!error.isEmpty())
            finishOutgoing(error);
        else if (m_payloadLimit == bluetoothPayloadLimit)
            finishOutgoing(tr("Il quaderno supera il limite Bluetooth di %1 MiB. Passa al Wi-Fi locale per inviarlo.")
                           .arg(bluetoothPayloadLimit / mebibyte));
        else
            finishOutgoing(tr("Il quaderno supera il limite di %1 MiB per questa connessione.")
                           .arg(m_payloadLimit / mebibyte));
        return;
    }
    m_stage = Stage::Preview;
    m_request = QUuid::createUuid().toString(QUuid::Id128);
    m_automatic = automatic;
    m_status = tr("Controllo della versione sull’altro dispositivo…");
    m_outgoingTimer.start(transferTimeout);
    updateBookStatus();
    emit changed();
    if (m_stage == Stage::Preview) {
        const QByteArray payload = m_payload;
        emit sendMessage({{QStringLiteral("type"), previewType}, {QStringLiteral("request"), m_request},
                          {QStringLiteral("bookId"), bookId}, {QStringLiteral("base"), m_baselines.value(bookId).toString()}}, payload);
    }
}

void BleSync::apply()
{
    if (!m_connected || m_stage != Stage::Review || !m_notebook)
        return;
    if (m_notebook->exportBookForTransfer(m_bookId) != m_payload) {
        const QString request = m_request;
        finishOutgoing(tr("Il quaderno è cambiato. Controlla una nuova anteprima."));
        emit sendMessage({{QStringLiteral("type"), cancelType}, {QStringLiteral("request"), request}}, {});
        return;
    }
    m_stage = Stage::Apply;
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
    }
    savePreferences();
    emit changed();
    scheduleAutoSync();
}

void BleSync::setAutoSync(bool enabled)
{
    if (m_peer.isEmpty() || m_autoSync == enabled)
        return;
    m_autoSync = enabled;
    savePreferences();
    if (!enabled) {
        m_autoTimer.stop();
        if (m_automatic && !applying())
            cancel();
    }
    emit changed();
    scheduleAutoSync();
}

void BleSync::receiveMessage(const QVariantMap &message, const QByteArray &payload)
{
    if (!m_connected || !m_notebook)
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
        else if (type == cancelType && request == m_incomingRequest)
            resetIncoming();
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

void BleSync::receivePreview(const QString &request, const QVariantMap &message, const QByteArray &payload)
{
    const QString bookId = message.value(QStringLiteral("bookId")).toString();
    const QString base = message.value(QStringLiteral("base")).toString();
    if (payload.isEmpty() || payload.size() > m_payloadLimit || !validBook(bookId) || (!base.isEmpty() && !hexString(base, 64))) {
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
        if (m_incomingAction == QStringLiteral("conflict"))
            setBookIssue(bookId, m_notebook->bookTransferRevision(bookId), QStringLiteral("conflict"), {});
    }
    sendReply(previewReplyType, request, std::move(result));
}

void BleSync::receiveApply(const QString &request, const QVariantMap &message)
{
    const QString token = message.value(QStringLiteral("token")).toString();
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
    const QString error = displayText(message.value(QStringLiteral("error")));
    if (!error.isEmpty()) {
        if (m_automatic && !isPreview && message.value(QStringLiteral("errorCode")).toString() == QStringLiteral("stale")) {
            auto attempt = m_autoAttempts.find(m_bookId);
            if (attempt != m_autoAttempts.end() && attempt->retries < 2) {
                ++attempt->retries;
                attempt->retry = true;
            }
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
    if (m_automatic && m_autoSync && m_pins.contains(m_bookId)
        && (action == QStringLiteral("replace") || action == QStringLiteral("unchanged"))) {
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
    m_incomingTimer.stop();
    m_incomingRequest.clear();
    m_incomingToken.clear();
    m_incomingAction.clear();
    m_incomingBook.clear();
    m_incomingPayload.clear();
    m_incomingBase.clear();
}

void BleSync::finishOutgoing(const QString &status, bool completed, bool conflict)
{
    const QString bookId = m_bookId;
    const QString revision = m_revision;
    const bool followWithResume = resumeBusy() && m_outgoingResume.request.isEmpty() && m_outgoingResume.bookId == bookId;
    resetOutgoing();
    m_status = status;
    m_completed = completed;
    if (!status.isEmpty() && (conflict || !completed))
        setBookIssue(bookId, revision, conflict ? QStringLiteral("conflict") : QStringLiteral("error"), status);
    updateBookStatus();
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

void BleSync::scheduleAutoSync()
{
    if (m_connected && m_autoSync && !m_editorBusy && m_notebook && m_stage == Stage::Idle && !m_pins.isEmpty())
        m_autoTimer.start(1700);
}

void BleSync::setEditorBusy(bool busy)
{
    if (m_editorBusy == busy)
        return;
    m_editorBusy = busy;
    if (busy)
        m_autoTimer.stop();
    else
        scheduleAutoSync();
    if (!m_resumeOffer.isEmpty())
        emit resumeChanged();
}

void BleSync::runAutoSync()
{
    if (!m_connected || !m_autoSync || m_editorBusy || !m_notebook || m_stage != Stage::Idle)
        return;
    for (const QString &bookId : m_pins) {
        const QString revision = m_notebook->bookTransferRevision(bookId);
        if (revision.isEmpty() || revision == m_baselines.value(bookId).toString())
            continue;
        AutoAttempt &attempt = m_autoAttempts[bookId];
        if (attempt.revision != revision)
            attempt = {revision};
        else if (!attempt.retry)
            continue;
        attempt.retry = false;
        beginPreview(bookId, revision, true);
        return;
    }
}

void BleSync::savePreferences() const
{
    if (m_peer.isEmpty())
        return;
    auto settings = syncSettings();
    settings.beginGroup(QStringLiteral("nearbySync/") + m_peer);
    settings.setValue(QStringLiteral("automatic"), m_autoSync);
    settings.setValue(QStringLiteral("pinnedBooks"), m_pins);
    settings.setValue(QStringLiteral("baselines"), m_baselines);
    settings.setValue(QStringLiteral("lastSuccess"), m_lastSuccess);
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
    if (!m_bookStatusTimer.isActive())
        m_bookStatusTimer.start(250);
}

void BleSync::updateBookStatus()
{
    m_bookStatusTimer.stop();
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
    if (!m_connected || !m_notebook || resumeBusy())
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
    m_outgoingResume = {bookId, revision, pageId, {}};
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
    m_resumeStatus = status;
    emit resumeChanged();
    if (notifyPeer && m_connected && !request.isEmpty())
        emit sendMessage({{QStringLiteral("type"), resumeCancelType}, {QStringLiteral("request"), request}}, {});
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
    emit resumeChanged();
}
