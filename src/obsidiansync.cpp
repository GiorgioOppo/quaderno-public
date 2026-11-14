#include "obsidiansync.h"
#include "notebook.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUuid>

#include <algorithm>
#include <utility>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace {
constexpr qsizetype maximumNotes = 3000;
constexpr qsizetype maximumIndexBytes = 4 * 1024 * 1024;

QString digest(const QString &text)
{
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool markdownPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return !path.isEmpty() && path.size() <= 1024 && !path.contains(QChar::Null)
        && !path.contains(u'\\') && QDir::cleanPath(path) == path && !QDir::isAbsolutePath(path)
        && !path.startsWith(u'.') && !path.contains(QStringLiteral("/."))
        && suffix == QStringLiteral("md");
}

bool hashValue(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[0-9a-f]{64}\\z"));
    return pattern.match(value).hasMatch();
}

bool validBookId(const QString &id)
{
    const QUuid uuid(id);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == id;
}

class MirrorLock
{
public:
    explicit MirrorLock(const QString &path)
    {
#ifdef Q_OS_LINUX
        m_fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        m_locked = m_fd >= 0 && ::flock(m_fd, LOCK_EX | LOCK_NB) == 0;
#else
        Q_UNUSED(path)
        m_locked = true;
#endif
    }

    ~MirrorLock() { release(); }
    bool locked() const { return m_locked; }
    void release()
    {
#ifdef Q_OS_LINUX
        if (m_fd >= 0)
            ::close(std::exchange(m_fd, -1));
#endif
        m_locked = false;
    }

private:
#ifdef Q_OS_LINUX
    int m_fd = -1;
#endif
    bool m_locked = false;
};
}

ObsidianSync::ObsidianSync(Notebook *notebook, QString directory, bool available, QObject *parent)
    : QObject(parent), m_notebook(notebook), m_directory(std::move(directory)), m_available(available)
{
    m_process.setStandardInputFile(QProcess::nullDevice());
    m_process.setStandardErrorFile(QProcess::nullDevice());
    m_process.setWorkingDirectory(m_directory);
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_timedOut = true;
        m_process.kill();
    });
    const auto readOutput = [this] {
        m_output.append(m_process.readAllStandardOutput());
        qsizetype newline;
        while ((newline = m_output.indexOf('\n')) >= 0) {
            const auto line = m_output.first(newline).trimmed();
            if (line == "Fully synced")
                m_complete = true;
            if (line == "Conflicted copy stored " + m_uploadPath.toUtf8())
                m_uploadConflict = true;
            m_output.remove(0, newline + 1);
        }
        if (m_output.size() > 4096)
            m_output.clear();
    };
    connect(&m_process, &QProcess::readyReadStandardOutput, this, readOutput);
    connect(&m_process, &QProcess::finished, this, [this, readOutput](int code, QProcess::ExitStatus status) {
        readOutput();
        finish(code == 0 && status == QProcess::NormalExit && m_complete && !m_timedOut);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(false);
    });
}

bool ObsidianSync::installed() const
{
    const QFileInfo wrapper(QDir(m_directory).filePath(QStringLiteral("ob-quaderno")));
    return m_available && wrapper.isFile() && !wrapper.isSymLink() && wrapper.isExecutable() && !vaultId().isEmpty();
}

QString ObsidianSync::vaultId() const
{
    QFile file(QDir(m_directory).filePath(QStringLiteral("vault-id")));
    if (QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > 130)
        return {};
    const QString id = QString::fromLatin1(file.read(131)).trimmed();
    static const QRegularExpression pattern(QStringLiteral("\\A[A-Za-z0-9_-]{8,128}\\z"));
    return pattern.match(id).hasMatch() ? id : QString{};
}

QString ObsidianSync::mirrorPath(const QString &path) const
{
    if (!markdownPath(path))
        return {};
    const QFileInfo root(QDir(m_directory).filePath(QStringLiteral("vault")));
    const QString canonicalRoot = root.canonicalFilePath();
    if (canonicalRoot.isEmpty() || root.isSymLink())
        return {};
    const QString absolute = QDir(canonicalRoot).filePath(path);
    const QFileInfo file(absolute);
    if (!file.isFile() || file.canonicalFilePath() != absolute)
        return {};
    return absolute;
}

