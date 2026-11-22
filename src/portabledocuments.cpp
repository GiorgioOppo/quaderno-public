#include "portabledocuments.h"
#include "notebook.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>

namespace { constexpr qint64 maximumBytes = 64 * 1024 * 1024; }

PortableDocuments::PortableDocuments(QObject *parent) : QObject(parent) {}
PortableDocuments::~PortableDocuments() = default;
Notebook *PortableDocuments::notebook() const { return m_notebook; }
void PortableDocuments::setNotebook(Notebook *value)
{
    if (m_notebook == value) return;
    m_notebook = value;
    clear();
}
void PortableDocuments::setLocked(bool value)
{
    if (m_locked == value) return;
    m_locked = value;
    if (value) { clear(); m_export.reset(); }
    emit changed();
}
void PortableDocuments::clear()
{
    m_payload.clear(); m_preview.clear(); m_error.clear();
    emit changed();
}
bool PortableDocuments::prepareImport(const QUrl &url)
{
    clear();
    if (m_locked || !m_notebook || m_notebook->importing()) return false;
    const QFileInfo info(url.toLocalFile());
    if (!url.isLocalFile() || (!url.host().isEmpty() && url.host() != QStringLiteral("localhost"))
        || !info.isAbsolute() || !info.isFile() || info.isSymLink() || info.size() > maximumBytes
        || info.suffix().compare(QStringLiteral("quaderno"), Qt::CaseInsensitive) != 0) {
        m_error = tr("Scegli un file Quaderno locale valido, fino a 64 MiB.");
        emit changed(); return false;
    }
    QFile input(info.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly)) {
        m_error = tr("Impossibile leggere il quaderno scaricato.");
        emit changed(); return false;
    }
    m_payload = input.read(maximumBytes + 1);
    if (input.error() != QFileDevice::NoError || m_payload.size() > maximumBytes) {
        m_payload.clear(); m_error = tr("Il quaderno scaricato supera il limite o non è leggibile.");
        emit changed(); return false;
    }
    m_preview = m_notebook->previewImportFromData(m_payload);
    m_error = m_preview.value(QStringLiteral("error")).toString();
    if (!m_error.isEmpty()) m_payload.clear();
    emit changed();
    return m_error.isEmpty() && !m_preview.value(QStringLiteral("token")).toString().isEmpty();
}
bool PortableDocuments::applyImport()
{
    if (m_locked || !m_notebook || m_notebook->importing() || m_payload.isEmpty()
        || m_preview.value(QStringLiteral("token")).toString().isEmpty()) return false;
    const auto result = m_notebook->applyImportFromData(m_payload, m_preview.value(QStringLiteral("token")).toString());
    m_error = result.value(QStringLiteral("error")).toString();
    if (m_error.isEmpty()) clear();
    else emit changed();
    return m_error.isEmpty();
}
QUrl PortableDocuments::exportCurrent()
{
    if (m_locked || !m_notebook || m_notebook->importing()) return {};
    const auto payload = m_notebook->exportBookForTransfer(m_notebook->bookId());
    if (payload.isEmpty()) { m_error = m_notebook->error(); emit changed(); return {}; }
    auto directory = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/quaderno-cloud-export-XXXXXX"));
    if (!directory->isValid()) { m_error = tr("Impossibile preparare il quaderno da caricare."); emit changed(); return {}; }
    const QString path = directory->filePath(QStringLiteral("quaderno.quaderno"));
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || !output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || output.write(payload) != payload.size() || !output.commit()) {
        m_error = tr("Impossibile salvare la copia temporanea del quaderno."); emit changed(); return {};
    }
    m_export = std::move(directory);
    m_error.clear(); emit changed();
    return QUrl::fromLocalFile(path);
}
