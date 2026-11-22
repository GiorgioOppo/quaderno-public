#include "documentreader.h"
#include "epubimport.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QRegularExpression>
#include <QCache>
#include <QDateTime>
#include <QDirIterator>
#include <QMutex>
#include <QMutexLocker>
#include <algorithm>

namespace {
QMutex cacheMutex;
QMutex sourceMutex;
QCache<QString, QImage> pageCache(64 * 1024);
std::atomic_int cacheLimit{64};
quint64 cacheGeneration = 0;
constexpr qint64 maximumSource = 48 * 1024 * 1024;

QFileInfoList cachedSources(const QString &directory, QString *error)
{
    QFileInfoList files;
    const QFileInfo root(directory);
    if (!root.exists()) return files;
    if (root.isSymLink() || !root.isDir()) {
        if (error) *error = QStringLiteral("The document cache is not a regular directory.");
        return files;
    }
    static const QRegularExpression name(QStringLiteral("^[a-f0-9]{64}\\.pdf$"));
    QDirIterator entries(directory, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    int inspected = 0;
    while (entries.hasNext()) {
        entries.next();
        if (++inspected > 10000) {
            if (error) *error = QStringLiteral("Too many files in the document cache.");
            return {};
        }
        const auto file = entries.fileInfo();
        if (file.isFile() && !file.isSymLink() && name.match(file.fileName()).hasMatch()
            && file.size() > 0 && file.size() <= maximumSource) files.append(file);
    }
    return files;
}

bool verifiedSource(const QFileInfo &info)
{
    QFile file(info.absoluteFilePath());
    if (info.isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() != info.size()) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 bytes = 0;
    while (!file.atEnd() && bytes <= maximumSource) {
        const QByteArray block = file.read(64 * 1024);
        if (block.isEmpty() && file.error() != QFileDevice::NoError) return false;
        bytes += block.size();
        hash.addData(block);
    }
    return bytes == info.size() && hash.result().toHex() == info.completeBaseName().toLatin1();
}

void trimSources(const QString &directory, const QString &retained)
{
    QString error;
    auto files = cachedSources(directory, &error);
    if (!error.isEmpty()) return;
    qint64 total = 0;
    for (const auto &file : files) total += file.size();
    const qint64 budget = qint64(cacheLimit) * 1024 * 1024;
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() < b.lastModified();
    });
    qint64 inspectedBytes = 0;
    for (const auto &file : files) {
        if (total <= budget) break;
        if (inspectedBytes + file.size() > 256 * 1024 * 1024) break;
        inspectedBytes += file.size();
        if ((file.absoluteFilePath() != retained || file.size() > budget)
            && verifiedSource(file) && QFile::remove(file.absoluteFilePath()))
            total -= file.size();
    }
}
}

