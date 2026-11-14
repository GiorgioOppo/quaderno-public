#include "epubimport.h"

#include <QCoreApplication>
#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QLibrary>
#include <QPainter>
#include <QScopeGuard>
#include <QSet>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QUrl>
#include <QXmlStreamReader>

#include <array>
#include <memory>
#include <sys/types.h>
#include <utility>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct archive;
struct archive_entry;

namespace {
constexpr qsizetype MiB = 1024 * 1024;
constexpr qsizetype maxArchiveBytes = 64 * MiB;
constexpr qsizetype maxMarkupBytes = 1 * MiB;
constexpr qsizetype maxTotalMarkupBytes = 16 * MiB;
constexpr qsizetype maxImageBytes = 30 * MiB;
constexpr int maxEntries = 5000;
constexpr int maxPages = 150;
constexpr int archiveOk = 0;
constexpr int archiveEof = 1;

const QString containerPath = QStringLiteral("META-INF/container.xml");
const QString encryptionPath = QStringLiteral("META-INF/encryption.xml");
const QString rightsPath = QStringLiteral("META-INF/rights.xml");
const QString fairPlayPath = QStringLiteral("META-INF/sinf.xml");
const QString xhtmlType = QStringLiteral("application/xhtml+xml");

// The tablet and macOS ship libarchive, but its development headers are not required.
class ZipReader
{
public:
    ZipReader()
    {
#if defined(Q_OS_MACOS)
        library.setFileName(QStringLiteral("/usr/lib/libarchive.2.dylib"));
#elif defined(Q_OS_LINUX)
        library.setFileNameAndVersion(QStringLiteral("archive"), 13);
#endif
    }

    bool available()
    {
        return library.load()
            && resolve(create, "archive_read_new")
            && resolve(useZip, "archive_read_support_format_zip")
            && resolve(open, "archive_read_open_memory")
            && resolve(next, "archive_read_next_header")
            && resolve(path, "archive_entry_pathname_utf8")
            && resolve(length, "archive_entry_size")
            && resolve(encrypted, "archive_entry_is_encrypted")
            && resolve(read, "archive_read_data")
            && resolve(skip, "archive_read_data_skip")
            && resolve(free, "archive_read_free");
    }

    bool extract(const QByteArray &zip, const QSet<QString> &wanted, QHash<QString, QByteArray> &files,
                 QString &error) const
    {
        const std::unique_ptr<archive, decltype(free)> reader(create(), free);
        if (!reader || useZip(reader.get()) != archiveOk
            || open(reader.get(), zip.constData(), size_t(zip.size())) != archiveOk) {
            error = QCoreApplication::translate("EpubImport", "Impossibile aprire l’archivio EPUB.");
            return false;
        }

        qsizetype total = 0;
        int count = 0;
        int status = archiveOk;
        archive_entry *entry = nullptr;
        std::array<char, 16384> buffer{};
        while ((status = next(reader.get(), &entry)) == archiveOk) {
            if (++count > maxEntries) {
                error = QCoreApplication::translate("EpubImport", "L’EPUB contiene troppi file.");
                return false;
            }
            if (encrypted(entry)) {
                error = QCoreApplication::translate("EpubImport", "L’EPUB è cifrato. Scegli un file senza DRM.");
                return false;
            }

            const QString name = QString::fromUtf8(path(entry));
            if (!wanted.contains(name)) {
                if (skip(reader.get()) != archiveOk)
                    break;
                continue;
            }

            if (files.contains(name)) {
                error = QCoreApplication::translate("EpubImport", "L’EPUB contiene riferimenti duplicati.");
                return false;
            }
            if (length(entry) > maxMarkupBytes) {
                error = QCoreApplication::translate("EpubImport", "Una sezione dell’EPUB supera il limite di importazione.");
                return false;
            }

            QByteArray data;
            ssize_t size = 0;
            while ((size = read(reader.get(), buffer.data(), buffer.size())) > 0) {
                total += size;
                if (data.size() + size > maxMarkupBytes || total > maxTotalMarkupBytes) {
                    error = QCoreApplication::translate("EpubImport", "Il testo dell’EPUB supera il limite di importazione.");
                    return false;
                }
                data.append(buffer.data(), size);
            }
            if (size < 0)
                break;
            files.insert(name, data);
        }
        if (status == archiveEof)
            return true;
        error = QCoreApplication::translate("EpubImport", "Impossibile leggere l’archivio EPUB.");
        return false;
    }

private:
    template<typename Function>
    bool resolve(Function &function, const char *symbol)
    {
        function = reinterpret_cast<Function>(library.resolve(symbol));
        return function != nullptr;
    }

