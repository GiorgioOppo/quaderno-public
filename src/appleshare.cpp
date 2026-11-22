#include "appleshare.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QStandardPaths>
#include <QLockFile>
#include <QSet>
#include <QUuid>

#ifdef Q_OS_MACOS
#include <functional>

namespace appleShareMac {
using FileSelection = std::function<void(const QString &)>;
void choosePdfDestination(const QString &fileName, FileSelection selected);
void chooseNotebookFile(FileSelection selected);
bool sharePdf(const QString &path);
void openExport(const QString &path, const QString &destination, std::function<void(bool)> completed);
void revealExport(const QString &path);
}
#endif

AppleShare::AppleShare(bool tablet, QObject *parent, const QString &deliveryDirectory) : QObject(parent), m_tablet(tablet)
{
    m_deliveryDirectory = deliveryDirectory.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("apple-deliveries"))
        : deliveryDirectory;
    if (available()) loadDeliveries();
}

AppleShare::~AppleShare() = default;

bool AppleShare::available() const
{
#ifdef Q_OS_MACOS
    return !m_tablet;
#else
    return false;
#endif
}

void AppleShare::setState(bool busy, const QString &status)
{
    if (m_busy == busy && m_status == status)
        return;
    m_busy = busy;
    m_status = status;
    emit changed();
}

void AppleShare::loadDeliveries()
{
    QFile file(QDir(m_deliveryDirectory).filePath(QStringLiteral("queue.json")));
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        m_deliveryLoadFailed = true;
    } else {
        m_deliveryRevision = file.readAll();
        const auto document = QJsonDocument::fromJson(m_deliveryRevision);
        if (!document.isArray() || document.array().size() > 256) {
            m_deliveryLoadFailed = true;
        } else {
            QSet<QString> ids;
            for (const auto &value : document.array()) {
                const auto job = value.toObject().toVariantMap();
                const QString id = job.value(QStringLiteral("id")).toString();
                const QString filename = job.value(QStringLiteral("file")).toString();
                const QString state = job.value(QStringLiteral("state")).toString();
                if (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id
                    || ids.contains(id) || filename.isEmpty() || QFileInfo(filename).fileName() != filename
                    || !QStringList{QStringLiteral("prepared"), QStringLiteral("awaitingConfirmation"),
                        QStringLiteral("completed"), QStringLiteral("failed"), QStringLiteral("cancelled")}.contains(state)) {
                    m_deliveryLoadFailed = true;
                    break;
                }
                ids.insert(id);
                m_deliveries.append(job);
            }
        }
    }
    if (m_deliveryLoadFailed) {
        m_deliveries.clear();
        m_status = tr("La coda Apple non è leggibile. I file esportati sono conservati.");
    }
}

void AppleShare::sharePdf(Notebook *notebook)
{
    prepareDelivery(notebook, QStringLiteral("share"));
}

bool AppleShare::saveDeliveries(const QVariantList &deliveries)
{
    if (m_deliveryLoadFailed || !QDir().mkpath(m_deliveryDirectory)) return false;
    const QString path = QDir(m_deliveryDirectory).filePath(QStringLiteral("queue.json"));
    QLockFile lock(path + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) return false;
    QFile previous(path);
    if (previous.exists() && (!previous.open(QIODevice::ReadOnly) || previous.readAll() != m_deliveryRevision))
        return false;
    if (!previous.exists() && !m_deliveryRevision.isEmpty()) return false;
    const QByteArray bytes = QJsonDocument::fromVariant(deliveries).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return false;
    m_deliveries = deliveries;
    m_deliveryRevision = bytes;
    emit changed();
    return true;
}

bool AppleShare::changeDelivery(const QString &id, const QString &state, const QString &error)
{
    auto jobs = m_deliveries;
    for (qsizetype i = 0; i < jobs.size(); ++i) {
        auto job = jobs.at(i).toMap();
        if (job.value(QStringLiteral("id")) != id) continue;
        job.insert(QStringLiteral("state"), state);
        job.insert(QStringLiteral("error"), error);
        jobs[i] = job;
        if (saveDeliveries(jobs)) return true;
        setState(false, tr("Impossibile aggiornare la coda Apple. Riapri Quaderno e riprova."));
        return false;
    }
    return false;
}

