#include "macdocumentexchange.h"

#include "notebook.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QScopedValueRollback>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
#include <utility>

#ifdef Q_OS_MACOS
#include <QDrag>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QWindow>
#include <functional>

namespace appleShareMac {
void openAppleBooks(std::function<void(bool)> completed);
void chooseAppleBooksFiles(std::function<void(const QList<QUrl> &)> selected);
}
#endif

namespace {
constexpr qsizetype maximumFiles = 16;
#ifdef Q_OS_MACOS
constexpr int dragFileLifetimeMs = 5 * 60 * 1000;
#endif

bool isDocument(const QUrl &file)
{
    if (!file.isLocalFile() || (!file.host().isEmpty() && file.host() != QStringLiteral("localhost")))
        return false;
    const QFileInfo info(file.toLocalFile());
    if (!info.isAbsolute())
        return false;
    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("pdf") || suffix == QStringLiteral("epub")
        || suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}
}

MacDocumentExchange::MacDocumentExchange(Notebook *notebook, bool tablet, QObject *parent)
    : QObject(parent), m_notebook(notebook), m_tablet(tablet)
{
    if (available())
        QCoreApplication::instance()->installEventFilter(this);
    connect(m_notebook, &Notebook::documentImported, this, [this](const QString &id) {
        if (m_importing && m_waitingImport) m_lastImportedBook = id;
    });
    connect(m_notebook, &Notebook::importFinished, this, [this](bool success) {
        if (!m_importing || !m_waitingImport)
            return;
        m_waitingImport = false;
        finishImportEntry(success);
        QTimer::singleShot(0, this, &MacDocumentExchange::importNext);
    });
}

MacDocumentExchange::~MacDocumentExchange() = default;

bool MacDocumentExchange::available() const
{
#ifdef Q_OS_MACOS
    return !m_tablet;
#else
    return false;
#endif
}

void MacDocumentExchange::setLocked(bool locked)
{
    m_locked = locked;
    if (locked) {
        if (m_importing && m_waitingImport && m_notebook)
            m_notebook->cancelDocumentImport();
        m_importing = false;
        m_waitingImport = false;
        m_importFiles.clear();
        m_importRows.clear();
        m_importReport.clear();
        m_reportFiles.clear();
        m_activeImportRow = -1;
        emit importReportChanged();
        m_finderFiles.clear();
        m_exports.clear();
        m_dragLocked = m_dragging;
        setStatus({});
    }
}

void MacDocumentExchange::setImportPaused(bool paused)
{
    if (m_importPaused == paused)
        return;
    m_importPaused = paused;
    if (paused)
        m_finderFiles.clear();
    emit importPausedChanged();
}

bool MacDocumentExchange::canImport() const
{
    return available() && m_notebook && !m_locked && !m_importPaused && !m_appleBooksPanelOpen
        && !m_importing && !m_notebook->importing() && !m_dragging;
}

bool MacDocumentExchange::hasSupportedFiles(const QList<QUrl> &files) const
{
    const auto preview = importPreview(files);
    return preview.value(QStringLiteral("allowed")).toBool() && preview.value(QStringLiteral("accepted")).toInt() > 0;
}