int DocumentCache::limitMiB() { const QMutexLocker lock(&cacheMutex); return cacheLimit; }
void DocumentCache::setLimitMiB(int value)
{
    if (value != 32 && value != 64 && value != 128 && value != 256) return;
    const QMutexLocker lock(&cacheMutex);
    cacheLimit = value;
    pageCache.setMaxCost(value * 1024);
}
quint64 DocumentCache::generation() { const QMutexLocker lock(&cacheMutex); return cacheGeneration; }
QImage DocumentCache::image(const QString &key)
{
    const QMutexLocker lock(&cacheMutex);
    const auto *cached = pageCache.object(key);
    return cached ? *cached : QImage{};
}
void DocumentCache::insertImage(const QString &key, const QImage &image, quint64 generation)
{
    const QMutexLocker lock(&cacheMutex);
    if (generation == cacheGeneration && !image.isNull())
        pageCache.insert(key, new QImage(image), qMax<qsizetype>(1, (image.sizeInBytes() + 1023) / 1024));
}
QVariantMap DocumentCache::memoryStatus()
{
    const QMutexLocker lock(&cacheMutex);
    return {{QStringLiteral("memoryCacheBytes"), qint64(pageCache.totalCost()) * 1024},
            {QStringLiteral("memoryCacheLimitBytes"), qint64(cacheLimit) * 1024 * 1024},
            {QStringLiteral("cachedPages"), pageCache.size()}};
}
void DocumentCache::clearMemory()
{
    const QMutexLocker lock(&cacheMutex);
    ++cacheGeneration;
    pageCache.clear();
}
QVariantMap DocumentCache::sourceStatus(const QString &directory, QString *error)
{
    const QMutexLocker lock(&sourceMutex);
    qint64 bytes = 0;
    const auto files = cachedSources(directory, error);
    for (const auto &file : files) bytes += file.size();
    return {{QStringLiteral("cacheBytes"), bytes}, {QStringLiteral("cachedSources"), files.size()}};
}
bool DocumentCache::clearSources(const QString &directory, QString *error)
{
    const QMutexLocker lock(&sourceMutex);
    const auto files = cachedSources(directory, error);
    if (error && !error->isEmpty()) return false;
    qint64 inspectedBytes = 0;
    for (const auto &file : files) {
        if (inspectedBytes + file.size() > 512 * 1024 * 1024) {
            if (error) *error = QCoreApplication::translate("LibraryMaintenance", "Una parte della cache è stata pulita. Ripeti l’operazione per completarla.");
            return false;
        }
        inspectedBytes += file.size();
        if (!verifiedSource(file)) continue;
        if (!QFile::remove(file.absoluteFilePath())) {
            if (error) *error = QStringLiteral("Cannot remove a document cache file.");
            return false;
        }
    }
    return true;
}

PreparedDocument prepareDocument(const QString &path, const DocumentCancellation &cancel,
                                 const DocumentProgress &progress)
{
    PreparedDocument result;
    const QString kind = QFileInfo(path).suffix().toLower();
    QByteArray original;
    if (kind == QStringLiteral("epub")) {
        original = epubSourceBytes(path, result.error);
    } else if (kind == QStringLiteral("pdf")) {
        QFile input(path);
        if (!input.open(QIODevice::ReadOnly) || input.size() < 1 || input.size() > 48 * 1024 * 1024) {
            result.error = QCoreApplication::translate("PdfImport", "Scegli un PDF leggibile da meno di 48 MB.");
            return result;
        }
        original = input.read(48 * 1024 * 1024 + 1);
        if (original.size() > 48 * 1024 * 1024 || input.error() != QFileDevice::NoError) {
            result.error = QCoreApplication::translate("PdfImport", "Impossibile leggere il PDF.");
            return result;
        }
    } else {
        result.error = QCoreApplication::translate("Notebook", "Il formato deve essere PDF, EPUB o Markdown.");
        return result;
    }
    if (original.isEmpty() || (cancel && cancel->load()))
        return result;
    const QString originalId = QString::fromLatin1(QCryptographicHash::hash(original, QCryptographicHash::Sha256).toHex());
    result.sources.insert(originalId, QVariantMap{{QStringLiteral("kind"), kind},
                                                {QStringLiteral("data"), QString::fromLatin1(original.toBase64())}});
    const QByteArray pdf = kind == QStringLiteral("pdf") ? original : convertEpubToPdf(original, result.error, cancel,
        [progress](int done, int total) { if (progress) progress(done, qMax(1, total) * 2); });
    if (pdf.isEmpty() || (cancel && cancel->load()))
        return result;
    const QString renderId = QString::fromLatin1(QCryptographicHash::hash(pdf, QCryptographicHash::Sha256).toHex());
    result.sources.insert(renderId, QVariantMap{{QStringLiteral("kind"), QStringLiteral("pdf")},
                                              {QStringLiteral("data"), QString::fromLatin1(pdf.toBase64())}});
    const auto index = inspectPdfData(pdf, result.error, cancel, [progress, kind](int done, int total) {
        if (progress) progress(kind == QStringLiteral("epub") ? total + done : done,
                               kind == QStringLiteral("epub") ? total * 2 : total);
    });
    if (!result.error.isEmpty() || (cancel && cancel->load()))
        return result;
    result.pages.reserve(index.pageCount);
    for (int page = 0; page < index.pageCount; ++page) {
        QVariantMap source{{QStringLiteral("kind"), QStringLiteral("pdf")}, {QStringLiteral("asset"), renderId},
                           {QStringLiteral("page"), page}};
        if (originalId != renderId)
            source.insert(QStringLiteral("origin"), originalId);
        result.pages.append(QVariantMap{{QStringLiteral("source"), source},
                                         {QStringLiteral("text"), index.pageText.value(page)}});
    }
    return result;
}

