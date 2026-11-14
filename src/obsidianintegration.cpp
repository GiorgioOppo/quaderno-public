#include "obsidianintegration.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QUuid>

#include <utility>

#ifdef Q_OS_MACOS
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
const QString publishSetting = QStringLiteral("obsidian/publishBaseUrl");

QSettings obsidianSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Quaderno"), QStringLiteral("Quaderno"));
}

bool hasControls(const QString &text)
{
    static const QRegularExpression controls(QStringLiteral("[\\p{Cc}\\p{Cf}]"));
    return controls.match(text).hasMatch();
}

bool pathPart(const QString &part)
{
    return !part.isEmpty() && part != QStringLiteral(".") && part != QStringLiteral("..")
        && !part.contains(QLatin1Char('/')) && !part.contains(QLatin1Char('\\')) && !hasControls(part);
}

QUrl httpsBase(const QString &address)
{
    if (hasControls(address))
        return {};
    QUrl url(address.trimmed(), QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty() || url.hasQuery()
        || url.hasFragment() || url.authority(QUrl::FullyEncoded).contains(QLatin1Char('@')))
        return {};
    QString path = url.path(QUrl::FullyEncoded);
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    if (!path.isEmpty()) {
        const auto parts = path.mid(1).split(QLatin1Char('/'));
        for (const QString &part : parts) {
            if (!pathPart(QUrl::fromPercentEncoding(part.toLatin1())))
                return {};
        }
    }
    url.setPath(path, QUrl::StrictMode);
    return url;
}

const QRegularExpression &invalidNoteCharacters()
{
    static const QRegularExpression invalid(QStringLiteral("[\\\\:*?\"<>|#^\\[\\]\\p{Cc}\\p{Cf}]"));
    return invalid;
}

QStringList vaultNoteParts(const QString &notePath)
{
    QStringList parts = notePath.trimmed().split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (!pathPart(part) || part.startsWith(QLatin1Char('.')) || part.endsWith(QLatin1Char('.'))
            || part.trimmed() != part || part.contains(invalidNoteCharacters()) || QFile::encodeName(part).size() > 255)
            return {};
    }
    if (!parts.last().endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        parts.last().append(QStringLiteral(".md"));
    if (QFile::encodeName(parts.last()).size() > 255)
        return {};
    return parts;
}

QUrl absoluteNoteUrl(const QString &absolutePath)
{
    return QUrl::fromEncoded("obsidian://open?path=" + QUrl::toPercentEncoding(absolutePath), QUrl::StrictMode);
}