    QLibrary library;
    archive *(*create)() = nullptr;
    int (*useZip)(archive *) = nullptr;
    int (*open)(archive *, const void *, size_t) = nullptr;
    int (*next)(archive *, archive_entry **) = nullptr;
    const char *(*path)(archive_entry *) = nullptr;
    qint64 (*length)(archive_entry *) = nullptr;
    int (*encrypted)(archive_entry *) = nullptr;
    ssize_t (*read)(archive *, void *, size_t) = nullptr;
    int (*skip)(archive *) = nullptr;
    int (*free)(archive *) = nullptr;
};

#ifdef Q_OS_UNIX
bool extractPackage(int root, const QSet<QString> &wanted, QHash<QString, QByteArray> &files, QString &error)
{
    qsizetype total = 0;
    for (const QString &name : wanted) {
        const auto components = name.split(u'/');
        int directory = root;
        const auto closeDirectory = qScopeGuard([&] { if (directory != root) ::close(directory); });
        int entry = -1;
        for (qsizetype index = 0; index < components.size(); ++index) {
            const QString &component = components.at(index);
            if (component.isEmpty() || component == u"." || component == u"..") {
                error = QCoreApplication::translate("EpubImport", "Impossibile leggere l’archivio EPUB.");
                return false;
            }
            const bool last = index == components.size() - 1;
            const int flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW | (last ? O_NONBLOCK : O_DIRECTORY);
            entry = ::openat(directory, QFile::encodeName(component).constData(), flags);
            if (entry < 0 || last)
                break;
            if (directory != root)
                ::close(directory);
            directory = entry;
        }
        if (entry < 0 && errno == ENOENT)
            continue;

        QFile file;
        struct stat entryInfo{};
        if (entry < 0 || ::fstat(entry, &entryInfo) != 0 || !S_ISREG(entryInfo.st_mode)
            || !file.open(entry, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            if (entry >= 0)
                ::close(entry);
            error = QCoreApplication::translate("EpubImport", "Impossibile leggere l’archivio EPUB.");
            return false;
        }
        const QByteArray data = file.read(maxMarkupBytes + 1);
        if (file.error() != QFileDevice::NoError) {
            error = QCoreApplication::translate("EpubImport", "Impossibile leggere l’archivio EPUB.");
            return false;
        }
        if (data.size() > maxMarkupBytes || (total += data.size()) > maxTotalMarkupBytes) {
            error = QCoreApplication::translate("EpubImport", "Il testo dell’EPUB supera il limite di importazione.");
            return false;
        }
        files.insert(name, data);
    }
    return true;
}
#endif

QString archivePath(const QString &base, const QString &reference)
{
    const QUrl url(reference);
    const QString relative = url.path(QUrl::FullyDecoded);
    if (!url.isValid() || !url.isRelative() || !url.authority().isEmpty() || url.hasQuery()
        || relative.isEmpty() || relative.startsWith(u'/') || relative.contains(u'\\')
        || relative.contains(QChar::Null))
        return {};

    const QString result = QDir::cleanPath(base + relative);
    if (result == QStringLiteral(".") || result == QStringLiteral("..") || result.startsWith(QStringLiteral("../")))
        return {};
    return result;
}

QString packagePath(const QByteArray &container)
{
    QXmlStreamReader xml(container);
    QString package;
    bool found = false;
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == u"rootfile" && !found) {
            found = true;
            package = archivePath({}, xml.attributes().value(u"full-path").toString());
        }
    }
    return xml.hasError() ? QString{} : package;
}

