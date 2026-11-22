#pragma once
#include <QByteArray>
#include <QString>

// A single atomic manifest selects immutable page/asset objects. The previous
// manifest remains readable across a crash; portable exports remain plain JSON.
namespace PageStore {
inline constexpr qsizetype maximumLibraryBytes = 256 * 1024 * 1024;
struct ReadResult {
    QByteArray payload;
    QByteArray currentBytes;
    QString error;
    bool recovered = false;
};
ReadResult read(const QString &path);
// A best-effort consistent snapshot without the live writer lock. Objects are
// immutable and digest checked; concurrent garbage collection fails the backup
// instead of preventing a notebook edit from being saved.
ReadResult readForBackup(const QString &path);
bool write(const QString &path, const QByteArray &payload, const QByteArray &expected,
           QByteArray *manifest, QString *error, bool recovering = false);
bool isManifest(const QByteArray &bytes);
}

#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QVariantMap>

namespace PageStore {
// Keep implicitly shared strings alive so repeated edits can reuse the digest
// without scanning immutable document/image bytes again.
struct BlobCache {
    struct Entry { QString text; QString hash; qsizetype bytes = 0; };
    QHash<quintptr, Entry> entries;
};
struct Update {
    QByteArray manifest;
    QHash<QString, QByteArray> changedObjects;
};
class Builder {
public:
    Builder(const QByteArray &previousManifest, BlobCache *cache);
    QString reusePage(const QString &bookId, const QString &pageId);
    QString addPage(const QVariant &page, QString *error);
    bool finish(const QVariantMap &metadata, Update *update, QString *error);
    qsizetype encodedBlobBytes() const { return m_encodedBlobBytes; }
private:
    bool encode(const QVariant &value, QJsonValue *json, QJsonArray *blobs,
                const QJsonArray &path, QString *error, int depth = 0);
    QString addBlob(const QString &text);
    QString addObject(const QByteArray &bytes, const QJsonArray &blobs = {});
    bool retain(const QString &hash);
    BlobCache *m_cache;
    QJsonObject m_previousRoot;
    QHash<QString, QString> m_previousPageRefs;
    QMap<QString, QJsonObject> m_previousObjects;
    QMap<QString, QJsonObject> m_objects;
    QHash<QString, QByteArray> m_changedObjects;
    qsizetype m_encodedBlobBytes = 0;
};
bool writeUpdate(const QString &path, const Update &update, const QByteArray &expected,
                 QByteArray *manifest, QString *error, bool recovering = false);
}
