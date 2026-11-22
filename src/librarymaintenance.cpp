#include "librarymaintenance.h"
#include "documentreader.h"
#include "pagestore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QUuid>
#include <algorithm>
#include <functional>

namespace {
constexpr qint64 maximumMetadata = 16 * 1024;
constexpr qint64 maximumReaderSettings = 1024 * 1024;
const auto ownerFile = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
QString digest(const QByteArray &bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
QString libraryKey(const QString &path) { return digest(QFileInfo(path).absoluteFilePath().toUtf8()); }

QByteArray readBounded(const QString &path, qint64 maximum, QString *error)
{
    QFile file(path);
    if (QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > maximum) {
        *error = LibraryMaintenance::tr("Impossibile leggere un file del backup.");
        return {};
    }
    const QByteArray bytes = file.read(maximum + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() > maximum) {
        *error = LibraryMaintenance::tr("Il file del backup supera il limite consentito.");
        return {};
    }
    return bytes;
}

bool atomicFile(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(ownerFile)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = LibraryMaintenance::tr("Impossibile salvare il backup locale. %1").arg(file.errorString());
        return false;
    }
    return true;
}

struct BackupEntry {
    QString id, directory, path, created, hash, settingsHash;
    qint64 bytes = 0, settingsBytes = 0;
};

QList<BackupEntry> backupEntries(const QString &path, QString *error)
{
    QList<BackupEntry> result;
    const QString rootPath = path + QStringLiteral(".backups");
    const QFileInfo root(rootPath);
    if (!root.exists()) return result;
    if (!root.isDir() || root.isSymLink()) {
        *error = LibraryMaintenance::tr("La cartella dei backup non è una cartella locale valida.");
        return result;
    }
    static const QRegularExpression pattern(QStringLiteral("^snapshot-([a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12})$"));
    QDirIterator entries(rootPath, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    int count = 0;
    while (entries.hasNext()) {
        entries.next();
        if (++count > 1000) {
            *error = LibraryMaintenance::tr("Troppi file nella cartella dei backup. Controllala prima di proseguire.");
            return {};
        }
        const auto info = entries.fileInfo();
        const auto match = pattern.match(info.fileName());
        if (!info.isDir() || info.isSymLink() || !match.hasMatch()) continue;
        QString metadataError;
        const auto metadata = QJsonDocument::fromJson(readBounded(QDir(info.absoluteFilePath()).filePath(QStringLiteral("metadata.json")), maximumMetadata, &metadataError)).object();
        if (!metadataError.isEmpty() || metadata.value(QStringLiteral("format")) != QStringLiteral("quaderno-local-backup-v1")
            || metadata.value(QStringLiteral("libraryKey")) != libraryKey(path)
            || metadata.value(QStringLiteral("id")).toString() != match.captured(1)) continue;
        BackupEntry entry;
        entry.id = match.captured(1);
        entry.directory = info.absoluteFilePath();
        entry.path = QDir(entry.directory).filePath(QStringLiteral("library.quaderno"));
        entry.created = metadata.value(QStringLiteral("created")).toString();
        entry.hash = metadata.value(QStringLiteral("sha256")).toString();
        entry.bytes = metadata.value(QStringLiteral("bytes")).toInteger(-1);
        entry.settingsHash = metadata.value(QStringLiteral("readerSettingsSha256")).toString();
        entry.settingsBytes = metadata.value(QStringLiteral("readerSettingsBytes")).toInteger(0);
        const QFileInfo data(entry.path);
        if (!QDateTime::fromString(entry.created, Qt::ISODateWithMs).isValid() || entry.hash.size() != 64
            || entry.bytes < 1 || entry.bytes > PageStore::maximumLibraryBytes || data.isSymLink()
            || !data.isFile() || data.size() != entry.bytes || entry.settingsBytes < 0
            || entry.settingsBytes > maximumReaderSettings) continue;
        result.append(entry);
    }
    std::sort(result.begin(), result.end(), [](const BackupEntry &a, const BackupEntry &b) {
        return a.created == b.created ? a.id > b.id : a.created > b.created;
    });
    return result;
}

bool verifiedBackup(const BackupEntry &entry)
{
    // Never remove a directory with extra files, symlinks, or altered content.
    const QStringList expected = entry.settingsBytes > 0
        ? QStringList{QStringLiteral("library.quaderno"), QStringLiteral("metadata.json"), QStringLiteral("reader-settings.json")}
        : QStringList{QStringLiteral("library.quaderno"), QStringLiteral("metadata.json")};
    const QDir directory(entry.directory);
    const auto files = directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    if (QFileInfo(entry.directory).isSymLink() || files.size() != expected.size()) return false;
    for (const auto &file : files)
        if (file.isSymLink() || !file.isFile() || !expected.contains(file.fileName())) return false;
    QString error;
    const QByteArray payload = readBounded(entry.path, PageStore::maximumLibraryBytes, &error);
    if (!error.isEmpty() || digest(payload) != entry.hash || !Notebook::validateBackupData(payload).isEmpty()) return false;
    if (entry.settingsBytes > 0) {
        const QByteArray settings = readBounded(directory.filePath(QStringLiteral("reader-settings.json")), maximumReaderSettings, &error);
        if (!error.isEmpty() || settings.size() != entry.settingsBytes || digest(settings) != entry.settingsHash) return false;
    }
    return true;
}

qint64 objectBytes(const QString &directory, QString *error)
{
    if (!QFileInfo::exists(directory)) return 0;
    if (QFileInfo(directory).isSymLink() || !QFileInfo(directory).isDir()) {
        *error = LibraryMaintenance::tr("L’archivio degli originali non è una cartella valida.");
        return 0;
    }
    qint64 bytes = 0;
    int inspected = 0;
    QDirIterator iterator(directory, QDir::Files | QDir::NoSymLinks);
    while (iterator.hasNext()) {
        iterator.next();
        if (++inspected > 100000) { *error = LibraryMaintenance::tr("L’archivio contiene troppi file."); return 0; }
        bytes += iterator.fileInfo().size();
    }
    return bytes;
}

QVariantMap inspect(const QString &path, const QString &cacheDirectory)
{
    QVariantMap result, storage = DocumentCache::memoryStatus();
    QString error;
    storage.insert(QStringLiteral("libraryBytes"), QFileInfo(path).size());
    storage.insert(QStringLiteral("objectBytes"), objectBytes(path + QStringLiteral(".objects"), &error));
    const auto cache = DocumentCache::sourceStatus(cacheDirectory, &error);
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) storage.insert(it.key(), it.value());
    const auto entries = backupEntries(path, &error);
    qint64 backupBytes = 0;
    QVariantList backups;
    for (const auto &entry : entries) {
        backupBytes += entry.bytes + entry.settingsBytes;
        backups.append(QVariantMap{{QStringLiteral("id"), entry.id}, {QStringLiteral("path"), entry.path},
            {QStringLiteral("created"), entry.created}, {QStringLiteral("bytes"), entry.bytes + entry.settingsBytes}});
    }
    storage.insert(QStringLiteral("backupBytes"), backupBytes);
    storage.insert(QStringLiteral("backupCount"), backups.size());
    const QStorageInfo volume(QFileInfo(path).absolutePath());
    storage.insert(QStringLiteral("freeBytes"), volume.isValid() ? volume.bytesAvailable() : -1);
    result.insert(QStringLiteral("storage"), storage);
    result.insert(QStringLiteral("backups"), backups);
    if (!entries.isEmpty()) {
        result.insert(QStringLiteral("lastBackupPath"), entries.first().path);
        result.insert(QStringLiteral("lastBackupAt"), entries.first().created);
    }
    if (!error.isEmpty()) result.insert(QStringLiteral("error"), error);
    return result;
}

QString createBackup(const QString &path, int retention)
{
    QString error;
    const auto snapshot = PageStore::readForBackup(path);
    if (!snapshot.error.isEmpty()) return snapshot.error;
    if (const auto invalid = Notebook::validateBackupData(snapshot.payload); !invalid.isEmpty()) return invalid;
    const QString rootPath = path + QStringLiteral(".backups");
    if (QFileInfo(rootPath).isSymLink() || !QDir().mkpath(rootPath))
        return LibraryMaintenance::tr("Impossibile creare la cartella dei backup locali.");
    if (!QFile::setPermissions(rootPath, ownerFile | QFileDevice::ExeOwner))
        return LibraryMaintenance::tr("Impossibile proteggere la cartella dei backup.");
    QByteArray settings;
    const QString settingsPath = path + QStringLiteral(".reader-settings.json");
    if (QFileInfo::exists(settingsPath)) {
        settings = readBounded(settingsPath, maximumReaderSettings, &error);
        if (!error.isEmpty()) return error;
        if (QJsonDocument::fromJson(settings).object().value(QStringLiteral("format")) != QStringLiteral("quaderno-reader-settings-v1"))
            return LibraryMaintenance::tr("Le preferenze di lettura non sono valide; il backup precedente viene conservato.");
    }
    const QStorageInfo volume(rootPath);
    if (volume.isValid() && volume.bytesAvailable() >= 0
        && volume.bytesAvailable() < snapshot.payload.size() + settings.size() + 1024 * 1024)
        return LibraryMaintenance::tr("Spazio insufficiente per il backup locale.");
    QTemporaryDir pending(QDir(rootPath).filePath(QStringLiteral(".pending-XXXXXX")));
    if (!pending.isValid()) return LibraryMaintenance::tr("Impossibile preparare il backup locale.");
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject metadata{{QStringLiteral("format"), QStringLiteral("quaderno-local-backup-v1")},
        {QStringLiteral("id"), id}, {QStringLiteral("libraryKey"), libraryKey(path)},
        {QStringLiteral("created"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("bytes"), snapshot.payload.size()}, {QStringLiteral("sha256"), digest(snapshot.payload)},
        {QStringLiteral("readerSettingsBytes"), settings.size()}, {QStringLiteral("readerSettingsSha256"), settings.isEmpty() ? QString{} : digest(settings)}};
    if (!atomicFile(pending.filePath(QStringLiteral("library.quaderno")), snapshot.payload, &error)
        || (!settings.isEmpty() && !atomicFile(pending.filePath(QStringLiteral("reader-settings.json")), settings, &error))
        || !atomicFile(pending.filePath(QStringLiteral("metadata.json")), QJsonDocument(metadata).toJson(QJsonDocument::Compact), &error)) return error;
    const QString destination = QDir(rootPath).filePath(QStringLiteral("snapshot-") + id);
    if (!QDir().rename(pending.path(), destination)) return LibraryMaintenance::tr("Impossibile completare il backup locale.");
    pending.setAutoRemove(false);
    const auto entries = backupEntries(path, &error);
    if (!error.isEmpty()) return error;
    // Retention is applied only after a successful publication and an explicit
    // retention binding/setting. Modified or foreign directories remain intact.
    qint64 inspectedBytes = 0;
    for (qsizetype index = retention > 0 ? retention : entries.size(); index < entries.size(); ++index) {
        const auto &entry = entries.at(index);
        if (inspectedBytes + entry.bytes > 512 * 1024 * 1024) break;
        inspectedBytes += entry.bytes;
        if (entry.id == id || !verifiedBackup(entry)) continue;
        if (!QFile::remove(entry.path)) continue;
        if (entry.settingsBytes > 0) QFile::remove(QDir(entry.directory).filePath(QStringLiteral("reader-settings.json")));
        QFile::remove(QDir(entry.directory).filePath(QStringLiteral("metadata.json")));
        QDir().rmdir(entry.directory);
    }
    return {};
}

class MaintenanceJob final : public QObject
{
    Q_OBJECT
public:
    void run(const std::function<QVariantMap()> &operation) { emit ready(operation()); deleteLater(); }
signals:
    void ready(const QVariantMap &result);
};
}

LibraryMaintenance::LibraryMaintenance(QObject *parent) : QObject(parent)
{
    m_scheduleTimer.setInterval(60000);
    connect(&m_scheduleTimer, &QTimer::timeout, this, &LibraryMaintenance::checkSchedule);
    m_scheduleTimer.start();
}
void LibraryMaintenance::setNotebook(Notebook *notebook)
{
    if (m_notebook == notebook || m_busy) return;
    m_notebook = notebook;
    m_storage.clear(); m_backups.clear(); m_lastBackupPath.clear(); m_lastBackupAt = {};
    emit changed();
    if (m_notebook) refreshStatus();
}
void LibraryMaintenance::setAllowed(bool value) { if (m_allowed != value) { m_allowed = value; emit changed(); } }
void LibraryMaintenance::setBackupSchedule(const QString &value)
{
    if (value == m_backupSchedule || !QStringList{QStringLiteral("manual"), QStringLiteral("daily"), QStringLiteral("weekly")}.contains(value)) return;
    m_backupSchedule = value; m_nextAutoAttempt = {}; emit changed();
}
void LibraryMaintenance::setBackupRetention(int value)
{
    if (value != 1 && value != 5 && value != 10 && value != 20) return;
    m_retentionConfigured = true;
    if (m_backupRetention != value) { m_backupRetention = value; emit changed(); }
}
void LibraryMaintenance::setCacheLimitMiB(int value)
{
    if (value != 32 && value != 64 && value != 128 && value != 256) return;
    DocumentCache::setLimitMiB(value);
    if (value != m_cacheLimitMiB) { m_cacheLimitMiB = value; emit changed(); }
}
bool LibraryMaintenance::backup() { return start(QStringLiteral("backup")); }
bool LibraryMaintenance::clearCache() { return start(QStringLiteral("clearCache")); }
bool LibraryMaintenance::refreshStatus() { return start(QStringLiteral("inspect")); }
void LibraryMaintenance::checkSchedule()
{
    const auto now = QDateTime::currentDateTimeUtc();
    if (!m_allowed || m_busy || !m_notebook || m_notebook->importing() || m_backupSchedule == QStringLiteral("manual")
        || (m_nextAutoAttempt.isValid() && now < m_nextAutoAttempt)) return;
    const qint64 interval = m_backupSchedule == QStringLiteral("weekly") ? 7 * 86400 : 86400;
    if (m_lastBackupAt.isValid() && m_lastBackupAt.secsTo(now) < interval) return;
    m_nextAutoAttempt = now.addSecs(3600);
    backup();
}
bool LibraryMaintenance::start(const QString &operation)
{
    if (m_busy || !m_notebook || (operation != QStringLiteral("inspect") && (!m_allowed || m_notebook->importing()))) return false;
    const QString path = m_notebook->storagePath(), cache = m_notebook->sourceCacheDirectory();
    const int retention = m_retentionConfigured ? m_backupRetention : 0;
    m_busy = true; m_error.clear();
    m_status = operation == QStringLiteral("backup") ? tr("Backup locale in corso…")
        : operation == QStringLiteral("clearCache") ? tr("Pulizia della cache in corso…") : tr("Controllo dello spazio locale…");
    emit changed();
    auto *job = new MaintenanceJob;
    connect(job, &MaintenanceJob::ready, this, [this, operation](const QVariantMap &result) {
        m_storage = result.value(QStringLiteral("storage")).toMap();
        m_backups = result.value(QStringLiteral("backups")).toList();
        m_lastBackupPath = result.value(QStringLiteral("lastBackupPath")).toString();
        m_lastBackupAt = QDateTime::fromString(result.value(QStringLiteral("lastBackupAt")).toString(), Qt::ISODateWithMs);
        m_error = result.value(QStringLiteral("error")).toString();
        m_status = !m_error.isEmpty() ? tr("Operazione non completata; gli originali sono conservati.")
            : operation == QStringLiteral("backup") ? tr("Backup salvato su questo dispositivo.")
            : operation == QStringLiteral("clearCache") ? tr("Cache ricreabile eliminata. Originali e annotazioni conservati.")
            : tr("Spazio locale aggiornato.");
        m_busy = false;
        emit changed(); emit finished(m_error.isEmpty());
    }, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start([job, path, cache, operation, retention] {
        job->run([path, cache, operation, retention] {
            QLockFile lock(path + QStringLiteral(".maintenance.lock"));
            if (!lock.tryLock(0)) return QVariantMap{{QStringLiteral("error"), LibraryMaintenance::tr("La manutenzione è già in corso. Riprova.")}};
            QString error;
            if (operation == QStringLiteral("backup")) error = createBackup(path, retention);
            else if (operation == QStringLiteral("clearCache")) {
                DocumentCache::clearMemory();
                DocumentCache::clearSources(cache, &error);
            }
            auto result = inspect(path, cache);
            if (!error.isEmpty()) result.insert(QStringLiteral("error"), error);
            return result;
        });
    });
    return true;
}

#include "librarymaintenance.moc"