bool hasDrm(const QByteArray &encryption)
{
    if (encryption.isEmpty())
        return false;

    QXmlStreamReader xml(encryption);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == u"EncryptionMethod") {
            // EPUB may obfuscate embedded fonts even when the publication has no DRM.
            const auto algorithm = xml.attributes().value(u"Algorithm");
            if (algorithm != u"http://www.idpf.org/2008/embedding"
                && algorithm != u"http://ns.adobe.com/pdf/enc#RC")
                return true;
        }
    }
    return xml.hasError();
}

bool readingOrder(const QByteArray &package, const QString &packageFile, QList<QString> &chapters, QString &error)
{
    const QString base = packageFile.left(packageFile.lastIndexOf(u'/') + 1);
    QHash<QString, QString> manifest;
    QList<QString> spine;
    QXmlStreamReader xml(package);
    int depth = 0;
    int manifestDepth = -1;
    int spineDepth = -1;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const auto attributes = xml.attributes();
            if (++depth > 64)
                return false;
            if (depth == 2 && xml.name() == u"manifest") {
                manifestDepth = depth;
            } else if (depth == 2 && xml.name() == u"spine") {
                spineDepth = depth;
            } else if (manifestDepth >= 0 && depth == manifestDepth + 1 && xml.name() == u"item") {
                const auto id = attributes.value(u"id").toString();
                if (id.isEmpty() || manifest.contains(id))
                    return false;
                const auto path = attributes.value(u"media-type") == xhtmlType
                    ? archivePath(base, attributes.value(u"href").toString()) : QString{};
                manifest.insert(id, path);
            } else if (spineDepth >= 0 && depth == spineDepth + 1 && xml.name() == u"itemref") {
                if (attributes.value(u"linear") != u"no") {
                    if (attributes.value(u"properties").toString().split(u' ', Qt::SkipEmptyParts)
                            .contains(QStringLiteral("rendition:layout-pre-paginated"))) {
                        error = QCoreApplication::translate("EpubImport", "Questo EPUB usa pagine a impaginazione fissa.");
                        return false;
                    }
                    spine.append(attributes.value(u"idref").toString());
                }
            } else if (xml.name() == u"meta" && attributes.value(u"property") == u"rendition:layout") {
                if (xml.readElementText().trimmed() == QStringLiteral("pre-paginated")) {
                    error = QCoreApplication::translate("EpubImport", "Questo EPUB usa pagine a impaginazione fissa.");
                    return false;
                }
                --depth;
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (depth == manifestDepth)
                manifestDepth = -1;
            if (depth == spineDepth)
                spineDepth = -1;
            --depth;
        }
    }
    if (xml.hasError() || spine.isEmpty())
        return false;
    if (spine.size() > maxPages) {
        error = QCoreApplication::translate("EpubImport", "L’EPUB supera il limite di 150 pagine importate.");
        return false;
    }

    for (const QString &id : spine) {
        const QString chapter = manifest.value(id);
        if (chapter.isEmpty()) {
            error = QCoreApplication::translate("EpubImport", "L’EPUB contiene una sezione priva di testo XHTML leggibile.");
            return false;
        }
        chapters.append(chapter);
    }
    return true;
}

class NamedEntities : public QXmlStreamEntityResolver
{
    QString resolveUndeclaredEntity(const QString &name) override
    {
        if (name.size() > 32)
            return {};

        auto previous = replacements.constFind(name);
        if (previous != replacements.cend())
            return *previous;

        // XML names cannot contain markup. Decode only the single XHTML entity through Qt's HTML parser.
        const QString entity = u'&' + name + u';';
        const QString decoded = QTextDocumentFragment::fromHtml(entity).toRawText();
        const QString result = decoded == entity ? QString{} : decoded;
        if (replacements.size() < 512)
            replacements.insert(name, result);
        return result;
    }

    QHash<QString, QString> replacements;
};

