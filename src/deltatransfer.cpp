#include "deltatransfer.h"
#include "notebookformat.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {
constexpr qsizetype chunkBytes = 64 * 1024;
constexpr qint64 cacheBytes = 256 * 1024 * 1024;
constexpr int chunkTimeout = 30 * 1000;

bool hex(const QString &value, qsizetype size)
{
    const QByteArray bytes = value.toLatin1();
    return bytes.size() == size && QByteArray::fromHex(bytes).toHex() == bytes;
}

QString dataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/sync-objects");
}

bool durableFlush(QFile &file)
{
    if (!file.flush())
        return false;
#ifdef Q_OS_UNIX
    return ::fsync(file.handle()) == 0;
#else
    return true;
#endif
}

int formatVersion(const QVariant &value)
{
    const auto type = value.metaType().id();
    if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong
        && type != QMetaType::ULongLong && type != QMetaType::Float && type != QMetaType::Double)
        return 0;
    const double number = value.toDouble();
    return std::isfinite(number) && number >= 1 && number <= std::numeric_limits<int>::max()
        && number == std::floor(number) ? int(number) : 0;
}

bool hasUmlNodes(const QJsonObject &diagram)
{
    const auto nodes = diagram.value(QStringLiteral("nodes")).toArray();
    return std::any_of(nodes.cbegin(), nodes.cend(), [](const QJsonValue &node) {
        // All UML identifiers share this prefix. Treat unknown UML shapes
        // conservatively here and leave exact shape validation to Notebook.
        return node.toObject().value(QStringLiteral("shape")).toString().startsWith(QStringLiteral("uml-"));
    });
}

int requiredNotebookVersion(const QByteArray &payload)
{
    const QJsonObject root = QJsonDocument::fromJson(payload).object();
    int required = formatVersion(root.value(QStringLiteral("version")).toVariant());
    if (!required) return 0;
    // Derive a lower bound from extended fields too. A stale envelope must not
    // allow an older reader to receive content it cannot open.
    for (const QJsonValue &entry : root.value(QStringLiteral("books")).toArray()) {
        const QJsonObject book = entry.toObject();
        if (!book.value(QStringLiteral("sourceDocuments")).toObject().isEmpty()
            || book.value(QStringLiteral("handwritingLanguage")).toString(QStringLiteral("it")) != QStringLiteral("it"))
            required = qMax(required, 9);
        for (const QJsonValue &page : book.value(QStringLiteral("pages")).toArray()) {
            const QJsonObject layout = page.toObject();
            if (layout.contains(QStringLiteral("source"))) required = qMax(required, 9);
            for (const QJsonValue &item : layout.value(QStringLiteral("items")).toArray()) {
                const QJsonObject object = item.toObject();
                const QJsonObject recovery = object.value(QStringLiteral("handwritingSource")).toObject();
                if (object.contains(QStringLiteral("handwritingSource")) || object.contains(QStringLiteral("rotation"))
                    || object.contains(QStringLiteral("textScale"))) required = qMax(required, 9);
                if (object.contains(QStringLiteral("styleScale"))
                    || recovery.contains(QStringLiteral("baseDiagram")))
                    required = qMax(required, 10);
                if (object.value(QStringLiteral("type")) == QStringLiteral("note")) required = qMax(required, 11);
                if (required < NotebookFormat::umlDiagramShapesVersion && object.value(QStringLiteral("type")) == QStringLiteral("diagram")
                    && (hasUmlNodes(object) || hasUmlNodes(recovery.value(QStringLiteral("baseDiagram")).toObject())))
                    required = NotebookFormat::umlDiagramShapesVersion;
            }
        }
    }
    return required;
}
}

