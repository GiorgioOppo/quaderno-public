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
#include <QImageReader>
#include <QPageSize>
#include <QPageLayout>
#include <QPdfWriter>
#include <QDirIterator>
#include <QtEndian>
#include <zlib.h>
#include <QLibrary>
#include <QPainter>
#include <QScopeGuard>
#include <QSet>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QUrl>
#include <QXmlStreamReader>

#include <array>
#include <cmath>
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
constexpr int maxPages = 100000;
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
                 QString &error, qsizetype entryLimit = maxMarkupBytes, qsizetype totalLimit = maxTotalMarkupBytes) const
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
            if (length(entry) > entryLimit) {
                error = QCoreApplication::translate("EpubImport", "Una sezione dell’EPUB supera il limite di importazione.");
                return false;
            }

            QByteArray data;
            ssize_t size = 0;
            while ((size = read(reader.get(), buffer.data(), buffer.size())) > 0) {
                total += size;
                if (data.size() + size > entryLimit || total > totalLimit) {
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
        error = QCoreApplication::translate("EpubImport", "L’EPUB supera il limite di 100000 pagine importate.");
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

bool safeChapterHtml(const QByteArray &xhtml, QString &html, const QString &base = {},
                     const QHash<QString, QImage> &images = {})
{
    static const QSet<QString> tags = {
        QStringLiteral("p"), QStringLiteral("div"), QStringLiteral("blockquote"), QStringLiteral("pre"),
        QStringLiteral("h1"), QStringLiteral("h2"), QStringLiteral("h3"), QStringLiteral("h4"),
        QStringLiteral("h5"), QStringLiteral("h6"), QStringLiteral("ol"), QStringLiteral("ul"),
        QStringLiteral("li"), QStringLiteral("table"), QStringLiteral("tr"), QStringLiteral("td"), QStringLiteral("th"), QStringLiteral("b"), QStringLiteral("strong"), QStringLiteral("i"),
        QStringLiteral("em"), QStringLiteral("u"), QStringLiteral("s"), QStringLiteral("sub"), QStringLiteral("sup")
    };
    static const QSet<QString> omitted = {
        QStringLiteral("script"), QStringLiteral("style"), QStringLiteral("math"),
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
                } else if (tag == QStringLiteral("img") || tag == QStringLiteral("image")) {
                    QString reference = xml.attributes().value(u"src").toString();
                    if (reference.isEmpty())
                        reference = xml.attributes().value(u"href").toString();
                    if (reference.isEmpty())
                        reference = xml.attributes().value(QStringLiteral("http://www.w3.org/1999/xlink"), QStringLiteral("href")).toString();
                    const QString path = archivePath(base, reference);
                    const QImage image = images.value(path);
                    if (!image.isNull()) {
                        const QSize size = image.size().scaled(QSize(760, 1000), Qt::KeepAspectRatio);
                        const QString resource = QStringLiteral("quaderno:/") + QString::fromLatin1(QUrl::toPercentEncoding(path, "/"));
                        html += QStringLiteral("<img src=\"%1\" width=\"%2\" height=\"%3\"/>")
                            .arg(resource.toHtmlEscaped()).arg(size.width()).arg(size.height());
                    } else {
                        const QString alt = xml.attributes().value(u"alt").toString();
                        html += alt.isEmpty() ? QStringLiteral("[Immagine non disponibile]") : alt.toHtmlEscaped();
                    }
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

QSet<QString> chapterImages(const QByteArray &xhtml, const QString &base)
{
    QSet<QString> paths;
    QXmlStreamReader xml(xhtml);
    NamedEntities entities;
    xml.setEntityResolver(&entities);
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (xml.name() != u"img" && xml.name() != u"image")
            continue;
        QString reference = xml.attributes().value(u"src").toString();
        if (reference.isEmpty())
            reference = xml.attributes().value(u"href").toString();
        if (reference.isEmpty())
            reference = xml.attributes().value(QStringLiteral("http://www.w3.org/1999/xlink"), QStringLiteral("href")).toString();
        const QString path = archivePath(base, reference);
        if (!path.isEmpty())
            paths.insert(path);
    }
    return paths;
}

void append16(QByteArray &bytes, quint16 value)
{
    const quint16 little = qToLittleEndian(value);
    bytes.append(reinterpret_cast<const char *>(&little), sizeof little);
}
void append32(QByteArray &bytes, quint32 value)
{
    const quint32 little = qToLittleEndian(value);
    bytes.append(reinterpret_cast<const char *>(&little), sizeof little);
}
}

QByteArray epubSourceBytes(const QString &path, QString &error)
{
    const QFileInfo info(path);
    if (info.isFile()) {
        QFile input(path);
        if (!input.open(QIODevice::ReadOnly) || input.size() < 1 || input.size() > maxArchiveBytes) {
            error = QCoreApplication::translate("EpubImport", "Scegli un file EPUB leggibile da meno di 64 MB.");
            return {};
        }
        const QByteArray data = input.read(maxArchiveBytes + 1);
        if (data.isEmpty() || data.size() > maxArchiveBytes || input.error() != QFileDevice::NoError) {
            error = QCoreApplication::translate("EpubImport", "Impossibile leggere il file EPUB.");
            return {};
        }
        return data;
    }
    if (!info.isDir() || info.isSymLink()) {
        error = QCoreApplication::translate("EpubImport", "Scegli un file EPUB leggibile.");
        return {};
    }
    // Apple Books can supply an unpacked EPUB. Retain all regular files in a
    // standards-compatible stored ZIP, without following links outside it.
    const QDir root(info.canonicalFilePath());
    QStringList names;
    QDirIterator entries(root.path(), QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        entries.next();
        const auto entry = entries.fileInfo();
        if (entry.isSymLink() || (!entry.isDir() && !entry.isFile())) {
            error = QCoreApplication::translate("EpubImport", "Il pacchetto EPUB contiene un collegamento non supportato.");
            return {};
        }
        if (entry.isFile())
            names.append(root.relativeFilePath(entry.absoluteFilePath()));
        if (names.size() > maxEntries) {
            error = QCoreApplication::translate("EpubImport", "L’EPUB contiene troppi file.");
            return {};
        }
    }
    names.sort();
    names.removeAll(QStringLiteral("mimetype"));
    names.prepend(QStringLiteral("mimetype"));
    QByteArray result;
    QByteArray central;
    for (const QString &name : std::as_const(names)) {
        QFile input(root.filePath(name));
        if (!input.open(QIODevice::ReadOnly) || input.size() < 0 || input.size() > maxArchiveBytes) {
            error = QCoreApplication::translate("EpubImport", "Impossibile leggere il pacchetto EPUB.");
            return {};
        }
        const QByteArray data = input.read(maxArchiveBytes + 1);
        const QByteArray encodedName = name.toUtf8();
        if (data.size() > maxArchiveBytes || input.error() != QFileDevice::NoError || encodedName.size() > 65535
            || result.size() + central.size() + data.size() + encodedName.size() * 2 + 100 > maxArchiveBytes) {
            error = QCoreApplication::translate("EpubImport", "Il pacchetto EPUB supera il limite di 64 MB.");
            return {};
        }
        const quint32 offset = result.size();
        const quint32 checksum = crc32(0, reinterpret_cast<const Bytef *>(data.constData()), data.size());
        append32(result, 0x04034b50); append16(result, 20); append16(result, 0x0800);
        append16(result, 0); append16(result, 0); append16(result, 0);
        append32(result, checksum); append32(result, data.size()); append32(result, data.size());
        append16(result, encodedName.size()); append16(result, 0);
        result += encodedName; result += data;
        append32(central, 0x02014b50); append16(central, 20); append16(central, 20); append16(central, 0x0800);
        append16(central, 0); append16(central, 0); append16(central, 0);
        append32(central, checksum); append32(central, data.size()); append32(central, data.size());
        append16(central, encodedName.size()); append16(central, 0); append16(central, 0);
        append16(central, 0); append16(central, 0); append32(central, 0); append32(central, offset);
        central += encodedName;
    }
    const quint32 centralOffset = result.size();
    result += central;
    append32(result, 0x06054b50); append16(result, 0); append16(result, 0);
    append16(result, names.size()); append16(result, names.size());
    append32(result, central.size()); append32(result, centralOffset); append16(result, 0);
    return result;
}

QByteArray convertEpubToPdf(const QByteArray &source, QString &error,
                            const DocumentCancellation &cancel, const DocumentProgress &progress)
{
    error.clear();
    if (source.isEmpty() || source.size() > maxArchiveBytes) {
        error = QCoreApplication::translate("EpubImport", "Scegli un file EPUB leggibile da meno di 64 MB.");
        return {};
    }
    ZipReader reader;
    if (!reader.available()) {
        error = QCoreApplication::translate("EpubImport", "La libreria di sistema per leggere gli EPUB non è disponibile.");
        return {};
    }
    QHash<QString, QByteArray> files;
    if (!reader.extract(source, {containerPath, encryptionPath, rightsPath, fairPlayPath}, files, error))
        return {};
    if (files.contains(rightsPath) || files.contains(fairPlayPath) || hasDrm(files.value(encryptionPath))) {
        error = QCoreApplication::translate("EpubImport", "L’EPUB è protetto. Scegli un file senza DRM.");
        return {};
    }
    const QString packageFile = packagePath(files.value(containerPath));
    files.clear();
    if (packageFile.isEmpty() || !reader.extract(source, {packageFile}, files, error)) {
        if (error.isEmpty())
            error = QCoreApplication::translate("EpubImport", "Impossibile trovare il contenuto dell’EPUB.");
        return {};
    }
    QList<QString> chapters;
    if (!readingOrder(files.value(packageFile), packageFile, chapters, error)) {
        if (error.isEmpty())
            error = QCoreApplication::translate("EpubImport", "L’indice dell’EPUB non è leggibile.");
        return {};
    }
    files.clear();
    if (!reader.extract(source, QSet<QString>(chapters.cbegin(), chapters.cend()), files, error))
        return {};

    QByteArray pdf;
    QBuffer output(&pdf);
    output.open(QIODevice::WriteOnly);
    int pages = 0;
    {
        QPdfWriter writer(&output);
        writer.setResolution(72);
        writer.setPageSize(QPageSize(QSizeF(900, 1200), QPageSize::Point));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);
        QPainter painter(&writer);
        if (!painter.isActive()) {
            error = QCoreApplication::translate("EpubImport", "Impossibile preparare il documento EPUB.");
            return {};
        }
        constexpr QRect content(70, 74, 760, 1052);
        for (qsizetype chapterIndex = 0; chapterIndex < chapters.size(); ++chapterIndex) {
            if (cancel && cancel->load())
                return {};
            const QString chapter = chapters.at(chapterIndex);
            const QString base = chapter.left(chapter.lastIndexOf(u'/') + 1);
            const QByteArray markup = files.value(chapter);
            const QSet<QString> references = chapterImages(markup, base);
            QHash<QString, QByteArray> imageFiles;
            if (!references.isEmpty() && !reader.extract(source, references, imageFiles, error, maxImageBytes, maxImageBytes))
                return {};
            QHash<QString, QImage> images;
            for (auto it = imageFiles.cbegin(); it != imageFiles.cend(); ++it) {
                if (cancel && cancel->load())
                    return {};
                QBuffer input;
                input.setData(it.value());
                input.open(QIODevice::ReadOnly);
                QImageReader decoder(&input);
                const QSize size = decoder.size();
                if (!size.isValid() || size.width() > 50000 || size.height() > 50000)
                    continue;
                decoder.setAutoTransform(true);
                const int imageEdge = qBound(128, int(std::sqrt((32.0 * MiB) / (4.0 * qMax(1, references.size())))), 2048);
                decoder.setScaledSize(size.scaled(QSize(imageEdge, imageEdge), Qt::KeepAspectRatio));
                const QImage image = decoder.read();
                if (!image.isNull())
                    images.insert(it.key(), image);
            }
            QString html;
            if (!safeChapterHtml(markup, html, base, images)) {
                error = QCoreApplication::translate("EpubImport", "Una sezione XHTML dell’EPUB non è leggibile.");
                return {};
            }
            if (html.trimmed().isEmpty())
                continue;
            OfflineTextDocument text;
            QFont font;
            font.setStyleHint(QFont::Serif);
            font.setPixelSize(29);
            text.setDefaultFont(font);
            text.setDocumentMargin(0);
            text.setDefaultStyleSheet(QStringLiteral("body { color: #252528; } p { margin-bottom: 13px; }"));
            for (auto it = images.cbegin(); it != images.cend(); ++it) {
                const QUrl resource(QStringLiteral("quaderno:/") + QString::fromLatin1(QUrl::toPercentEncoding(it.key(), "/")));
                text.addResource(QTextDocument::ImageResource, resource, it.value());
            }
            text.setHtml(html);
            text.setPageSize(content.size());
            const int count = text.pageCount();
            if (count < 1 || count > maxPages - pages) {
                error = QCoreApplication::translate("EpubImport", "L’EPUB supera il limite di 100000 pagine importate.");
                return {};
            }
            for (int page = 0; page < count; ++page) {
                if (cancel && cancel->load())
                    return {};
                if (pages++ && !writer.newPage()) {
                    error = QCoreApplication::translate("EpubImport", "Impossibile preparare una pagina EPUB.");
                    return {};
                }
                painter.save();
                painter.fillRect(QRect(0, 0, 900, 1200), Qt::white);
                painter.setClipRect(content);
                painter.translate(content.x(), content.y() - page * content.height());
                QAbstractTextDocumentLayout::PaintContext context;
                context.palette.setColor(QPalette::Text, QColor(37, 37, 40));
                context.clip = QRectF(0, page * content.height(), content.width(), content.height());
                text.documentLayout()->draw(&painter, context);
                painter.restore();
                if (pdf.size() > 48 * MiB) {
                    error = QCoreApplication::translate("EpubImport", "Il documento EPUB preparato supera il limite di 48 MB.");
                    return {};
                }
            }
            if (progress)
                progress(chapterIndex + 1, chapters.size());
        }
        painter.end();
    }
    if (pages == 0 || pdf.size() > 48 * MiB) {
        error = QCoreApplication::translate("EpubImport", "L’EPUB non contiene pagine importabili o supera il limite di dimensione.");
        return {};
    }
    return pdf;
}

QList<QByteArray> renderEpubPages(const QString &path, QString &error)
{
    const QByteArray source = epubSourceBytes(path, error);
    if (source.isEmpty())
        return {};
    const QByteArray pdf = convertEpubToPdf(source, error);
    return pdf.isEmpty() ? QList<QByteArray>{} : renderPdfData(pdf, error);
}