QVariantMap MacDocumentExchange::importPreview(const QList<QUrl> &files) const
{
    QSet<QString> seen;
    QVariantList entries;
    int accepted = 0, duplicates = 0, unsupported = 0, unreadable = 0;
    // A drag can contain arbitrary URLs. Inspect only bounded local metadata, never file contents.
    for (const QUrl &file : files.mid(0, maximumFiles)) {
        const QFileInfo info(file.toLocalFile());
        QString state, message;
        QUrl canonicalUrl = file;
        const bool package = info.isDir() && info.suffix().compare(QStringLiteral("epub"), Qt::CaseInsensitive) == 0;
        if (!isDocument(file)) {
            state = QStringLiteral("unsupported");
            message = tr("Formato non supportato. Scegli PDF, EPUB o Markdown locali.");
            ++unsupported;
        } else if ((!info.isFile() && !package) || !info.isReadable()) {
            state = QStringLiteral("failed");
            message = tr("Il file non è leggibile o non è disponibile sul Mac.");
            ++unreadable;
        } else {
            const QString canonical = info.canonicalFilePath();
            canonicalUrl = QUrl::fromLocalFile(canonical);
            if (seen.contains(canonical)) {
                state = QStringLiteral("duplicate");
                message = tr("File già presente in questa importazione.");
                ++duplicates;
            } else {
                seen.insert(canonical);
                state = QStringLiteral("queued");
                ++accepted;
            }
        }
        entries.append(QVariantMap{{QStringLiteral("name"), file.fileName()}, {QStringLiteral("state"), state},
            {QStringLiteral("message"), message}, {QStringLiteral("url"), canonicalUrl}});
    }
    const bool allowed = canImport() && files.size() <= maximumFiles;
    return {{QStringLiteral("allowed"), allowed}, {QStringLiteral("accepted"), accepted},
        {QStringLiteral("duplicates"), duplicates}, {QStringLiteral("unsupported"), unsupported},
        {QStringLiteral("unreadable"), unreadable}, {QStringLiteral("entries"), entries},
        {QStringLiteral("message"), files.size() > maximumFiles ? tr("Importa al massimo %1 documenti alla volta.").arg(maximumFiles)
            : !canImport() ? tr("Attendi il termine dell’operazione in corso.") : QString{}}};
}

bool MacDocumentExchange::importFiles(const QList<QUrl> &files, const QString &folderId)
{
    const auto preview = importPreview(files);
    if (!preview.value(QStringLiteral("allowed")).toBool()) {
        setStatus(preview.value(QStringLiteral("message")).toString());
        return false;
    }
    m_importFiles.clear();
    m_importRows.clear();
    m_importReport.clear();
    m_reportFiles.clear();
    m_importError.clear();
    const auto entries = preview.value(QStringLiteral("entries")).toList();
    for (const auto &value : entries) {
        auto entry = value.toMap();
        const auto file = entry.take(QStringLiteral("url")).toUrl();
        const int row = m_importReport.size();
        m_importReport.append(entry);
        m_reportFiles.append(file);
        if (entry.value(QStringLiteral("state")) == QStringLiteral("queued")) {
            m_importFiles.append(file);
            m_importRows.append(row);
        }
    }
    m_importFolder = folderId;
    m_importFileCount = m_importFiles.size();
    m_importedCount = 0;
    m_lastImportedBook.clear();
    m_importCancelled = false;
    m_activeImportRow = -1;
    if (m_importFiles.isEmpty()) {
        setStatus(tr("Nessun documento importabile. Apri il rapporto per i dettagli."));
        emit importReportChanged();
        return false;
    }
    m_importing = true;
    emit importReportChanged();
    setStatus(tr("Preparazione dell’importazione…"));
    QTimer::singleShot(0, this, &MacDocumentExchange::importNext);
    return true;
}