DeltaTransfer::DeltaTransfer(std::function<QByteArray(const QString &)> localBook, QObject *parent, QString storageRoot)
    : QObject(parent), m_localBook(std::move(localBook)), m_storageRoot(std::move(storageRoot))
{
    m_negotiationTimer.setSingleShot(true);
    m_chunkTimer.setSingleShot(true);
    m_receiveTimer.setSingleShot(true);
    connect(&m_receiveTimer, &QTimer::timeout, this, [this] {
        if (!m_incomingMetadata.isEmpty())
            incomingError(tr("Ricezione interrotta."));
    });
    connect(&m_negotiationTimer, &QTimer::timeout, this, [this] {
        m_negotiated = true;
        m_peerNotebookVersion = 8; // The last pre-delta reader supported formats 1–8.
        beginSend();
    });
    connect(&m_chunkTimer, &QTimer::timeout, this, [this] {
        if (!m_connected || m_outgoingMetadata.isEmpty() || m_lastChunkMetadata.isEmpty())
            return;
        if (++m_chunkRetries > 3) {
            const QString request = m_outgoingMetadata.value(QStringLiteral("request")).toString();
            cancelOutgoing(request);
            emit failed(request, tr("Invio interrotto. Le parti ricevute restano disponibili per riprendere il trasferimento."));
            return;
        }
        m_chunkTimer.start(chunkTimeout);
        emit sendMessage(m_lastChunkMetadata, m_lastChunk);
    });
}

void DeltaTransfer::setConnected(bool connected, const QString &peer)
{
    m_connected = connected && hex(peer, 32);
    m_peer = hex(peer, 32) ? peer : QString{};
    m_capable = false;
    m_negotiated = false;
    m_peerNotebookVersion = 0;
    m_negotiationTimer.stop();
    m_chunkTimer.stop();
    cancelOutgoing(m_outgoingMetadata.value(QStringLiteral("request")).toString());
    clearIncoming();
    if (!m_connected)
        return;
    m_helloRequest = QUuid::createUuid().toString(QUuid::Id128);
    m_negotiationTimer.start(1500);
    const QString request = m_helloRequest;
    QTimer::singleShot(0, this, [this, request] {
        if (m_connected && m_helloRequest == request)
            emit sendMessage({{QStringLiteral("type"), QStringLiteral("syncHello")},
                {QStringLiteral("request"), request}, {QStringLiteral("deltaVersion"), 1},
                {QStringLiteral("maxNotebookVersion"), NotebookFormat::maximumSupportedVersion}}, {});
    });
}

void DeltaTransfer::sendPreview(const QVariantMap &metadata, const QByteArray &payload, qsizetype legacyLimit)
{
    cancelOutgoing(m_outgoingMetadata.value(QStringLiteral("request")).toString());
    m_outgoingMetadata = metadata;
    m_outgoingPayload = payload;
    m_legacyLimit = legacyLimit;
    m_completedBytes = 0;
    m_totalBytes = 0;
    emit progressChanged();
    if (m_negotiated)
        beginSend();
}

void DeltaTransfer::beginSend()
{
    if (!m_connected || m_outgoingMetadata.isEmpty() || m_outgoingPayload.isEmpty())
        return;
    const QString request = m_outgoingMetadata.value(QStringLiteral("request")).toString();
    const int requiredVersion = requiredNotebookVersion(m_outgoingPayload);
    if (!requiredVersion || !m_peerNotebookVersion || requiredVersion > m_peerNotebookVersion) {
        cancelOutgoing(request);
        emit failed(request, !requiredVersion ? tr("Il formato del quaderno da trasferire non è valido.")
            : !m_peerNotebookVersion
                ? tr("L’altro dispositivo non ha indicato una versione valida del formato. Aggiorna Quaderno su entrambi i dispositivi e ricollegali.")
                : tr("Questo quaderno richiede il formato %2. Per l’altro dispositivo Quaderno usa il formato %1 come limite di compatibilità. Aggiorna Quaderno sull’altro dispositivo. Evidenziazioni e note restano salvate qui.")
                    .arg(m_peerNotebookVersion).arg(requiredVersion));
        return;
    }
    if (!m_capable) {
        if (m_outgoingPayload.size() > m_legacyLimit) {
            cancelOutgoing(request);
            emit failed(request, tr("L’altro dispositivo usa il trasferimento precedente. Aggiorna Quaderno o passa al Wi-Fi per questo quaderno."));
            return;
        }
        const QVariantMap metadata = m_outgoingMetadata;
        const QByteArray payload = m_outgoingPayload;
        m_outgoingPayload.clear();
        emit sendMessage(metadata, payload);
        return;
    }
    QString error;
    m_package = SyncPackage::split(m_outgoingPayload, &error);
    m_outgoingPayload.clear();
    if (!m_package.isValid()) {
        cancelOutgoing(request);
        emit failed(request, tr("Impossibile preparare il trasferimento incrementale: %1").arg(error));
        return;
    }
    m_completedBytes = 0;
    m_totalBytes = 0;
    for (const QByteArray &bytes : std::as_const(m_package.objects))
        m_totalBytes += bytes.size();
    emit progressChanged();
    QVariantMap offer = m_outgoingMetadata;
    offer[QStringLiteral("type")] = QStringLiteral("deltaOffer");
    offer[QStringLiteral("notebookVersion")] = requiredVersion;
    emit sendMessage(offer, m_package.manifest);
}

