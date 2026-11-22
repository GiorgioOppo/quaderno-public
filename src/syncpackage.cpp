#include "syncpackage.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace {
constexpr qsizetype maxObjects = 100000;
constexpr int maxDepth = 64;
constexpr qsizetype stringThreshold = 4096;

bool fail(QString *error, const char *message)
{
    if (error)
        *error = QString::fromUtf8(message);
    return false;
}

bool validHash(const QString &value)
{
    const QByteArray bytes = value.toLatin1();
    return bytes.size() == 64 && QByteArray::fromHex(bytes).toHex() == bytes;
}

QByteArray jsonBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool boundedDepth(const QJsonValue &value, int depth = 0)
{
    if (depth > maxDepth)
        return false;
    if (value.isArray()) {
        for (const auto &child : value.toArray()) {
            if (!boundedDepth(child, depth + 1))
                return false;
        }
    } else if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!boundedDepth(it.value(), depth + 1))
                return false;
        }
    }
    return true;
}

QString addObject(SyncPackage::Package &package, const QByteArray &bytes)
{
    const QString hash = SyncPackage::digest(bytes);
    package.objects.insert(hash, bytes);
    return hash;
}

bool extractStrings(QJsonValue &value, QJsonArray &blobs, const QJsonArray &path,
                    SyncPackage::Package &package, int depth)
{
    if (depth > maxDepth || package.objects.size() > maxObjects)
        return false;
    if (value.isString()) {
        const QString string = value.toString();
        if (string.size() >= stringThreshold) {
            const QString hash = addObject(package, string.toUtf8());
            blobs.append(QJsonObject{{QStringLiteral("path"), path}, {QStringLiteral("hash"), hash}});
            value = QJsonValue::Null;
        }
    } else if (value.isArray()) {
        QJsonArray array = value.toArray();
        for (qsizetype i = 0; i < array.size(); ++i) {
            QJsonArray childPath = path;
            childPath.append(i);
            QJsonValue child = array.at(i);
            if (!extractStrings(child, blobs, childPath, package, depth + 1))
                return false;
            array.replace(i, child);
        }
        value = array;
    } else if (value.isObject()) {
        QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            QJsonArray childPath = path;
            childPath.append(it.key());
            QJsonValue child = it.value();
            if (!extractStrings(child, blobs, childPath, package, depth + 1))
                return false;
            it.value() = child;
        }
        value = object;
    }
    return true;
}

bool replacePlaceholder(QJsonValue &value, const QJsonArray &path, const QJsonValue &replacement, int at = 0)
{
    if (path.size() > maxDepth)
        return false;
    if (at == path.size()) {
        if (!value.isNull())
            return false;
        value = replacement;
        return true;
    }
    const QJsonValue part = path.at(at);
    if (value.isObject() && part.isString()) {
        QJsonObject object = value.toObject();
        const QString key = part.toString();
        if (!object.contains(key))
            return false;
        QJsonValue child = object.value(key);
        if (!replacePlaceholder(child, path, replacement, at + 1))
            return false;
        object.insert(key, child);
        value = object;
        return true;
    }
    if (value.isArray() && part.isDouble()) {
        const int index = part.toInt(-1);
        QJsonArray array = value.toArray();
        if (index < 0 || index >= array.size() || double(index) != part.toDouble())
            return false;
        QJsonValue child = array.at(index);
        if (!replacePlaceholder(child, path, replacement, at + 1))
            return false;
        array.replace(index, child);
        value = array;
        return true;
    }
    return false;
}

struct Manifest {
    QJsonObject json;
    QMap<QString, qsizetype> sizes;
};