ObsidianSync::Note ObsidianSync::readNote(const QString &path) const
{
    QFile file(mirrorPath(path));
    if (file.fileName().isEmpty() || !file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray bytes = file.read(Notebook::maximumMarkdownBytes + 1);
    if (bytes.size() > Notebook::maximumMarkdownBytes || file.error() != QFileDevice::NoError)
        return {};
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless | QStringDecoder::Flag::ConvertInitialBom);
    const QString text = decoder(bytes);
    if (decoder.hasError() || text.contains(QChar::Null))
        return {};
    return {text, digest(Notebook::normalizeMarkdownText(text))};
}

bool ObsidianSync::loadLinks()
{
    const QString id = vaultId();
    if (id.isEmpty())
        return false;
    if (m_loaded && id == m_vault)
        return true;
    m_loaded = false;
    m_links.clear();
    m_files.clear();
    m_notes.clear();
    QFile file(QDir(m_directory).filePath(QStringLiteral("quaderno-links.json")));
    if (QFileInfo(file).isSymLink())
        return false;
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly))
            return false;
        const QByteArray bytes = file.read(maximumIndexBytes + 1);
        const QJsonDocument index = QJsonDocument::fromJson(bytes);
        if (bytes.size() > maximumIndexBytes || !index.isObject() || index.object().value("vault").toString() != id
            || !index.object().value("links").isObject())
            return false;
        const QJsonObject entries = index.object().value("links").toObject();
        if (entries.size() > maximumNotes)
            return false;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            const auto entry = it.value().toObject();
            const Link link{entry.value("book").toString(), entry.value("hash").toString(), entry.value("revision").toString(),
                            entry.value("pendingHash").toString(), entry.value("pendingRevision").toString()};
            if (!markdownPath(it.key()) || !validBookId(link.bookId) || !hashValue(link.hash)
                || (!link.revision.isEmpty() && !hashValue(link.revision))
                || (link.pendingHash.isEmpty() != link.pendingRevision.isEmpty())
                || (!link.pendingHash.isEmpty() && (!hashValue(link.pendingHash) || !hashValue(link.pendingRevision))))
                return false;
            m_links.insert(it.key(), link);
        }
    }
    m_vault = id;
    m_loaded = true;
    return true;
}

bool ObsidianSync::saveLinks(const QHash<QString, Link> &links)
{
    if (!m_loaded || vaultId() != m_vault || links.size() > maximumNotes)
        return false;
    QJsonObject entries;
    for (auto it = links.cbegin(); it != links.cend(); ++it) {
        const auto &link = it.value();
        entries.insert(it.key(), QJsonObject{{"book", link.bookId}, {"hash", link.hash}, {"revision", link.revision},
                                             {"pendingHash", link.pendingHash}, {"pendingRevision", link.pendingRevision}});
    }
    QSaveFile file(QDir(m_directory).filePath(QStringLiteral("quaderno-links.json")));
    const auto bytes = QJsonDocument(QJsonObject{{"vault", m_vault}, {"links", entries}}).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumIndexBytes || !file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit())
        return false;
    m_links = links;
    return true;
}