bool DeltaTransfer::receiveMessage(const QVariantMap &metadata, const QByteArray &payload)
{
    const QString type = metadata.value(QStringLiteral("type")).toString();
    const bool ours = type == QStringLiteral("syncHello") || type == QStringLiteral("syncHelloReply")
        || type == QStringLiteral("deltaOffer") || type == QStringLiteral("deltaNeed")
        || type == QStringLiteral("deltaChunk") || type == QStringLiteral("deltaError");
    if (!ours)
        return false;
    const QString request = metadata.value(QStringLiteral("request")).toString();
    if (!m_connected || !hex(request, 32))
        return true;
    if ((type == QStringLiteral("syncHello") || type == QStringLiteral("syncHelloReply")) && payload.isEmpty()) {
        if (type == QStringLiteral("syncHelloReply") && request != m_helloRequest)
            return true;
        if (metadata.value(QStringLiteral("deltaVersion")).toInt() == 1) {
            m_capable = true;
            m_negotiated = true;
            // Historical delta-v1 peers (0.2.0–0.2.9) read through format 9.
            // 0.2.10 also omitted this field; use that lower common denominator.
            m_peerNotebookVersion = metadata.contains(QStringLiteral("maxNotebookVersion"))
                ? formatVersion(metadata.value(QStringLiteral("maxNotebookVersion"))) : 9;
            m_negotiationTimer.stop();
        }
        if (type == QStringLiteral("syncHello"))
            emit sendMessage({{QStringLiteral("type"), QStringLiteral("syncHelloReply")},
                {QStringLiteral("request"), request}, {QStringLiteral("deltaVersion"), 1},
                {QStringLiteral("maxNotebookVersion"), NotebookFormat::maximumSupportedVersion}}, {});
        beginSend();
    } else if (type == QStringLiteral("deltaOffer")) {
        receiveOffer(metadata, payload);
    } else if (type == QStringLiteral("deltaNeed") && payload.isEmpty()) {
        receiveNeed(metadata);
    } else if (type == QStringLiteral("deltaChunk")) {
        receiveChunk(metadata, payload);
    } else if (type == QStringLiteral("deltaError") && payload.isEmpty()
               && request == m_outgoingMetadata.value(QStringLiteral("request")).toString()) {
        cancelOutgoing(request);
        emit failed(request, metadata.value(QStringLiteral("errorCode")) == QStringLiteral("format")
            ? tr("L’altro dispositivo non supporta il formato di questo quaderno. Aggiorna Quaderno prima di sincronizzare le annotazioni.")
            : tr("L’altro dispositivo non ha potuto ricevere il trasferimento incrementale. Riprova per riprendere le parti già salvate."));
    }
    return true;
}

QString DeltaTransfer::cacheDirectory() const
{
    return m_peer.isEmpty() ? QString{} : (m_storageRoot.isEmpty() ? dataDirectory() : m_storageRoot) + QLatin1Char('/') + m_peer;
}

QString DeltaTransfer::objectPath(const QString &hash, bool partial) const
{
    if (!hex(hash, 64) || m_peer.isEmpty())
        return {};
    return cacheDirectory() + QLatin1Char('/') + hash + (partial ? QStringLiteral(".part") : QString{});
}