Manifest parseManifest(const QByteArray &bytes, QString *error, qsizetype maximumBytes)
{
    Manifest result;
    if (error)
        error->clear();
    if (maximumBytes < 2 || bytes.size() > SyncPackage::maximumManifestBytes) {
        fail(error, "Manifest exceeds the transfer limit.");
        return {};
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parse);
    const QJsonObject object = document.object();
    const auto entries = object.value(QStringLiteral("objects"));
    const double payloadSize = object.value(QStringLiteral("payloadBytes")).toDouble(-1);
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || object.value(QStringLiteral("format")).toString() != QStringLiteral("quaderno-objects-v1")
        || !validHash(object.value(QStringLiteral("payloadHash")).toString())
        || !validHash(object.value(QStringLiteral("root")).toString())
        || payloadSize < 2 || payloadSize > maximumBytes || std::floor(payloadSize) != payloadSize
        || !entries.isArray() || entries.toArray().isEmpty() || entries.toArray().size() > maxObjects) {
        fail(error, "Invalid object manifest.");
        return {};
    }
    // Descriptor overhead is bounded independently of the original portable JSON.
    const qint64 storageLimit = qint64(maximumBytes) + maxObjects * 512;
    qint64 total = 0;
    for (const QJsonValue &entry : entries.toArray()) {
        const QJsonObject item = entry.toObject();
        const QString hash = item.value(QStringLiteral("hash")).toString();
        const double size = item.value(QStringLiteral("size")).toDouble(-1);
        if (!entry.isObject() || !validHash(hash) || size < 1 || size > storageLimit
            || std::floor(size) != size || result.sizes.contains(hash) || total > storageLimit - qint64(size)) {
            fail(error, "Invalid object size or duplicate hash.");
            return {};
        }
        total += qint64(size);
        result.sizes.insert(hash, qsizetype(size));
    }
    if (!result.sizes.contains(object.value(QStringLiteral("root")).toString())) {
        fail(error, "Missing root object.");
        return {};
    }
    result.json = object;
    return result;
}
}

