#include "pagestore.h"
#include "syncpackage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QLockFile>
#include <QJsonArray>
#include <QCryptographicHash>
#include <cmath>

namespace {
constexpr auto localFormat = "quaderno-pages-v1";
constexpr qsizetype maximumObjects = 100000;
constexpr int maximumDepth = 64;
QByteArray expandLocal(const QString &path, const QByteArray &bytes, QString *error);
QMap<QString, QJsonObject> localObjectRecords(const QByteArray &bytes, QString *error);
bool atomicWrite(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QByteArray boundedRead(const QString &path, qsizetype maximum, QString *error)
{
    QFile file(path);
    if (!file.exists()) return {};
    if (QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > maximum) {
        if (error) *error = QStringLiteral("Cannot read library object: %1").arg(path);
        return {};
    }
    const QByteArray bytes = file.read(maximum + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() > maximum) {
        if (error) *error = QStringLiteral("Incomplete library object: %1").arg(path);
        return {};
    }
    return bytes.isNull() ? QByteArray("") : bytes;
}

QByteArray expand(const QString &path, const QByteArray &bytes, QString *error)
{
    if (!PageStore::isManifest(bytes)) {
        if (!QJsonDocument::fromJson(bytes).object().value(QStringLiteral("books")).isArray()) {
            if (error) *error = QStringLiteral("Invalid library snapshot.");
            return {};
        }
        return bytes;
    }
    if (QJsonDocument::fromJson(bytes).object().value(QStringLiteral("format")) == QLatin1String(localFormat))
        return expandLocal(path, bytes, error);
    return SyncPackage::assemble(bytes, [&](const QString &hash) {
        return boundedRead(path + QStringLiteral(".objects/") + hash, PageStore::maximumLibraryBytes, nullptr);
    }, error, PageStore::maximumLibraryBytes);
}

void collectUnused(const QString &path, const QByteArray &current, const QByteArray &previous)
{
    QSet<QString> retained;
    for (const auto &snapshot : {current, previous}) {
        if (!PageStore::isManifest(snapshot)) continue;
        QString error;
        if (QJsonDocument::fromJson(snapshot).object().value(QStringLiteral("format")) == QLatin1String(localFormat)) {
            const auto records = localObjectRecords(snapshot, &error);
            if (!error.isEmpty()) return;
            for (auto it = records.cbegin(); it != records.cend(); ++it) retained.insert(it.key());
        } else {
            const auto sizes = SyncPackage::objectSizes(snapshot, &error, PageStore::maximumLibraryBytes);
            if (!error.isEmpty()) return;
            for (auto it = sizes.cbegin(); it != sizes.cend(); ++it) retained.insert(it.key());
        }
    }
    QDir objects(path + QStringLiteral(".objects"));
    const QStringList names = objects.entryList(QDir::Files | QDir::NoSymLinks);
    for (const QString &name : names) {
        const QByteArray hash = name.toLatin1();
        if (hash.size() == 64 && QByteArray::fromHex(hash).toHex() == hash && !retained.contains(name))
            objects.remove(name);
    }
}
}

bool PageStore::isManifest(const QByteArray &bytes)
{
    const auto format = QJsonDocument::fromJson(bytes).object().value(QStringLiteral("format"));
    return format == QStringLiteral("quaderno-objects-v1") || format == QLatin1String(localFormat);
}

PageStore::ReadResult PageStore::read(const QString &path)
{
    ReadResult result;
    QLockFile lock(path + QStringLiteral(".lock"));
    if ((QFileInfo::exists(path) || QFileInfo::exists(path + QStringLiteral(".previous"))) && !lock.tryLock(0)) {
        result.error = QStringLiteral("Library is being saved by another process. Try again.");
        return result;
    }
    result.currentBytes = boundedRead(path, maximumLibraryBytes, &result.error);
    if (result.currentBytes.isNull() && result.error.isEmpty()) {
        if (!QFileInfo::exists(path + QStringLiteral(".previous"))) return result;
        result.error = QStringLiteral("The current library manifest is missing.");
    }
    if (result.error.isEmpty()) result.payload = expand(path, result.currentBytes, &result.error);
    if (!result.error.isEmpty()) {
        QString previousError;
        const QByteArray previous = boundedRead(path + QStringLiteral(".previous"), maximumLibraryBytes, &previousError);
        if (previousError.isEmpty() && !previous.isNull()) {
            const QByteArray payload = expand(path, previous, &previousError);
            if (previousError.isEmpty()) {
                result.payload = payload;
                result.recovered = true;
                result.error.clear();
            }
        }
    }
    return result;
}

PageStore::ReadResult PageStore::readForBackup(const QString &path)
{
    ReadResult result;
    result.currentBytes = boundedRead(path, maximumLibraryBytes, &result.error);
    if (result.currentBytes.isNull() && result.error.isEmpty())
        result.error = QStringLiteral("The library has not been saved yet.");
    if (result.error.isEmpty()) result.payload = expand(path, result.currentBytes, &result.error);
    return result;
}

bool PageStore::write(const QString &path, const QByteArray &payload, const QByteArray &expected,
                      QByteArray *manifest, QString *error, bool recovering)
{
    if (error) error->clear();
    QString readError;
    const QByteArray current = boundedRead(path, maximumLibraryBytes, &readError);
    if (!readError.isEmpty() || current != expected) {
        if (error) *error = readError.isEmpty() ? QStringLiteral("Library changed in another process.") : readError;
        return false;
    }
    const auto package = SyncPackage::split(payload, error, maximumLibraryBytes);
    if (!package.isValid()) return false;
    const QString objectPath = path + QStringLiteral(".objects");
    if (!QDir().mkpath(objectPath)) {
        if (error) *error = QStringLiteral("Cannot create page storage.");
        return false;
    }
    for (auto it = package.objects.cbegin(); it != package.objects.cend(); ++it) {
        const QString target = QDir(objectPath).filePath(it.key());
        const QByteArray stored = boundedRead(target, maximumLibraryBytes, nullptr);
        if (stored == it.value()) continue;
        if (!atomicWrite(target, it.value(), error)) return false;
    }
    QByteArray previous = current;
    if (recovering) {
        // Retain the damaged snapshot for diagnosis and preserve the last valid
        // generation instead of replacing it with corrupt data during recovery.
        if (!current.isNull() && !atomicWrite(path + QStringLiteral(".damaged"), current, error)) return false;
        previous = boundedRead(path + QStringLiteral(".previous"), maximumLibraryBytes, error);
        if (previous.isNull()) return false;
    } else if (!current.isNull()) {
        if (!isManifest(current) && !QFileInfo::exists(path + QStringLiteral(".legacy"))
            && !atomicWrite(path + QStringLiteral(".legacy"), current, error)) return false;
        if (!atomicWrite(path + QStringLiteral(".previous"), current, error)) return false;
    }
    if (!atomicWrite(path, package.manifest, error)) return false;
    if (manifest) *manifest = package.manifest;
    // Current and immediately previous generations stay valid. Orphan objects
    // from interrupted commits can be removed only after the new commit exists.
    collectUnused(path, package.manifest, previous);
    return true;
}

namespace {
bool storageFailure(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}

bool validObjectHash(const QString &value)
{
    const QByteArray bytes = value.toLatin1();
    return bytes.size() == 64 && QByteArray::fromHex(bytes).toHex() == bytes;
}

QByteArray compactJson(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QMap<QString, QJsonObject> localObjectRecords(const QByteArray &bytes, QString *error)
{
    QMap<QString, QJsonObject> result;
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    const auto manifest = document.object();
    const auto root = manifest.value(QStringLiteral("root"));
    const auto records = manifest.value(QStringLiteral("objects"));
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || manifest.value(QStringLiteral("format")) != QLatin1String(localFormat)
        || !root.isObject() || !records.isArray() || records.toArray().size() > maximumObjects
        || SyncPackage::digest(compactJson(root.toObject())) != manifest.value(QStringLiteral("rootHash")).toString()) {
        storageFailure(error, QStringLiteral("Invalid page manifest."));
        return {};
    }
    qint64 total = 0;
    for (const auto &value : records.toArray()) {
        const auto record = value.toObject();
        const QString hash = record.value(QStringLiteral("hash")).toString();
        const double size = record.value(QStringLiteral("bytes")).toDouble(-1);
        const auto dependencies = record.value(QStringLiteral("dependencies"));
        const double expanded = record.value(QStringLiteral("expandedBytes")).toDouble(-1);
        const double jsonSize = record.value(QStringLiteral("jsonBytes")).toDouble(size);
        if (expanded < size || expanded > PageStore::maximumLibraryBytes || std::floor(expanded) != expanded
            || jsonSize < size || jsonSize > PageStore::maximumLibraryBytes || std::floor(jsonSize) != jsonSize
            || !validObjectHash(hash) || result.contains(hash) || size < 1 || std::floor(size) != size
            || size > PageStore::maximumLibraryBytes || total > PageStore::maximumLibraryBytes - size
            || (!dependencies.isUndefined() && (!dependencies.isArray() || dependencies.toArray().size() > maximumObjects))) {
            storageFailure(error, QStringLiteral("Invalid page object descriptor."));
            return {};
        }
        total += qint64(size);
        result.insert(hash, record);
    }
    for (auto it = result.cbegin(); it != result.cend(); ++it) {
        for (const auto &dependency : it.value().value(QStringLiteral("dependencies")).toArray()) {
            if (!dependency.isString() || !result.contains(dependency.toString()) || dependency.toString() == it.key()) {
                storageFailure(error, QStringLiteral("Missing immutable page asset."));
                return {};
            }
        }
    }
    return result;
}

bool restoreValue(QJsonValue &value, const QJsonArray &path, const QJsonValue &replacement, int at = 0)
{
    if (path.size() > maximumDepth) return false;
    if (at == path.size()) {
        if (!value.isNull()) return false;
        value = replacement;
        return true;
    }
    if (value.isObject() && path.at(at).isString()) {
        auto object = value.toObject();
        const QString key = path.at(at).toString();
        if (!object.contains(key)) return false;
        auto child = object.value(key);
        if (!restoreValue(child, path, replacement, at + 1)) return false;
        object.insert(key, child); value = object; return true;
    }
    if (value.isArray() && path.at(at).isDouble()) {
        auto array = value.toArray();
        const int index = path.at(at).toInt(-1);
        if (index < 0 || index >= array.size() || path.at(at).toDouble() != index) return false;
        auto child = array.at(index);
        if (!restoreValue(child, path, replacement, at + 1)) return false;
        array.replace(index, child); value = array; return true;
    }
    return false;
}

QByteArray expandLocal(const QString &path, const QByteArray &bytes, QString *error)
{
    const auto records = localObjectRecords(bytes, error);
    if (error && !error->isEmpty()) return {};
    const auto manifest = QJsonDocument::fromJson(bytes).object();
    qint64 expandedBytes = 0;
    QHash<QString, QByteArray> loaded;
    const auto readObject = [&](const QString &hash) -> QByteArray {
        if (loaded.contains(hash)) return loaded.value(hash);
        const auto found = records.constFind(hash);
        if (found == records.cend()) {
            storageFailure(error, QStringLiteral("Missing page object reference.")); return {};
        }
        const auto data = boundedRead(path + QStringLiteral(".objects/") + hash, PageStore::maximumLibraryBytes, error);
        if (data.isNull() || data.size() != found->value(QStringLiteral("bytes")).toInteger()
            || SyncPackage::digest(data) != hash) {
            storageFailure(error, QStringLiteral("A page object is missing or damaged.")); return {};
        }
        loaded.insert(hash, data);
        return data;
    };
    const auto expandCapsule = [&](const QJsonObject &capsule) -> QJsonValue {
        if (!capsule.contains(QStringLiteral("value")) || !capsule.value(QStringLiteral("blobs")).isArray()) {
            storageFailure(error, QStringLiteral("Invalid page data.")); return {};
        }
        QJsonValue value = capsule.value(QStringLiteral("value"));
        for (const auto &entry : capsule.value(QStringLiteral("blobs")).toArray()) {
            const auto blob = entry.toObject();
            const auto location = blob.value(QStringLiteral("path"));
            const QByteArray data = readObject(blob.value(QStringLiteral("hash")).toString());
            if ((error && !error->isEmpty()) || data.isNull()) return {};
            expandedBytes += data.size();
            if (expandedBytes > PageStore::maximumLibraryBytes || !location.isArray()) {
                storageFailure(error, QStringLiteral("Expanded library exceeds its size limit.")); return {};
            }
            const QString text = QString::fromUtf8(data);
            if (text.toUtf8() != data || !restoreValue(value, location.toArray(), text)) {
                storageFailure(error, QStringLiteral("Invalid immutable text/image asset.")); return {};
            }
        }
        return value;
    };
    QJsonValue rootValue = expandCapsule(manifest.value(QStringLiteral("root")).toObject());
    if ((error && !error->isEmpty()) || !rootValue.isObject()) return {};
    auto root = rootValue.toObject();
    if (!root.value(QStringLiteral("books")).isArray()) {
        storageFailure(error, QStringLiteral("Invalid library metadata.")); return {};
    }
    auto books = root.value(QStringLiteral("books")).toArray();
    for (qsizetype index = 0; index < books.size(); ++index) {
        auto book = books.at(index).toObject();
        if (!book.value(QStringLiteral("pages")).isArray()) {
            storageFailure(error, QStringLiteral("Invalid page collection.")); return {};
        }
        auto pages = book.value(QStringLiteral("pages")).toArray();
        for (qsizetype page = 0; page < pages.size(); ++page) {
            if (!pages.at(page).isString()) {
                storageFailure(error, QStringLiteral("Invalid page reference.")); return {};
            }
            const QByteArray data = readObject(pages.at(page).toString());
            if (error && !error->isEmpty()) return {};
            expandedBytes += data.size();
            if (expandedBytes > PageStore::maximumLibraryBytes) {
                storageFailure(error, QStringLiteral("Expanded library exceeds its size limit.")); return {};
            }
            const QJsonValue value = expandCapsule(QJsonDocument::fromJson(data).object());
            if (error && !error->isEmpty()) return {};
            pages.replace(page, value);
        }
        book.insert(QStringLiteral("pages"), pages);
        books.replace(index, book);
    }
    root.insert(QStringLiteral("books"), books);
    const QByteArray payload = compactJson(root);
    if (payload.size() > PageStore::maximumLibraryBytes) {
        storageFailure(error, QStringLiteral("Expanded library exceeds its size limit.")); return {};
    }
    return payload;
}
}

PageStore::Builder::Builder(const QByteArray &previousManifest, BlobCache *cache) : m_cache(cache)
{
    if (QJsonDocument::fromJson(previousManifest).object().value(QStringLiteral("format")) != QLatin1String(localFormat))
        return;
    QString error;
    m_previousObjects = localObjectRecords(previousManifest, &error);
    if (!error.isEmpty()) { m_previousObjects.clear(); return; }
    m_previousRoot = QJsonDocument::fromJson(previousManifest).object().value(QStringLiteral("root")).toObject()
        .value(QStringLiteral("value")).toObject();
    for (const auto &value : m_previousRoot.value(QStringLiteral("books")).toArray()) {
        const auto book = value.toObject();
        const QString id = book.value(QStringLiteral("id")).toString();
        const auto ids = book.value(QStringLiteral("pageIds")).toArray();
        const auto pages = book.value(QStringLiteral("pages")).toArray();
        if (ids.size() != pages.size()) continue;
        for (qsizetype index = 0; index < ids.size(); ++index)
            m_previousPageRefs.insert(id + u'/' + ids.at(index).toString(), pages.at(index).toString());
    }
}

bool PageStore::Builder::retain(const QString &hash)
{
    if (m_objects.contains(hash)) return true;
    const auto found = m_previousObjects.constFind(hash);
    if (found == m_previousObjects.cend()) return false;
    m_objects.insert(hash, *found);
    for (const auto &dependency : found->value(QStringLiteral("dependencies")).toArray())
        if (!retain(dependency.toString())) return false;
    return true;
}

QString PageStore::Builder::reusePage(const QString &bookId, const QString &pageId)
{
    const QString hash = m_previousPageRefs.value(bookId + u'/' + pageId);
    return !hash.isEmpty() && retain(hash) ? hash : QString{};
}

QString PageStore::Builder::addObject(const QByteArray &bytes, const QJsonArray &blobs)
{
    const QString hash = SyncPackage::digest(bytes);
    QJsonObject record{{QStringLiteral("hash"), hash}, {QStringLiteral("bytes"), bytes.size()}};
    qint64 expanded = bytes.size();
    for (const auto &value : blobs) {
        const auto dependency = m_objects.value(value.toObject().value(QStringLiteral("hash")).toString());
        expanded += dependency.value(QStringLiteral("jsonBytes")).toInteger(dependency.value(QStringLiteral("bytes")).toInteger());
    }
    record.insert(QStringLiteral("expandedBytes"), expanded);
    if (!blobs.isEmpty()) {
        QJsonArray dependencies;
        QSet<QString> seen;
        for (const auto &value : blobs) {
            const QString dependency = value.toObject().value(QStringLiteral("hash")).toString();
            if (!seen.contains(dependency)) { dependencies.append(dependency); seen.insert(dependency); }
        }
        record.insert(QStringLiteral("dependencies"), dependencies);
    }
    m_objects.insert(hash, record);
    if (!m_previousObjects.contains(hash))
        m_changedObjects.insert(hash, bytes);
    return hash;
}

QString PageStore::Builder::addBlob(const QString &text)
{
    const quintptr identity = reinterpret_cast<quintptr>(text.constData());
    if (m_cache) {
        const auto found = m_cache->entries.constFind(identity);
        if (found != m_cache->entries.cend() && found->text.size() == text.size() && retain(found->hash))
            return found->hash;
    }
    const QByteArray bytes = text.toUtf8();
    m_encodedBlobBytes += bytes.size();
    const QString hash = addObject(bytes);
    qint64 jsonSize = bytes.size() + 2; // quotes plus JSON escapes
    for (const unsigned char ch : bytes) {
        if (ch == '"' || ch == '\\') ++jsonSize;
        else if (ch < 0x20) jsonSize += (ch == '\b' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t') ? 1 : 5;
    }
    auto record = m_objects.value(hash);
    record.insert(QStringLiteral("jsonBytes"), jsonSize);
    m_objects.insert(hash, record);
    if (m_cache) {
        // Keep at most two shared allocations of the same bytes, even after
        // repeated paste/import operations create equal strings independently.
        int copies = 0;
        for (auto it = m_cache->entries.begin(); it != m_cache->entries.end(); ) {
            if (it.key() != identity && it->hash == hash && ++copies >= 2) it = m_cache->entries.erase(it);
            else ++it;
        }
        m_cache->entries.insert(identity, BlobCache::Entry{text, hash, bytes.size()});
    }
    return hash;
}

bool PageStore::Builder::encode(const QVariant &value, QJsonValue *json, QJsonArray *blobs,
                                const QJsonArray &path, QString *error, int depth)
{
    if (depth > maximumDepth || m_objects.size() > maximumObjects)
        return storageFailure(error, QStringLiteral("Library object is too complex."));
    if (value.metaType().id() == QMetaType::QString) {
        const QString text = value.toString();
        if (text.size() >= 4096) {
            const QString hash = addBlob(text);
            blobs->append(QJsonObject{{QStringLiteral("path"), path}, {QStringLiteral("hash"), hash}});
            *json = QJsonValue::Null;
        } else *json = text;
    } else if (value.metaType().id() == QMetaType::QVariantList || value.metaType().id() == QMetaType::QStringList) {
        const QVariantList list = value.toList();
        QJsonArray result;
        for (qsizetype index = 0; index < list.size(); ++index) {
            QJsonArray childPath = path; childPath.append(index);
            QJsonValue child;
            if (!encode(list.at(index), &child, blobs, childPath, error, depth + 1)) return false;
            result.append(child);
        }
        *json = result;
    } else if (value.metaType().id() == QMetaType::QVariantMap || value.metaType().id() == QMetaType::QVariantHash) {
        const QVariantMap map = value.toMap();
        QJsonObject result;
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            QJsonArray childPath = path; childPath.append(it.key());
            QJsonValue child;
            if (!encode(it.value(), &child, blobs, childPath, error, depth + 1)) return false;
            result.insert(it.key(), child);
        }
        *json = result;
    } else *json = QJsonValue::fromVariant(value);
    return true;
}

QString PageStore::Builder::addPage(const QVariant &page, QString *error)
{
    QJsonValue json;
    QJsonArray blobs;
    if (!encode(page, &json, &blobs, {}, error)) return {};
    return addObject(compactJson({{QStringLiteral("value"), json}, {QStringLiteral("blobs"), blobs}}), blobs);
}

bool PageStore::Builder::finish(const QVariantMap &metadata, Update *update, QString *error)
{
    QJsonValue root;
    QJsonArray blobs;
    if (!encode(metadata, &root, &blobs, {}, error)) return false;
    const QJsonObject capsule{{QStringLiteral("value"), root}, {QStringLiteral("blobs"), blobs}};
    qint64 expanded = compactJson(capsule).size();
    for (const auto &value : blobs) {
        const auto record = m_objects.value(value.toObject().value(QStringLiteral("hash")).toString());
        expanded += record.value(QStringLiteral("jsonBytes")).toInteger(record.value(QStringLiteral("bytes")).toInteger());
    }
    for (const auto &value : metadata.value(QStringLiteral("books")).toList()) {
        for (const auto &page : value.toMap().value(QStringLiteral("pages")).toList()) {
            const auto record = m_objects.constFind(page.toString());
            if (record == m_objects.cend()) return storageFailure(error, QStringLiteral("Missing page reference."));
            expanded += record->value(QStringLiteral("expandedBytes")).toInteger();
        }
    }
    if (expanded > maximumLibraryBytes)
        return storageFailure(error, QStringLiteral("Expanded library exceeds its size limit."));
    QJsonArray records;
    for (const auto &record : std::as_const(m_objects)) records.append(record);
    update->manifest = compactJson({{QStringLiteral("format"), QLatin1String(localFormat)},
        {QStringLiteral("root"), capsule}, {QStringLiteral("rootHash"), SyncPackage::digest(compactJson(capsule))},
        {QStringLiteral("objects"), records}});
    QString validation;
    localObjectRecords(update->manifest, &validation);
    if (!validation.isEmpty()) return storageFailure(error, validation);
    update->changedObjects = m_changedObjects;
    if (m_cache) {
        for (auto it = m_cache->entries.begin(); it != m_cache->entries.end(); ) {
            if (!m_objects.contains(it->hash)) it = m_cache->entries.erase(it); else ++it;
        }
    }
    return true;
}

bool PageStore::writeUpdate(const QString &path, const Update &update, const QByteArray &expected,
                            QByteArray *manifest, QString *error, bool recovering)
{
    if (error) error->clear();
    QString readError;
    const QByteArray current = boundedRead(path, maximumLibraryBytes, &readError);
    if (!readError.isEmpty() || current != expected)
        return storageFailure(error, readError.isEmpty() ? QStringLiteral("Library changed in another process.") : readError);
    const auto records = localObjectRecords(update.manifest, error);
    if (error && !error->isEmpty()) return false;
    const QString directory = path + QStringLiteral(".objects");
    if (!QDir().mkpath(directory)) return storageFailure(error, QStringLiteral("Cannot create page storage."));
    // Only newly changed objects are opened or written. Existing immutable
    // assets are referenced by digest; every byte is verified again on load.
    for (auto it = update.changedObjects.cbegin(); it != update.changedObjects.cend(); ++it) {
        if (!records.contains(it.key()) || SyncPackage::digest(it.value()) != it.key())
            return storageFailure(error, QStringLiteral("Invalid changed page object."));
        const QString target = QDir(directory).filePath(it.key());
        if (!atomicWrite(target, it.value(), error)) return false;
    }
    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        const QFileInfo object(QDir(directory).filePath(it.key()));
        if (object.isSymLink() || !object.isFile() || object.size() != it.value().value(QStringLiteral("bytes")).toInteger())
            return storageFailure(error, QStringLiteral("An immutable page asset is missing. Reopen the library to recover."));
    }
    QByteArray previous = current;
    if (recovering) {
        if (!current.isNull() && !atomicWrite(path + QStringLiteral(".damaged"), current, error)) return false;
        previous = boundedRead(path + QStringLiteral(".previous"), maximumLibraryBytes, error);
        if (previous.isNull()) return false;
    } else if (!current.isNull()) {
        if (!isManifest(current) && !QFileInfo::exists(path + QStringLiteral(".legacy"))
            && !atomicWrite(path + QStringLiteral(".legacy"), current, error)) return false;
        if (!atomicWrite(path + QStringLiteral(".previous"), current, error)) return false;
    }
    if (!atomicWrite(path, update.manifest, error)) return false;
    if (manifest) *manifest = update.manifest;
    collectUnused(path, update.manifest, previous);
    return true;
}