bool safeChapterHtml(const QByteArray &xhtml, QString &html)
{
    static const QSet<QString> tags = {
        QStringLiteral("p"), QStringLiteral("div"), QStringLiteral("blockquote"), QStringLiteral("pre"),
        QStringLiteral("h1"), QStringLiteral("h2"), QStringLiteral("h3"), QStringLiteral("h4"),
        QStringLiteral("h5"), QStringLiteral("h6"), QStringLiteral("ol"), QStringLiteral("ul"),
        QStringLiteral("li"), QStringLiteral("b"), QStringLiteral("strong"), QStringLiteral("i"),
        QStringLiteral("em"), QStringLiteral("u"), QStringLiteral("s"), QStringLiteral("sub"), QStringLiteral("sup")
    };
    static const QSet<QString> omitted = {
        QStringLiteral("script"), QStringLiteral("style"), QStringLiteral("svg"), QStringLiteral("math"),
        QStringLiteral("audio"), QStringLiteral("video"), QStringLiteral("iframe"), QStringLiteral("object")
    };

    NamedEntities entities;
    QXmlStreamReader xml(xhtml);
    xml.setEntityResolver(&entities);
    QString title;
    bool heading = false;
    bool inBody = false;
    int depth = 0;
    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartElement: {
            if (++depth > 64)
                return false;
            const QString tag = xml.name().toString().toLower();
            if (tag == QStringLiteral("body")) {
                inBody = true;
            } else if (!inBody && tag == QStringLiteral("title")) {
                title = xml.readElementText(QXmlStreamReader::SkipChildElements).simplified();
                --depth;
            } else if (inBody) {
                if (omitted.contains(tag)) {
                    xml.skipCurrentElement();
                    --depth;
                } else if (tags.contains(tag)) {
                    heading = heading || (tag.size() == 2 && tag.startsWith(u'h'));
                    html += u'<' + tag + u'>';
                } else if (tag == QStringLiteral("br") || tag == QStringLiteral("hr")) {
                    html += u'<' + tag + QStringLiteral("/>");
                } else if (tag == QStringLiteral("img")) {
                    html += xml.attributes().value(u"alt").toString().toHtmlEscaped();
                }
            }
            break;
        }
        case QXmlStreamReader::EndElement: {
            const QString tag = xml.name().toString().toLower();
            if (tag == QStringLiteral("body"))
                inBody = false;
            else if (inBody && tags.contains(tag))
                html += QStringLiteral("</") + tag + u'>';
            --depth;
            break;
        }
        case QXmlStreamReader::Characters:
            if (inBody)
                html += xml.text().toString().toHtmlEscaped();
            break;
        default:
            break;
        }
        if (html.size() > maxMarkupBytes)
            return false;
    }
    if (!heading && !title.isEmpty() && !html.trimmed().isEmpty())
        html.prepend(QStringLiteral("<h2>") + title.toHtmlEscaped() + QStringLiteral("</h2>"));
    return !xml.hasError();
}

class OfflineTextDocument : public QTextDocument
{
    QVariant loadResource(int, const QUrl &) override { return {}; }
};

bool renderChapter(const QString &html, QList<QByteArray> &pages, qsizetype &total, QString &error)
{
    constexpr QSize pageSize(900, 1200);
    constexpr QRect content(70, 74, 760, 1052);
    OfflineTextDocument text;
    QFont font;
    font.setStyleHint(QFont::Serif);
    font.setPixelSize(29);
    text.setDefaultFont(font);
    text.setDocumentMargin(0);
    text.setDefaultStyleSheet(QStringLiteral("body { color: #252528; } p { margin-bottom: 13px; }"));
    text.setHtml(html);
    if (text.toPlainText().trimmed().isEmpty())
        return true;
    text.setPageSize(content.size());
    const int count = text.pageCount();
    if (count > maxPages - pages.size()) {
        error = QCoreApplication::translate("EpubImport", "L’EPUB supera il limite di 150 pagine importate.");
        return false;
    }

    for (int page = 0; page < count; ++page) {
        QImage image(pageSize, QImage::Format_RGB32);
        image.fill(Qt::white);
        const int top = page * content.height();
        QPainter painter(&image);
        painter.setClipRect(content);
        painter.translate(content.x(), content.y() - top);
        QAbstractTextDocumentLayout::PaintContext context;
        context.clip = QRectF(0, top, content.width(), content.height());
        text.documentLayout()->draw(&painter, context);
        painter.end();

        QByteArray data;
        QBuffer output(&data);
        output.open(QIODevice::WriteOnly);
        if (!image.save(&output, "JPEG", 85)) {
            error = QCoreApplication::translate("EpubImport", "Impossibile preparare una pagina EPUB.");
            return false;
        }
        total += data.size();
        if (total > maxImageBytes) {
            error = QCoreApplication::translate("EpubImport", "Le pagine EPUB superano il limite di 30 MB.");
            return false;
        }
        pages.append(std::move(data));
    }
    return true;
}
}