void AppleShare::prepareDelivery(Notebook *notebook, const QString &destination)
{
#ifdef Q_OS_MACOS
    if (!available() || m_busy || !notebook || notebook->bookId().isEmpty() || m_deliveryLoadFailed)
        return;
    if (!QStringList{QStringLiteral("share"), QStringLiteral("notes"), QStringLiteral("books"),
        QStringLiteral("numbers"), QStringLiteral("keynote"), QStringLiteral("journal")}.contains(destination)) return;
    if (m_deliveries.size() >= 256) {
        setState(false, tr("La coda Apple contiene 256 esportazioni. I file sono conservati."));
        return;
    }
    setState(true);
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QDir folder(QDir(m_deliveryDirectory).filePath(id));
    if (!QDir().mkpath(folder.path())) {
        setState(false, tr("Impossibile preparare la cartella di condivisione."));
        return;
    }
    const bool notes = destination == QStringLiteral("notes");
    const bool numbers = destination == QStringLiteral("numbers");
    const QString name = notes ? QStringLiteral("quaderno.md") : numbers ? QStringLiteral("tabella.csv") : QStringLiteral("quaderno.pdf");
    const QString path = folder.filePath(name);
    bool exported = false;
    if (notes) {
        exported = notebook->exportMarkdownWithAssets(QUrl::fromLocalFile(path));
    } else if (numbers) {
        QVariantMap table;
        for (const auto &item : notebook->pageItems())
            if (item.toMap().value(QStringLiteral("type")) == QStringLiteral("table")) { table = item.toMap(); break; }
        if (table.isEmpty()) {
            setState(false, tr("Apri una pagina con una tabella per esportarla in Numbers."));
            return;
        }
        exported = notebook->exportTableCsv(QUrl::fromLocalFile(path), table);
    } else {
        exported = notebook->exportPdf(path);
    }
    if (!exported) {
        setState(false, notebook->error());
        return;
    }
    auto jobs = m_deliveries;
    jobs.prepend(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("file"), name},
        {QStringLiteral("title"), notebook->title()}, {QStringLiteral("bookId"), notebook->bookId()},
        {QStringLiteral("revision"), notebook->bookTransferRevision(notebook->bookId())},
        {QStringLiteral("destination"), destination}, {QStringLiteral("state"), QStringLiteral("prepared")},
        {QStringLiteral("created"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    if (!saveDeliveries(jobs)) {
        setState(false, tr("File preparato, ma la coda Apple non è stata salvata."));
        return;
    }
    setState(false);
    retryDelivery(id);
#else
    Q_UNUSED(notebook)
    Q_UNUSED(destination)
#endif
}

void AppleShare::retryDelivery(const QString &id)
{
#ifdef Q_OS_MACOS
    if (!available() || m_busy) return;
    for (const auto &value : m_deliveries) {
        const auto job = value.toMap();
        if (job.value(QStringLiteral("id")) != id) continue;
        const QString state = job.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("completed") || state == QStringLiteral("cancelled")) return;
        const QString path = deliveryFileUrl(id).toLocalFile();
        if (path.isEmpty()) {
            changeDelivery(id, QStringLiteral("failed"), tr("Il file esportato non è disponibile."));
            return;
        }
        if (!changeDelivery(id, QStringLiteral("prepared"))) return;
        setState(true);
        const QPointer<AppleShare> self(this);
        appleShareMac::openExport(path, job.value(QStringLiteral("destination")).toString(), [self, id](bool opened) {
            if (!self) return;
            const QString message = opened ? self->tr("Completa l’importazione nell’app Apple, poi confermala qui.")
                : self->tr("Impossibile aprire l’app Apple. Il file è conservato e puoi riprovare.");
            const bool saved = self->changeDelivery(id, opened ? QStringLiteral("awaitingConfirmation") : QStringLiteral("failed"), opened ? QString{} : message);
            if (saved) self->setState(false, message);
        });
        return;
    }
#else
    Q_UNUSED(id)
#endif
}

QUrl AppleShare::deliveryFileUrl(const QString &id) const
{
    if (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id)
        return {};
    const QFileInfo root(m_deliveryDirectory);
    const QString canonicalRoot = root.canonicalFilePath();
    if (canonicalRoot.isEmpty() || !root.isDir() || root.isSymLink())
        return {};
    for (const auto &entry : m_deliveries) {
        const auto job = entry.toMap();
        if (job.value(QStringLiteral("id")).toString() != id)
            continue;
        const QString filename = job.value(QStringLiteral("file")).toString();
        if (filename.isEmpty() || filename == QStringLiteral(".") || filename == QStringLiteral("..")
            || QFileInfo(filename).fileName() != filename)
            return {};
        const QString expectedDirectory = QDir(canonicalRoot).filePath(id);
        const QFileInfo folder(expectedDirectory);
        const QFileInfo file(QDir(expectedDirectory).filePath(filename));
        if (!folder.isDir() || folder.isSymLink() || folder.canonicalFilePath() != expectedDirectory
            || !file.isFile() || file.isSymLink() || !file.isReadable()
            || QFileInfo(file.canonicalFilePath()).absolutePath() != expectedDirectory)
            return {};
        return QUrl::fromLocalFile(file.canonicalFilePath());
    }
    return {};
}

bool AppleShare::revealDelivery(const QString &id)
{
#ifdef Q_OS_MACOS
    if (!available() || m_busy)
        return false;
    const QUrl file = deliveryFileUrl(id);
    if (file.isEmpty()) {
        setState(false, tr("Il file esportato non è disponibile. Puoi preparare una nuova esportazione dal quaderno."));
        return false;
    }
    appleShareMac::revealExport(file.toLocalFile());
    return true;
#else
    Q_UNUSED(id)
    return false;
#endif
}

bool AppleShare::confirmDelivery(const QString &id)
{
    if (m_busy) return false;
    for (const auto &value : m_deliveries) {
        const auto job = value.toMap();
        if (job.value(QStringLiteral("id")) == id && job.value(QStringLiteral("state")) == QStringLiteral("awaitingConfirmation"))
            return changeDelivery(id, QStringLiteral("completed"));
    }
    return false;
}

bool AppleShare::cancelDelivery(const QString &id)
{
    if (m_busy) return false;
    for (const auto &value : m_deliveries) {
        const auto job = value.toMap();
        if (job.value(QStringLiteral("id")) == id && job.value(QStringLiteral("state")) != QStringLiteral("completed"))
            return changeDelivery(id, QStringLiteral("cancelled"));
    }
    return false;
}

bool AppleShare::archiveCompletedDeliveries()
{
    if (m_busy) return false;
    QVariantList pending;
    for (const auto &value : m_deliveries) {
        const auto state = value.toMap().value(QStringLiteral("state")).toString();
        if (state != QStringLiteral("completed") && state != QStringLiteral("cancelled"))
            pending.append(value);
    }
    // Archive queue entries only. Their exported files remain available on disk.
    if (!saveDeliveries(pending)) return false;
    setState(false, tr("Cronologia completata archiviata. I file esportati sono conservati."));
    return true;
}

void AppleShare::savePdf(Notebook *notebook)
{
#ifdef Q_OS_MACOS
    if (!available() || m_busy || !notebook || notebook->bookId().isEmpty())
        return;

    const QString originalBook = notebook->bookId();
    const auto model = QPointer<Notebook>(notebook);
    const auto self = QPointer<AppleShare>(this);
    setState(true);
    appleShareMac::choosePdfDestination(notebook->pdfFileName(), [self, model, originalBook](const QString &path) {
        if (!self)
            return;
        if (path.isEmpty()) {
            self->setState(false);
        } else if (!model || model->bookId() != originalBook) {
            self->setState(false, tr("Il quaderno selezionato è cambiato. Riprova."));
        } else if (model->exportPdf(path)) {
            self->setState(false, tr("PDF salvato."));
        } else {
            self->setState(false, model->error());
        }
    });
#else
    Q_UNUSED(notebook)
#endif
}

void AppleShare::importNotebookFile(Notebook *notebook)
{
#ifdef Q_OS_MACOS
    if (!available() || m_busy || !notebook)
        return;

    const auto model = QPointer<Notebook>(notebook);
    const auto self = QPointer<AppleShare>(this);
    setState(true);
    appleShareMac::chooseNotebookFile([self, model](const QString &path) {
        if (!self)
            return;
        if (path.isEmpty() || !model) {
            self->setState(false);
        } else if (model->importFromFile(path)) {
            self->setState(false, tr("File dei quaderni importato."));
        } else {
            self->setState(false, model->error());
        }
    });
#else
    Q_UNUSED(notebook)
#endif
}