void MacDocumentExchange::importNext()
{
    if (!m_importing || m_waitingImport || m_locked || !m_notebook)
        return;
    if (m_importFiles.isEmpty()) {
        m_importing = false;
        m_activeImportRow = -1;
        int importedCount = 0, failed = 0, skipped = 0, cancelled = 0;
        for (const auto &value : std::as_const(m_importReport)) {
            const QString state = value.toMap().value(QStringLiteral("state")).toString();
            importedCount += state == QStringLiteral("imported");
            failed += state == QStringLiteral("failed");
            skipped += state == QStringLiteral("duplicate") || state == QStringLiteral("unsupported");
            cancelled += state == QStringLiteral("cancelled");
        }
        setStatus(tr("Importati: %1 · Non riusciti: %2 · Ignorati: %3 · Annullati: %4")
            .arg(importedCount).arg(failed).arg(skipped).arg(cancelled));
        emit importReportChanged();
        if (!m_lastImportedBook.isEmpty())
            emit imported(m_lastImportedBook);
        return;
    }
    const QUrl file = m_importFiles.takeFirst();
    m_activeImportRow = m_importRows.takeFirst();
    updateImportEntry(m_activeImportRow, QStringLiteral("importing"));
    setStatus(tr("Importazione di %1…").arg(QFileInfo(file.toLocalFile()).fileName()));
    m_waitingImport = true;
    if (!m_notebook->startDocumentImport(file, m_importFolder) && m_waitingImport) {
        m_waitingImport = false;
        finishImportEntry(false);
        QTimer::singleShot(0, this, &MacDocumentExchange::importNext);
    }
}

void MacDocumentExchange::updateImportEntry(int index, const QString &state, const QString &message)
{
    if (index < 0 || index >= m_importReport.size()) return;
    auto entry = m_importReport.at(index).toMap();
    entry.insert(QStringLiteral("state"), state);
    entry.insert(QStringLiteral("message"), message);
    m_importReport[index] = entry;
    emit importReportChanged();
}

void MacDocumentExchange::finishImportEntry(bool success)
{
    if (success) {
        ++m_importedCount;
        updateImportEntry(m_activeImportRow, QStringLiteral("imported"));
    } else {
        const bool cancelled = m_importCancelled || (m_notebook && m_notebook->error().isEmpty());
        m_importError = m_notebook ? m_notebook->error() : QString{};
        updateImportEntry(m_activeImportRow, cancelled ? QStringLiteral("cancelled") : QStringLiteral("failed"),
                          cancelled ? tr("Importazione annullata.") : m_importError);
        if (cancelled) cancelImports();
    }
    m_activeImportRow = -1;
}

bool MacDocumentExchange::canRetryImports() const
{
    if (m_importing || m_locked) return false;
    for (const auto &value : m_importReport)
        if (value.toMap().value(QStringLiteral("state")) == QStringLiteral("failed")) return true;
    return false;
}

bool MacDocumentExchange::retryFailedImports()
{
    if (!canImport() || !canRetryImports()) return false;
    m_importFiles.clear();
    m_importRows.clear();
    for (int row = 0; row < m_importReport.size(); ++row) {
        if (m_importReport.at(row).toMap().value(QStringLiteral("state")) != QStringLiteral("failed")) continue;
        m_importFiles.append(m_reportFiles.at(row));
        m_importRows.append(row);
        updateImportEntry(row, QStringLiteral("queued"));
    }
    m_importFileCount = m_importFiles.size();
    m_importedCount = 0;
    m_importCancelled = false;
    m_importError.clear();
    m_lastImportedBook.clear();
    m_importing = true;
    emit importReportChanged();
    QTimer::singleShot(0, this, &MacDocumentExchange::importNext);
    return true;
}

void MacDocumentExchange::cancelImports()
{
    if (!m_importing) return;
    m_importCancelled = true;
    for (const int row : std::as_const(m_importRows))
        updateImportEntry(row, QStringLiteral("cancelled"), tr("Importazione annullata."));
    m_importFiles.clear();
    m_importRows.clear();
    if (m_waitingImport && m_notebook) m_notebook->cancelDocumentImport();
    else QTimer::singleShot(0, this, &MacDocumentExchange::importNext);
}

void MacDocumentExchange::clearImportReport()
{
    if (m_importing) return;
    m_importReport.clear();
    m_reportFiles.clear();
    setStatus({});
    emit importReportChanged();
}

void MacDocumentExchange::openAppleBooks()
{
#ifdef Q_OS_MACOS
    if (!available() || m_locked)
        return;
    setStatus({});
    appleShareMac::openAppleBooks([self = QPointer<MacDocumentExchange>(this)](bool opened) {
        if (self && !self->m_locked && !opened)
            self->setStatus(tr("Impossibile aprire Apple Books."));
    });
#endif
}