QImage renderDocumentPage(const QVariantMap &source, int maximumEdge, QString &error)
{
    error.clear();
    const QString id = source.value(QStringLiteral("asset")).toString();
    static const QRegularExpression digest(QStringLiteral("^[a-f0-9]{64}$"));
    if (!digest.match(id).hasMatch() || source.value(QStringLiteral("kind")) != QStringLiteral("pdf")
        || source.value(QStringLiteral("page"), -1).toInt() < 0) {
        error = QCoreApplication::translate("PdfImport", "Sorgente della pagina non valida.");
        return {};
    }
    // The descriptor is portable; cache paths are supplied only by Notebook.
    // All decoding and cache writes happen on the rendering worker.
    const QString directory = source.value(QStringLiteral("cacheDirectory")).toString();
    const QString path = directory.isEmpty() ? QString{} : QDir(directory).filePath(id + QStringLiteral(".pdf"));
    QByteArray data;
    QMutexLocker sourceLock(&sourceMutex);
    const bool safeDirectory = !directory.isEmpty() && !QFileInfo(directory).isSymLink();
    if (safeDirectory && !QFileInfo(path).isSymLink()) {
        QFile cached(path);
        if (cached.open(QIODevice::ReadOnly) && cached.size() > 0 && cached.size() <= maximumSource) {
            data = cached.read(maximumSource + 1);
            if (cached.error() != QFileDevice::NoError || data.size() > maximumSource) data.clear();
        }
    }
    if (data.isEmpty() || QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) != id) {
        const QByteArray encoded = source.value(QStringLiteral("data")).toString().toLatin1();
        if (encoded.size() > 64 * 1024 * 1024) {
            error = QCoreApplication::translate("PdfImport", "La sorgente PDF supera il limite di dimensione.");
            return {};
        }
        data = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
        if (data.isEmpty() || data.size() > 48 * 1024 * 1024
            || QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) != id) {
            error = QCoreApplication::translate("PdfImport", "La sorgente della pagina è mancante o danneggiata.");
            return {};
        }
        if (safeDirectory && data.size() <= qint64(cacheLimit) * 1024 * 1024
            && !QFileInfo(path).isSymLink() && QDir().mkpath(directory)) {
            QSaveFile cached(path);
            if (cached.open(QIODevice::WriteOnly) && cached.write(data) == data.size())
                cached.commit();
        }
    }
    if (safeDirectory) trimSources(directory, path);
    sourceLock.unlock();
    return renderPdfPage(data, source.value(QStringLiteral("page")).toInt(), maximumEdge, error);
}

#include <QThreadPool>
#include <QObject>

namespace {
class ImportJob final : public QObject
{
    Q_OBJECT
public:
    void run(const QString &path, const DocumentCancellation &cancel)
    {
        const auto result = prepareDocument(path, cancel, [this, cancel](int done, int total) {
            if (!cancel->load())
                emit advanced(done, total);
        });
        emit ready(result);
        deleteLater();
    }
signals:
    void advanced(int done, int total);
    void ready(const PreparedDocument &result);
};
}

DocumentCancellation startDocumentPreparation(const QString &path, QObject *context,
                                               const DocumentProgress &progress,
                                               std::function<void(PreparedDocument)> finished)
{
    qRegisterMetaType<PreparedDocument>();
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    auto *job = new ImportJob;
    QObject::connect(job, &ImportJob::advanced, context, progress, Qt::QueuedConnection);
    QObject::connect(job, &ImportJob::ready, context, std::move(finished), Qt::QueuedConnection);
    QThreadPool::globalInstance()->start([job, path, cancel] { job->run(path, cancel); });
    return cancel;
}

#include "documentreader.moc"