QList<QByteArray> renderEpubPages(const QString &path, QString &error)
{
    error.clear();
    const QFileInfo info(path);
    QByteArray zip;
    ZipReader reader;
#ifdef Q_OS_UNIX
    const bool package = info.isDir();
    const int root = package ? ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW) : -1;
    const auto closeRoot = qScopeGuard([root] { if (root >= 0) ::close(root); });
    if (package && root < 0) {
        error = QCoreApplication::translate("EpubImport", "Impossibile leggere l’archivio EPUB.");
        return {};
    }
    if (!package)
#endif
    {
        QFile input(path);
        if (!info.isFile() || !input.open(QIODevice::ReadOnly) || input.size() <= 0 || input.size() > maxArchiveBytes) {
            error = QCoreApplication::translate("EpubImport", "Scegli un file EPUB leggibile da meno di 64 MB.");
            return {};
        }
        zip = input.read(maxArchiveBytes + 1);
        if (zip.isEmpty() || zip.size() > maxArchiveBytes || input.error() != QFileDevice::NoError) {
            error = QCoreApplication::translate("EpubImport", "Impossibile leggere il file EPUB.");
            return {};
        }
        if (!reader.available()) {
            error = QCoreApplication::translate("EpubImport", "La libreria di sistema per leggere gli EPUB non è disponibile.");
            return {};
        }
    }

    QHash<QString, QByteArray> files;
    const auto extract = [&](const QSet<QString> &wanted) {
        files.clear();
#ifdef Q_OS_UNIX
        if (root >= 0)
            return extractPackage(root, wanted, files, error);
#endif
        return reader.extract(zip, wanted, files, error);
    };
    if (!extract({containerPath, encryptionPath, rightsPath, fairPlayPath}))
        return {};
    if (files.contains(rightsPath) || files.contains(fairPlayPath) || hasDrm(files.value(encryptionPath))) {
        error = QCoreApplication::translate("EpubImport", "L’EPUB è protetto. Scegli un file senza DRM.");
        return {};
    }

    const QString packageFile = packagePath(files.value(containerPath));
    if (packageFile.isEmpty()) {
        error = QCoreApplication::translate("EpubImport", "Impossibile trovare il contenuto dell’EPUB.");
        return {};
    }
    if (!extract({packageFile}))
        return {};
    QList<QString> chapters;
    if (!readingOrder(files.value(packageFile), packageFile, chapters, error)) {
        if (error.isEmpty())
            error = QCoreApplication::translate("EpubImport", "L’indice dell’EPUB non è leggibile.");
        return {};
    }

    if (!extract(QSet<QString>(chapters.cbegin(), chapters.cend())))
        return {};
    QList<QByteArray> pages;
    qsizetype total = 0;
    for (const QString &chapter : std::as_const(chapters)) {
        QString html;
        if (!safeChapterHtml(files.value(chapter), html)) {
            error = QCoreApplication::translate("EpubImport", "Una sezione XHTML dell’EPUB non è leggibile.");
            return {};
        }
        if (!renderChapter(html, pages, total, error))
            return {};
    }
    if (pages.isEmpty())
        error = QCoreApplication::translate("EpubImport", "L’EPUB non contiene testo importabile.");
    return pages;
}