void MacDocumentExchange::chooseAppleBooksFiles(const QString &folderId)
{
#ifdef Q_OS_MACOS
    if (!canImport())
        return;
    m_appleBooksPanelOpen = true;
    setStatus({});
    appleShareMac::chooseAppleBooksFiles([self = QPointer<MacDocumentExchange>(this), folderId](const QList<QUrl> &files) {
        if (!self)
            return;
        self->m_appleBooksPanelOpen = false;
        if (!files.isEmpty())
            self->importFiles(files, folderId);
    });
#else
    Q_UNUSED(folderId)
#endif
}

bool MacDocumentExchange::eventFilter(QObject *watched, QEvent *event)
{
#ifdef Q_OS_MACOS
    if (watched != QCoreApplication::instance() || event->type() != QEvent::FileOpen)
        return QObject::eventFilter(watched, event);

    const QUrl file = static_cast<QFileOpenEvent *>(event)->url();
    if (!isDocument(file))
        return false;
    if (!canImport()) {
        if (m_locked)
            setStatus(tr("Sblocca Quaderno, poi riapri il documento dal Finder."));
        else
            setStatus(tr("Torna al quaderno o alla libreria, poi riapri il documento dal Finder."));
        return true;
    }

    if (m_finderFiles.size() >= maximumFiles)
        return true;
    m_finderFiles.append(file);
    if (!m_finderDispatchPending) {
        m_finderDispatchPending = true;
        QTimer::singleShot(0, this, &MacDocumentExchange::openFinderFiles);
    }
    return true;
#else
    return QObject::eventFilter(watched, event);
#endif
}

void MacDocumentExchange::openFinderFiles()
{
    m_finderDispatchPending = false;
    const QList<QUrl> files = std::exchange(m_finderFiles, {});
    if (!files.isEmpty())
        importFiles(files, {});
}

void MacDocumentExchange::beginPdfDrag(bool currentPage)
{
#ifdef Q_OS_MACOS
    if (!available() || m_locked || m_dragging || m_importing || !m_notebook || m_notebook->bookId().isEmpty())
        return;
    auto *window = QGuiApplication::focusWindow();
    if (!window)
        return;

    QScopedValueRollback dragging(m_dragging, true);
    m_dragLocked = false;
    auto directory = std::make_shared<QTemporaryDir>(QDir::temp().filePath(QStringLiteral("Quaderno-PDF-XXXXXX")));
    if (!directory->isValid()) {
        setStatus(tr("Impossibile preparare il PDF da trascinare."));
        return;
    }
    QString fileName = m_notebook->pdfFileName();
    if (currentPage)
        fileName.insert(fileName.size() - 4, tr(" - Pagina %1").arg(m_notebook->page() + 1));
    const QString path = QDir(directory->path()).filePath(fileName);
    if (!m_notebook->exportPdf(path, currentPage)) {
        setStatus(m_notebook->error());
        return;
    }
    if (m_locked)
        return;

    auto *drag = new QDrag(window);
    auto *mime = new QMimeData;
    mime->setUrls({QUrl::fromLocalFile(path)});
    drag->setMimeData(mime);
    setStatus({});
    drag->exec(Qt::CopyAction, Qt::CopyAction);
    // The Cocoa plugin can return IgnoreAction after a successful external drop.
    // Keep the file available for the receiving application to finish copying it.
    if (!m_dragLocked) {
        m_exports.append(directory);
        const std::weak_ptr<QTemporaryDir> exported = directory;
        QTimer::singleShot(dragFileLifetimeMs, this, [this, exported] {
            if (const auto directory = exported.lock())
                m_exports.removeOne(directory);
        });
    }
#else
    Q_UNUSED(currentPage)
#endif
}

void MacDocumentExchange::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}