QByteArray DeltaTransfer::readObject(const QString &hash, qsizetype expected) const
{
    QFile file(objectPath(hash));
    if (expected <= 0 || !file.open(QIODevice::ReadOnly) || file.size() != expected)
        return {};
    const QByteArray bytes = file.read(expected + 1);
    return bytes.size() == expected && SyncPackage::digest(bytes) == hash ? bytes : QByteArray{};
}

bool DeltaTransfer::storeObject(const QString &hash, const QByteArray &bytes)
{
    if (bytes.isEmpty() || SyncPackage::digest(bytes) != hash || !QDir().mkpath(cacheDirectory()))
        return false;
    QFile::setPermissions(cacheDirectory(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QSaveFile file(objectPath(hash));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return file.write(bytes) == bytes.size() && file.commit();
}

void DeltaTransfer::receiveOffer(const QVariantMap &metadata, const QByteArray &manifest)
{
    const QString request = metadata.value(QStringLiteral("request")).toString();
    if (metadata.contains(QStringLiteral("notebookVersion"))) {
        const int version = formatVersion(metadata.value(QStringLiteral("notebookVersion")));
        if (!version || version > NotebookFormat::maximumSupportedVersion) {
            emit sendMessage({{QStringLiteral("type"), QStringLiteral("deltaError")},
                {QStringLiteral("request"), request}, {QStringLiteral("errorCode"), QStringLiteral("format")}}, {});
            return;
        }
    }
    const QString book = metadata.value(QStringLiteral("bookId")).toString();
    const QString base = metadata.value(QStringLiteral("base")).toString();
    QString error;
    const auto sizes = SyncPackage::objectSizes(manifest, &error);
    if (book.isEmpty() || book.size() > 256 || (!base.isEmpty() && !hex(base, 64)) || sizes.isEmpty()) {
        emit sendMessage({{QStringLiteral("type"), QStringLiteral("deltaError")},
            {QStringLiteral("request"), request}}, {});
        return;
    }
    clearIncoming();
    m_incomingMetadata = metadata;
    m_incomingMetadata[QStringLiteral("type")] = QStringLiteral("preview");
    m_manifest = manifest;
    m_sizes = sizes;
    m_objectOrder = sizes.keys();
    m_receiveTimer.start(15 * 60 * 1000);
    emit receivingChanged();
    const QSet<QString> wanted(sizes.keyBegin(), sizes.keyEnd());
    pruneCache(wanted);

    // Seed content-addressed objects from the current local version. This also
    // saves bandwidth on the first incremental sync after upgrading an old peer.
    const QByteArray local = m_localBook ? m_localBook(book) : QByteArray{};
    if (!local.isEmpty()) {
        const auto seed = SyncPackage::split(local);
        for (auto it = seed.objects.cbegin(); it != seed.objects.cend(); ++it) {
            if (sizes.contains(it.key()) && readObject(it.key(), sizes.value(it.key())).isEmpty())
                storeObject(it.key(), it.value());
        }
    }
    QTimer::singleShot(0, this, &DeltaTransfer::requestNext);
}

void DeltaTransfer::requestNext()
{
    if (!m_connected || m_incomingMetadata.isEmpty())
        return;
    while (m_nextObject < m_objectOrder.size()) {
        const QString hash = m_objectOrder.at(m_nextObject);
        const qsizetype size = m_sizes.value(hash);
        if (m_verified.contains(hash)) {
            m_receivedBytes += size;
            ++m_nextObject;
            continue;
        }
        if (!readObject(hash, size).isEmpty()) {
            m_verified.insert(hash);
            m_receivedBytes += size;
            ++m_nextObject;
            continue;
        }
        m_wantedHash = hash;
        QFile partial(objectPath(hash, true));
        qint64 offset = QFileInfo(partial).size();
        if (offset >= size) {
            if (offset == size && partial.open(QIODevice::ReadOnly)) {
                const QByteArray bytes = partial.readAll();
                partial.close();
                if (SyncPackage::digest(bytes) == hash && storeObject(hash, bytes)) {
                    partial.remove();
                    m_verified.insert(hash);
                    m_receivedBytes += size;
                    ++m_nextObject;
                    continue;
                }
            }
            partial.remove();
            offset = 0;
        }
        emit sendMessage({{QStringLiteral("type"), QStringLiteral("deltaNeed")},
            {QStringLiteral("request"), m_incomingMetadata.value(QStringLiteral("request"))},
            {QStringLiteral("hash"), hash}, {QStringLiteral("offset"), offset},
            {QStringLiteral("received"), m_receivedBytes + offset}}, {});
        return;
    }
    QString error;
    const QByteArray payload = SyncPackage::assemble(m_manifest, [this](const QString &hash) {
        return readObject(hash, m_sizes.value(hash));
    }, &error);
    if (payload.isEmpty()) {
        incomingError(error);
        return;
    }
    const QVariantMap metadata = m_incomingMetadata;
    clearIncoming();
    pruneCache();
    emit previewReady(metadata, payload);
}

void DeltaTransfer::receiveNeed(const QVariantMap &metadata)
{
    const QString request = metadata.value(QStringLiteral("request")).toString();
    if (request != m_outgoingMetadata.value(QStringLiteral("request")).toString() || !m_package.isValid())
        return;
    const QString hash = metadata.value(QStringLiteral("hash")).toString();
    const double rawOffset = metadata.value(QStringLiteral("offset"), -1).toDouble();
    const auto found = m_package.objects.constFind(hash);
    if (found == m_package.objects.cend() || !metadata.contains(QStringLiteral("offset"))
        || !std::isfinite(rawOffset) || rawOffset < 0 || rawOffset >= found.value().size() || std::floor(rawOffset) != rawOffset) {
        cancelOutgoing(request);
        emit failed(request, tr("La richiesta di una parte del quaderno non è valida."));
        return;
    }
    const qsizetype offset = qsizetype(rawOffset);
    m_completedBytes = qBound(qsizetype{0}, qsizetype(metadata.value(QStringLiteral("received")).toLongLong()), m_totalBytes);
    emit progressChanged();
    m_lastChunk = found.value().sliced(offset, qMin(chunkBytes, found.value().size() - offset));
    m_lastChunkMetadata = {{QStringLiteral("type"), QStringLiteral("deltaChunk")},
        {QStringLiteral("request"), request}, {QStringLiteral("hash"), hash},
        {QStringLiteral("offset"), offset}, {QStringLiteral("chunkHash"), SyncPackage::digest(m_lastChunk)}};
    m_chunkRetries = 0;
    m_chunkTimer.start(chunkTimeout);
    const QVariantMap outgoing = m_lastChunkMetadata;
    const QByteArray bytes = m_lastChunk;
    QTimer::singleShot(0, this, [this, outgoing, bytes, request] {
        if (m_connected && request == m_outgoingMetadata.value(QStringLiteral("request")).toString())
            emit sendMessage(outgoing, bytes);
    });
}

void DeltaTransfer::receiveChunk(const QVariantMap &metadata, const QByteArray &payload)
{
    if (metadata.value(QStringLiteral("request")) != m_incomingMetadata.value(QStringLiteral("request"))
        || m_incomingMetadata.isEmpty())
        return;
    const QString hash = metadata.value(QStringLiteral("hash")).toString();
    const double rawOffset = metadata.value(QStringLiteral("offset"), -1).toDouble();
    if (!m_sizes.contains(hash) || payload.isEmpty() || payload.size() > chunkBytes
        || !std::isfinite(rawOffset) || rawOffset < 0 || rawOffset > m_sizes.value(hash) - payload.size() || std::floor(rawOffset) != rawOffset
        || SyncPackage::digest(payload) != metadata.value(QStringLiteral("chunkHash")).toString()) {
        incomingError(tr("Parte del quaderno non valida."));
        return;
    }
    if (m_verified.contains(hash)) {
        QTimer::singleShot(0, this, &DeltaTransfer::requestNext);
        return;
    }
    if (hash != m_wantedHash)
        return;
    m_receiveTimer.start(15 * 60 * 1000);
    if (!QDir().mkpath(cacheDirectory())) {
        incomingError(tr("Impossibile preparare il trasferimento."));
        return;
    }
    QFile::setPermissions(cacheDirectory(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QFile partial(objectPath(hash, true));
    if (!partial.open(QIODevice::ReadWrite)) {
        incomingError(tr("Impossibile salvare una parte del quaderno."));
        return;
    }
    partial.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const qint64 offset = qint64(rawOffset);
    if (offset < partial.size()) {
        if (!partial.seek(offset) || partial.read(payload.size()) != payload) {
            incomingError(tr("Una parte del quaderno non corrisponde ai dati salvati."));
            return;
        }
    } else if (offset != partial.size() || !partial.seek(offset)
               || partial.write(payload) != payload.size() || !durableFlush(partial)) {
        incomingError(tr("Impossibile salvare una parte del quaderno."));
        return;
    }
    if (partial.size() == m_sizes.value(hash)) {
        partial.seek(0);
        const QByteArray bytes = partial.readAll();
        partial.close();
        if (SyncPackage::digest(bytes) != hash) {
            partial.remove();
            if (++m_integrityRetries[hash] > 1) {
                incomingError(tr("La verifica del quaderno non è riuscita."));
                return;
            }
        } else if (!storeObject(hash, bytes)) {
            incomingError(tr("Impossibile completare il salvataggio della parte ricevuta."));
            return;
        } else {
            partial.remove();
            m_verified.insert(hash);
        }
    }
    // Acknowledgment follows the durable write. On reconnect the next offer asks
    // only for missing hashes or the remaining bytes of an incomplete object.
    QTimer::singleShot(0, this, &DeltaTransfer::requestNext);
}

void DeltaTransfer::incomingError(const QString &message)
{
    Q_UNUSED(message)
    const QString request = m_incomingMetadata.value(QStringLiteral("request")).toString();
    clearIncoming();
    emit sendMessage({{QStringLiteral("type"), QStringLiteral("deltaError")},
        {QStringLiteral("request"), request}}, {});
}

void DeltaTransfer::cancelOutgoing(const QString &request)
{
    if (request != m_outgoingMetadata.value(QStringLiteral("request")).toString())
        return;
    m_chunkTimer.stop();
    m_outgoingMetadata.clear();
    m_outgoingPayload.clear();
    m_package = {};
    m_lastChunk.clear();
    m_lastChunkMetadata.clear();
}

void DeltaTransfer::cancelIncoming(const QString &request)
{
    if (request == m_incomingMetadata.value(QStringLiteral("request")).toString())
        clearIncoming();
}

void DeltaTransfer::clearIncoming()
{
    const bool wasReceiving = receiving();
    m_receiveTimer.stop();
    m_incomingMetadata.clear();
    m_manifest.clear();
    m_sizes.clear();
    m_objectOrder.clear();
    m_nextObject = 0;
    m_receivedBytes = 0;
    m_verified.clear();
    m_wantedHash.clear();
    m_integrityRetries.clear();
    if (wasReceiving) emit receivingChanged();
}

void DeltaTransfer::forgetPeer(const QString &peer)
{
    if (hex(peer, 32))
        QDir((m_storageRoot.isEmpty() ? dataDirectory() : m_storageRoot) + QLatin1Char('/') + peer).removeRecursively();
}

void DeltaTransfer::pruneCache(const QSet<QString> &keep)
{
    if (m_peer.isEmpty())
        return;
    QDir directory(cacheDirectory());
    auto files = directory.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Time | QDir::Reversed);
    qint64 total = 0;
    for (const auto &file : files)
        total += file.size();
    const QDateTime oldest = QDateTime::currentDateTimeUtc().addDays(-30);
    for (const auto &file : files) {
        QString hash = file.fileName();
        if (hash.endsWith(QStringLiteral(".part")))
            hash.chop(5);
        if (hex(hash, 64) && !keep.contains(hash) && (total > cacheBytes || file.lastModified() < oldest)) {
            if (QFile::remove(file.absoluteFilePath()))
                total -= file.size();
        }
    }
}