#ifdef Q_OS_MACOS
// Open every component relative to an already-open directory. A replaced
// symlink cannot redirect the note outside the folder the user chose.
int openDirectory(int parent, const QStringList &parts)
{
    int directory = parent;
    for (const QString &part : parts) {
        const int child = ::openat(directory, QFile::encodeName(part).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(directory);
        if (child < 0)
            return -1;
        directory = child;
    }
    return directory;
}

enum class SaveResult { Saved, Exists, Failed };

SaveResult writeNote(int directory, const QByteArray &name, const QByteArray &content)
{
    const QByteArray stage = ".quaderno-" + QUuid::createUuid().toByteArray(QUuid::Id128);
    const int file = ::openat(directory, stage.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (file < 0)
        return SaveResult::Failed;
    qsizetype offset = 0;
    while (offset < content.size()) {
        const auto count = ::write(file, content.constData() + offset, size_t(content.size() - offset));
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            break;
        offset += count;
    }
    const bool flushed = offset == content.size() && ::fsync(file) == 0;
    const bool closed = ::close(file) == 0;
    SaveResult result = SaveResult::Failed;
    if (flushed && closed) {
        if (::renameatx_np(directory, stage.constData(), directory, name.constData(), RENAME_EXCL) == 0)
            return SaveResult::Saved;
        if (errno == ENOTSUP) {
            if (::linkat(directory, stage.constData(), directory, name.constData(), 0) == 0)
                result = SaveResult::Saved;
        }
        if (result != SaveResult::Saved && errno == EEXIST)
            result = SaveResult::Exists;
    }
    ::unlinkat(directory, stage.constData(), 0);
    return result;
}
#endif
}

ObsidianIntegration::ObsidianIntegration(QObject *parent) : QObject(parent)
{
    m_publishBase = httpsBase(obsidianSettings().value(publishSetting).toString());
}

bool ObsidianIntegration::available() const
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

bool ObsidianIntegration::setPublishBaseUrl(const QString &address)
{
    const QUrl url = httpsBase(address);
    if (!address.trimmed().isEmpty() && url.isEmpty())
        return report(tr("Inserisci l’indirizzo HTTPS del sito Obsidian Publish."));
    if (m_publishBase != url) {
        auto settings = obsidianSettings();
        if (url.isEmpty())
            settings.remove(publishSetting);
        else
            settings.setValue(publishSetting, url.toString(QUrl::FullyEncoded));
        settings.sync();
        if (settings.status() != QSettings::NoError)
            return report(tr("Impossibile salvare l’indirizzo del sito."));
        m_publishBase = url;
        emit publishBaseUrlChanged();
    }
    return report({}, true);
}

QUrl ObsidianIntegration::publishUrl(const QString &notePath) const
{
    if (m_publishBase.isEmpty() || hasControls(notePath))
        return {};
    if (notePath.trimmed().isEmpty())
        return m_publishBase;

    QStringList parts = notePath.trimmed().split(QLatin1Char('/'));
    if (parts.last().endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        parts.last().chop(3);
    QString path = m_publishBase.path(QUrl::FullyEncoded);
    for (const QString &part : std::as_const(parts)) {
        if (!pathPart(part))
            return {};
        QByteArray encoded = QUrl::toPercentEncoding(part);
        encoded.replace("%20", "+");
        path += QLatin1Char('/') + QString::fromLatin1(encoded);
    }
    QUrl url = m_publishBase;
    url.setPath(path, QUrl::StrictMode);
    return url;
}

bool ObsidianIntegration::openPublished(const QString &notePath)
{
    const QUrl url = publishUrl(notePath);
    if (url.isEmpty())
        return report(tr("Controlla l’indirizzo del sito e il percorso della nota pubblicata."));
    if (!available() || !QDesktopServices::openUrl(url))
        return report(tr("Impossibile aprire il collegamento nel browser."));
    return report({}, true);
}

bool ObsidianIntegration::copyPublished(const QString &notePath)
{
    const QUrl url = publishUrl(notePath);
    if (url.isEmpty())
        return report(tr("Controlla l’indirizzo del sito e il percorso della nota pubblicata."));
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return report(tr("Appunti di sistema non disponibili."));
    clipboard->setText(url.toString(QUrl::FullyEncoded));
    return report(tr("Collegamento copiato."), true);
}

QUrl ObsidianIntegration::vaultUrl(const QString &vault, const QString &notePath) const
{
    const QString name = vault.trimmed();
    const QStringList parts = vaultNoteParts(notePath);
    if (name.isEmpty() || hasControls(vault) || parts.isEmpty())
        return {};
    return QUrl::fromEncoded("obsidian://open?vault=" + QUrl::toPercentEncoding(name)
                             + "&file=" + QUrl::toPercentEncoding(parts.join(QLatin1Char('/'))), QUrl::StrictMode);
}

bool ObsidianIntegration::openInObsidian(const QString &vault, const QString &notePath)
{
    const QUrl url = vaultUrl(vault, notePath);
    if (!available() || url.isEmpty() || !QDesktopServices::openUrl(url))
        return report(tr("Impossibile aprire la nota in Obsidian."));
    return report({}, true);
}

bool ObsidianIntegration::saveMarkdown(const QUrl &vaultFolder, const QString &notePath, const QString &markdown)
{
#ifdef Q_OS_MACOS
    setLastSavedFile({});
    if (markdown.isEmpty())
        return report(tr("La nota Markdown è vuota."));
    const QString folder = vaultFolder.toLocalFile();
    if (!vaultFolder.isLocalFile() || !vaultFolder.host().isEmpty() || vaultFolder.hasQuery() || vaultFolder.hasFragment()
        || !QDir::isAbsolutePath(folder) || folder.contains(QChar::Null))
        return report(tr("Scegli una cartella locale del vault Obsidian."));
    const QFileInfo rootInfo(folder);
    const QString root = rootInfo.canonicalFilePath();
    if (!rootInfo.isDir() || root.isEmpty())
        return report(tr("Scegli una cartella locale del vault Obsidian."));
    const QStringList parts = vaultNoteParts(notePath);
    if (parts.isEmpty())
        return report(tr("Inserisci un percorso Markdown valido all’interno del vault."));

    const int filesystem = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (filesystem < 0)
        return report(tr("Impossibile accedere alla cartella del vault."));
    const QStringList directories = root.split(QLatin1Char('/'), Qt::SkipEmptyParts) + parts.mid(0, parts.size() - 1);
    const int destination = openDirectory(filesystem, directories);
    if (destination < 0)
        return report(tr("La sottocartella del vault deve esistere e non può essere un collegamento simbolico."));
    const SaveResult result = writeNote(destination, QFile::encodeName(parts.last()), markdown.toUtf8());
    ::close(destination);
    if (result == SaveResult::Exists)
        return report(tr("Una nota con questo nome esiste già. Scegli un altro nome."));
    if (result != SaveResult::Saved)
        return report(tr("Impossibile salvare il file Markdown nel vault."));
    setLastSavedFile(QUrl::fromLocalFile(QDir(root).filePath(parts.join(QLatin1Char('/')))));
    return report(tr("Nota Markdown salvata nel vault."), true);
#else
    Q_UNUSED(vaultFolder)
    Q_UNUSED(notePath)
    Q_UNUSED(markdown)
    return false;
#endif
}

bool ObsidianIntegration::openSavedNote()
{
    const QString path = m_lastSavedFile.toLocalFile();
    const QFileInfo file(path);
    if (!available() || path.isEmpty() || !file.isFile() || file.isSymLink() || !QDesktopServices::openUrl(absoluteNoteUrl(path)))
        return report(tr("Impossibile aprire la nota in Obsidian."));
    return report({}, true);
}

void ObsidianIntegration::clearLastSavedNote()
{
    setLastSavedFile({});
    report({});
}

QString ObsidianIntegration::suggestedFileName(const QString &title) const
{
    QString name = title;
    name.replace(QLatin1Char('/'), QLatin1Char('-'));
    name.replace(invalidNoteCharacters(), QStringLiteral("-"));
    name = name.simplified().left(56);
    if (!name.isEmpty() && name.back().isHighSurrogate())
        name.chop(1);
    name = name.trimmed();
    if (name.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        name.chop(3);
    while (name.startsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char('.')))
        name = name.startsWith(QLatin1Char('.')) ? name.mid(1).trimmed() : name.chopped(1).trimmed();
    return (name.isEmpty() ? tr("Quaderno") : name) + QStringLiteral(".md");
}

void ObsidianIntegration::setLastSavedFile(const QUrl &file)
{
    if (m_lastSavedFile != file) {
        m_lastSavedFile = file;
        emit lastSavedFileChanged();
    }
}

bool ObsidianIntegration::report(const QString &message, bool success)
{
    if (m_status != message) {
        m_status = message;
        emit statusChanged();
    }
    return success;
}