namespace SyncPackage {
QString digest(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

Package split(const QByteArray &payload, QString *error, qsizetype maximumBytes)
{
    if (error)
        error->clear();
    if (payload.isEmpty() || payload.size() > maximumBytes) {
        fail(error, "Portable document exceeds the transfer limit.");
        return {};
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || !document.object().value(QStringLiteral("books")).isArray()) {
        fail(error, "Invalid portable library.");
        return {};
    }
    Package result;
    QJsonObject root = document.object();
    QJsonArray books = root.value(QStringLiteral("books")).toArray();
    QJsonArray pageRefs;
    for (qsizetype bookIndex = 0; bookIndex < books.size(); ++bookIndex) {
        QJsonObject book = books.at(bookIndex).toObject();
        if (!books.at(bookIndex).isObject() || !book.value(QStringLiteral("pages")).isArray()) {
            fail(error, "Invalid page collection.");
            return {};
        }
        QJsonArray pages = book.value(QStringLiteral("pages")).toArray();
        for (qsizetype pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
            QJsonValue page = pages.at(pageIndex);
            QJsonArray blobs;
            if (!extractStrings(page, blobs, {}, result, 0)) {
                fail(error, "Portable document is too deeply nested.");
                return {};
            }
            const QString hash = addObject(result, jsonBytes({{QStringLiteral("value"), page},
                                                              {QStringLiteral("blobs"), blobs}}));
            pageRefs.append(QJsonObject{{QStringLiteral("path"), QJsonArray{QStringLiteral("books"), bookIndex,
                        QStringLiteral("pages"), pageIndex}}, {QStringLiteral("hash"), hash}});
            pages.replace(pageIndex, QJsonValue::Null);
        }
        book.insert(QStringLiteral("pages"), pages);
        books.replace(bookIndex, book);
    }
    root.insert(QStringLiteral("books"), books);
    QJsonValue rootValue = root;
    QJsonArray rootBlobs;
    if (!extractStrings(rootValue, rootBlobs, {}, result, 0)) {
        fail(error, "Portable document is too deeply nested.");
        return {};
    }
    const QString rootHash = addObject(result, jsonBytes({{QStringLiteral("value"), rootValue},
        {QStringLiteral("pages"), pageRefs}, {QStringLiteral("blobs"), rootBlobs}}));
    QJsonArray objects;
    for (auto it = result.objects.cbegin(); it != result.objects.cend(); ++it)
        objects.append(QJsonObject{{QStringLiteral("hash"), it.key()}, {QStringLiteral("size"), it.value().size()}});
    const QByteArray canonical = document.toJson(QJsonDocument::Compact);
    result.manifest = jsonBytes({{QStringLiteral("format"), QStringLiteral("quaderno-objects-v1")},
        {QStringLiteral("root"), rootHash}, {QStringLiteral("objects"), objects},
        {QStringLiteral("payloadHash"), digest(canonical)}, {QStringLiteral("payloadBytes"), canonical.size()}});
    if (objectSizes(result.manifest, error, maximumBytes).isEmpty())
        return {};
    return result;
}

QMap<QString, qsizetype> objectSizes(const QByteArray &manifest, QString *error, qsizetype maximumBytes)
{
    return parseManifest(manifest, error, maximumBytes).sizes;
}

QByteArray assemble(const QByteArray &manifest, const std::function<QByteArray(const QString &)> &load,
                    QString *error, qsizetype maximumBytes)
{
    const Manifest parsed = parseManifest(manifest, error, maximumBytes);
    if (parsed.sizes.isEmpty())
        return {};
    qint64 expandedBytes = 0;
    const auto object = [&](const QString &hash) -> QByteArray {
        if (!parsed.sizes.contains(hash))
            return {};
        const QByteArray bytes = load(hash);
        if (bytes.size() != parsed.sizes.value(hash) || digest(bytes) != hash)
            return {};
        return bytes;
    };
    const auto restoreBlobs = [&](QJsonValue &value, const QJsonArray &blobs) {
        if (blobs.size() > maxObjects)
            return false;
        for (const auto &entry : blobs) {
            const QJsonObject ref = entry.toObject();
            const QByteArray bytes = object(ref.value(QStringLiteral("hash")).toString());
            if (bytes.isEmpty() || !ref.value(QStringLiteral("path")).isArray()
                || expandedBytes > qint64(maximumBytes) + maxObjects * 512 - bytes.size())
                return false;
            expandedBytes += bytes.size();
            const QString string = QString::fromUtf8(bytes);
            if (string.toUtf8() != bytes || !replacePlaceholder(value, ref.value(QStringLiteral("path")).toArray(), string))
                return false;
        }
        return true;
    };
    const QByteArray rootBytes = object(parsed.json.value(QStringLiteral("root")).toString());
    const QJsonObject root = QJsonDocument::fromJson(rootBytes).object();
    QJsonValue value = root.value(QStringLiteral("value"));
    expandedBytes = rootBytes.size();
    if (rootBytes.isEmpty() || !value.isObject() || !boundedDepth(value) || !root.value(QStringLiteral("pages")).isArray()
        || !root.value(QStringLiteral("blobs")).isArray()
        || !restoreBlobs(value, root.value(QStringLiteral("blobs")).toArray())) {
        fail(error, "Root object or asset is missing or corrupt.");
        return {};
    }
    const QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
    if (pages.size() > maxObjects) {
        fail(error, "Too many pages in manifest.");
        return {};
    }
    for (const QJsonValue &entry : pages) {
        const QJsonObject ref = entry.toObject();
        const QByteArray pageBytes = object(ref.value(QStringLiteral("hash")).toString());
        const QJsonObject page = QJsonDocument::fromJson(pageBytes).object();
        QJsonValue content = page.value(QStringLiteral("value"));
        // Descriptor metadata is not part of the output, so only count content here.
        const qsizetype contentBytes = jsonBytes({{QStringLiteral("v"), content}}).size();
        if (pageBytes.isEmpty() || !ref.value(QStringLiteral("path")).isArray()
            || !page.value(QStringLiteral("blobs")).isArray() || content.isUndefined() || !boundedDepth(content)
            || expandedBytes > qint64(maximumBytes) + maxObjects * 512 - contentBytes) {
            fail(error, "Page object is missing or corrupt.");
            return {};
        }
        expandedBytes += contentBytes;
        if (!restoreBlobs(content, page.value(QStringLiteral("blobs")).toArray())
            || !replacePlaceholder(value, ref.value(QStringLiteral("path")).toArray(), content)) {
            fail(error, "Invalid page or asset reference.");
            return {};
        }
    }
    const QByteArray result = QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    if (result.size() != parsed.json.value(QStringLiteral("payloadBytes")).toInteger()
        || result.size() > maximumBytes || digest(result) != parsed.json.value(QStringLiteral("payloadHash")).toString()) {
        fail(error, "Reconstructed document does not match its manifest.");
        return {};
    }
    return result;
}
}
