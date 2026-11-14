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
        && !m_importing && !m_dragging;
}

bool MacDocumentExchange::hasSupportedFiles(const QList<QUrl> &files) const
{
    if (!canImport() || files.size() > maximumFiles)
        return false;
    for (const QUrl &file : files) {
        if (isDocument(file))
            return true;
    }
    return false;
}

bool MacDocumentExchange::importFiles(const QList<QUrl> &files, const QString &folderId)
{
    if (!canImport())
        return false;
    if (!hasSupportedFiles(files)) {
        setStatus(files.size() > maximumFiles ? tr("Importa al massimo %1 documenti alla volta.").arg(maximumFiles)
                                             : tr("Scegli un file PDF, EPUB o Markdown locale."));
        return false;
    }

    QScopedValueRollback importing(m_importing, true);
    QSet<QString> seen;
    QString lastBook;
    QString error;
    int importedCount = 0;
    int skippedCount = 0;
    for (const QUrl &file : files) {
        if (m_locked || !m_notebook)
            return false;
        if (!isDocument(file)) {
            ++skippedCount;
            continue;
        }
        const QFileInfo info(file.toLocalFile());
        const bool epubPackage = info.isDir() && info.suffix().compare(QStringLiteral("epub"), Qt::CaseInsensitive) == 0;
        if ((!info.isFile() && !epubPackage) || !info.isReadable()) {
            ++skippedCount;
            if (error.isEmpty())
                error = tr("Impossibile leggere uno dei documenti selezionati.");
            continue;
        }
        const QString canonical = info.canonicalFilePath();
        if (seen.contains(canonical)) {
            ++skippedCount;
            continue;
        }
        seen.insert(canonical);
        if (m_notebook->importDocument(file, folderId)) {
            ++importedCount;
            lastBook = m_notebook->bookId();
        } else {
            ++skippedCount;
            if (error.isEmpty())
                error = m_notebook->error();
        }
    }
    if (m_locked)
        return false;
    if (lastBook.isEmpty()) {
        setStatus(error.isEmpty() ? tr("Impossibile importare i documenti selezionati.") : error);
        return false;
    }
    if (skippedCount == 0)
        setStatus(importedCount == 1 ? tr("Documento importato.") : tr("%1 documenti importati.").arg(importedCount));
    else
        setStatus(tr("Importati %1 documenti su %2. %3").arg(importedCount).arg(files.size()).arg(error).trimmed());
    emit imported(lastBook);
    return true;
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
