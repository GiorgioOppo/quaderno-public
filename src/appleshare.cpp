#include "appleshare.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>
#include <QUuid>

#ifdef Q_OS_MACOS
#include <functional>

namespace appleShareMac {
using FileSelection = std::function<void(const QString &)>;
void choosePdfDestination(const QString &fileName, FileSelection selected);
void chooseNotebookFile(FileSelection selected);
bool sharePdf(const QString &path);
}
#endif

AppleShare::AppleShare(bool tablet, QObject *parent) : QObject(parent), m_tablet(tablet) {}

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

QString AppleShare::sharingPath(const QString &fileName)
{
    if (fileName.isEmpty() || QFileInfo(fileName).fileName() != fileName
        || !fileName.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        return {};
    if (!m_sharedFiles)
        m_sharedFiles = std::make_unique<QTemporaryDir>(QDir::temp().filePath(QStringLiteral("Quaderno-XXXXXX")));
    if (!m_sharedFiles->isValid())
        return {};

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QDir root(m_sharedFiles->path());
    if (!root.mkdir(id))
        return {};
    return QDir(root.filePath(id)).filePath(fileName);
}

void AppleShare::sharePdf(Notebook *notebook)
{
#ifdef Q_OS_MACOS
    if (!available() || m_busy || !notebook || notebook->bookId().isEmpty())
        return;

    setState(true);
    const QString path = sharingPath(notebook->pdfFileName());
    if (path.isEmpty()) {
        setState(false, tr("Impossibile preparare la cartella di condivisione."));
        return;
    }
    if (!notebook->exportPdf(path)) {
        setState(false, notebook->error());
        return;
    }

    const bool showedPicker = appleShareMac::sharePdf(path);
    setState(false, showedPicker ? tr("Scegli una destinazione nel menu Condividi.")
                                 : tr("PDF mostrato nel Finder. Usa Condividi dal menu del file."));
#else
    Q_UNUSED(notebook)
#endif
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