void ObsidianSync::rebuildNotes(bool scan)
{
    if (scan) {
        m_files.clear();
        const QDir root(QDir(m_directory).filePath(QStringLiteral("vault")));
        QDirIterator files(root.absolutePath(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (files.hasNext() && m_files.size() < maximumNotes) {
            const QString path = root.relativeFilePath(files.next());
            if (!markdownPath(path))
                continue;
            const Note note = readNote(path);
            if (!note.hash.isEmpty())
                m_files.insert(path, note.hash);
        }
    }
    QStringList paths = m_files.keys();
    std::sort(paths.begin(), paths.end());
    QVariantList notes;
    for (const auto &path : std::as_const(paths)) {
        const auto link = m_links.value(path);
        const auto text = m_notebook->markdownTextForBook(link.bookId);
        const bool linked = !text.isNull();
        const QString localHash = linked ? digest(text) : QString{};
        const bool changed = linked && localHash != link.hash;
        const bool remoteChanged = linked && m_files.value(path) != link.hash;
        const bool conflict = (m_openConflict && m_openConflictPath == path)
            || (remoteChanged && m_files.value(path) != localHash
                && (changed || m_notebook->bookTransferRevision(link.bookId) != link.revision
                    || !m_notebook->canRefreshMarkdownBook(link.bookId)));
        notes.append(QVariantMap{{"path", path}, {"title", QFileInfo(path).completeBaseName()}, {"linked", linked},
                                 {"localChanged", changed}, {"conflict", conflict}});
    }
    m_notes = notes;
    emit changed();
}

void ObsidianSync::setLocked(bool locked)
{
    m_locked = locked;
    if (locked) {
        m_notes.clear();
        m_files.clear();
        m_openConflict = false;
        m_openConflictPath.clear();
        report({});
    } else {
        emit changed();
    }
}

void ObsidianSync::reload()
{
    if (!m_available || m_locked || busy())
        return;
    if (!installed()) {
        report({});
        return;
    }
    if (!loadLinks()) {
        report(tr("Impossibile leggere i collegamenti alle note Obsidian."));
        return;
    }
    m_openConflict = false;
    m_openConflictPath.clear();
    rebuildNotes(true);
}

void ObsidianSync::syncNow()
{
    if (m_locked || busy() || !installed())
        return;
    if (!loadLinks()) {
        report(tr("Impossibile leggere i collegamenti alle note Obsidian."));
        return;
    }
    run(Download);
}

void ObsidianSync::openNote(const QString &path, const QString &folderId)
{
    if (m_locked || busy() || !m_loaded || !m_files.contains(path))
        return;
    const Note note = readNote(path);
    if (note.hash.isEmpty()) {
        report(tr("Impossibile leggere la nota Markdown del vault."));
        return;
    }
    auto links = m_links;
    auto &link = links[path];
    if (!link.bookId.isEmpty() && m_notebook->hasBookId(link.bookId)) {
        const auto current = m_notebook->markdownTextForBook(link.bookId);
        if (current.isNull()) {
            report(tr("Ripristina il quaderno collegato dal cestino."));
            return;
        }
        if (note.hash != link.hash || link.revision.isEmpty()) {
            const QString revision = m_notebook->bookTransferRevision(link.bookId);
            if (note.hash != digest(current) && (revision != link.revision
                || !m_notebook->refreshMarkdownBook(link.bookId, note.text, revision))) {
                m_openConflict = true;
                m_openConflictPath = path;
                rebuildNotes(false);
                report(tr("La nota cloud e il quaderno collegato differiscono. Puoi importare una copia separata."));
                return;
            }
            link.hash = note.hash;
            link.revision = m_notebook->bookTransferRevision(link.bookId);
            link.pendingHash.clear();
            link.pendingRevision.clear();
            if (!saveLinks(links)) {
                report(tr("Impossibile salvare il collegamento alla nota Obsidian."));
                return;
            }
        }
    } else {
        link = {QUuid::createUuid().toString(QUuid::WithoutBraces), note.hash, {}, {}, {}};
        if (!saveLinks(links)) {
            report(tr("Impossibile salvare il collegamento alla nota Obsidian."));
            return;
        }
        if (!m_notebook->importLinkedMarkdown(note.text, QFileInfo(path).completeBaseName(), folderId, link.bookId)) {
            report(m_notebook->error());
            return;
        }
        link.revision = m_notebook->bookTransferRevision(link.bookId);
        if (!saveLinks(links)) {
            report(tr("Impossibile salvare il collegamento alla nota Obsidian."));
            return;
        }
    }
    rebuildNotes(false);
    emit opened(link.bookId);
}

void ObsidianSync::openCloudCopy(const QString &path, const QString &folderId)
{
    if (m_locked || busy() || !m_files.contains(path))
        return;
    const Note note = readNote(path);
    if (note.hash.isEmpty()) {
        report(tr("Impossibile leggere la nota Markdown del vault."));
        return;
    }
    const QString title = tr("%1 (copia Obsidian)").arg(QFileInfo(path).completeBaseName().left(60));
    if (!m_notebook->importMarkdownText(note.text, title, folderId)) {
        report(m_notebook->error());
        return;
    }
    emit opened(m_notebook->bookId());
}

void ObsidianSync::sendNote(const QString &path)
{
    if (m_locked || busy() || !m_loaded || !installed() || !m_files.contains(path))
        return;
    const auto link = m_links.value(path);
    m_uploadRevision = m_notebook->bookTransferRevision(link.bookId);
    if (m_uploadRevision.isEmpty())
        return;
    m_uploadPath = path;
    run(BeforeUpload);
}

void ObsidianSync::run(Phase phase)
{
    m_phase = phase;
    m_timedOut = false;
    m_complete = false;
    m_uploadConflict = false;
    m_output.clear();
    report(phase == Upload ? tr("Invio modifiche a Obsidian…") : tr("Sincronizzazione con Obsidian…"));
    m_timeout.start(5 * 60 * 1000);
    m_process.start(QDir(m_directory).filePath(QStringLiteral("ob-quaderno")), {QStringLiteral("sync")});
}

void ObsidianSync::finish(bool success)
{
    const Phase phase = m_phase;
    m_phase = Idle;
    m_timeout.stop();
    if (phase == Idle)
        return;
    if (m_locked) {
        emit changed();
        return;
    }
    if (!success || vaultId() != m_vault) {
        report(m_timedOut ? tr("La sincronizzazione con Obsidian è scaduta.")
                          : tr("Obsidian Sync non è riuscito. Verifica il Wi-Fi e l’accesso al vault."));
        return;
    }
    if (!reconcile()) {
        report(tr("Impossibile salvare il collegamento alla nota Obsidian."));
        return;
    }
    if (phase == BeforeUpload) {
        upload();
        return;
    }
    rebuildNotes(true);
    const auto link = m_links.value(m_uploadPath);
    if (phase == Upload && (m_uploadConflict || link.hash != m_files.value(m_uploadPath) || link.revision != m_uploadRevision))
        report(tr("Invio da verificare. La nota è cambiata anche in Obsidian."));
    else
        report(phase == Upload ? tr("Modifiche inviate a Obsidian.") : tr("Vault Obsidian sincronizzato."));
}

bool ObsidianSync::reconcile()
{
    auto links = m_links;
    bool changed = false;
    for (auto &link : links) {
        if (link.revision.isEmpty() && m_notebook->hasBookId(link.bookId)) {
            const auto text = m_notebook->markdownTextForBook(link.bookId);
            if (!text.isNull() && digest(text) == link.hash) {
                link.revision = m_notebook->bookTransferRevision(link.bookId);
                changed = true;
            }
        }
    }
    for (auto it = links.begin(); it != links.end(); ++it) {
        auto &link = it.value();
        if (link.pendingHash.isEmpty())
            continue;
        const auto note = readNote(it.key());
        if (note.hash == link.pendingHash) {
            link.hash = link.pendingHash;
            link.revision = link.pendingRevision;
        }
        link.pendingHash.clear();
        link.pendingRevision.clear();
        changed = true;
    }
    return !changed || saveLinks(links);
}

void ObsidianSync::upload()
{
    MirrorLock lock(m_directory);
    if (!lock.locked()) {
        report(tr("Un’altra sincronizzazione Obsidian è in corso."));
        return;
    }
    auto links = m_links;
    auto &link = links[m_uploadPath];
    const auto remote = readNote(m_uploadPath);
    const QString text = m_notebook->markdownTextForBook(link.bookId);
    if (text.isNull() || text.contains(QChar::Null) || !text.isValidUtf16()
        || text.toUtf8().size() > Notebook::maximumMarkdownBytes
        || m_notebook->bookTransferRevision(link.bookId) != m_uploadRevision || remote.hash.isEmpty()) {
        rebuildNotes(true);
        report(tr("Il quaderno o la nota cloud è cambiata. Riapri il pannello e riprova."));
        return;
    }
    const QString hash = digest(text);
    if (remote.hash == hash) {
        link.hash = hash;
        link.revision = m_uploadRevision;
        const bool saved = saveLinks(links);
        rebuildNotes(true);
        report(saved ? tr("Il testo coincide già con la nota Obsidian.")
                     : tr("Impossibile salvare il collegamento alla nota Obsidian."));
        return;
    }
    if (remote.hash != link.hash) {
        rebuildNotes(true);
        report(tr("La nota cloud e il quaderno collegato differiscono. Puoi importare una copia separata."));
        return;
    }
    link.pendingHash = hash;
    link.pendingRevision = m_uploadRevision;
    if (!saveLinks(links)) {
        report(tr("Impossibile salvare il collegamento alla nota Obsidian."));
        return;
    }
    const QString path = mirrorPath(m_uploadPath);
    QByteArray bytes = text.toUtf8();
    if (text.startsWith(QChar::ByteOrderMark))
        bytes.prepend("\xef\xbb\xbf");
    QSaveFile file(path);
    if (path.isEmpty() || readNote(m_uploadPath).hash != remote.hash || !file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        report(tr("Impossibile salvare la nota Markdown nel vault del Paper Pro."));
        return;
    }
    lock.release();
    run(Upload);
}

void ObsidianSync::report(const QString &message)
{
    m_status = message;
    emit changed();
}
