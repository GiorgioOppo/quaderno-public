#include "pdfimport.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QPainter>

#ifdef Q_OS_MACOS
#include "pdfimport_mac.h"
#else
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#endif

#include <cmath>
#include <memory>
#include <utility>

namespace {
constexpr qint64 maxPdfSize = 48 * 1024 * 1024;
constexpr qint64 maxRenderedSize = 30 * 1024 * 1024;
constexpr int maxPages = 150;
constexpr int maxImageDimension = 1200;
constexpr qsizetype maxTextPerPage = 20'000;
constexpr qsizetype maxTextPerDocument = 750'000;

QSize rasterSize(double width, double height)
{
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        return {};
    const double longEdge = qMax(width, height);
    return {qMax(1, qRound((width / longEdge) * maxImageDimension)),
            qMax(1, qRound((height / longEdge) * maxImageDimension))};
}

bool checkPageCount(qsizetype count, QString &error)
{
    if (count >= 1 && count <= maxPages)
        return true;
    error = count > maxPages ? QCoreApplication::translate("PdfImport", "Il PDF supera il limite di 150 pagine.")
                             : QCoreApplication::translate("PdfImport", "Il PDF non contiene pagine leggibili.");
    return false;
}

bool storePage(const QImage &image, qsizetype index, QList<QByteArray> &pages,
               qint64 &renderedSize, QString &error)
{
    QByteArray data;
    QBuffer buffer(&data);
    QImageWriter writer(&buffer, "jpeg");
    writer.setQuality(86);
    if (image.isNull() || !buffer.open(QIODevice::WriteOnly) || !writer.write(image)) {
        error = QCoreApplication::translate("PdfImport", "Impossibile preparare la pagina %1 del PDF.").arg(index + 1);
        return false;
    }
    renderedSize += data.size();
    if (renderedSize > maxRenderedSize) {
        error = QCoreApplication::translate("PdfImport", "Le pagine preparate del PDF superano 30 MB.");
        return false;
    }
    pages.append(std::move(data));
    return true;
}

void storeText(ImportedPdf &pdf, QString text, qsizetype &remaining)
{
    text.remove(QChar(u'\0'));
    text.remove(QChar(u'\u00ad'));
    text = text.simplified().normalized(QString::NormalizationForm_C);
    text.truncate(qMin(maxTextPerPage, remaining));
    if (!text.isEmpty() && text.back().isHighSurrogate())
        text.chop(1);
    remaining -= text.size();
    pdf.pageText.append(std::move(text));
}

#ifdef Q_OS_MACOS
QImage drawPage(CGPDFPageRef page, CGColorSpaceRef colorSpace)
{
    if (!page)
        return {};

    const CGRect crop = CGRectIntersection(CGPDFPageGetBoxRect(page, kCGPDFCropBox),
                                           CGPDFPageGetBoxRect(page, kCGPDFMediaBox));
    const bool quarterTurn = CGPDFPageGetRotationAngle(page) % 180 != 0;
    const double width = quarterTurn ? crop.size.height : crop.size.width;
    const double height = quarterTurn ? crop.size.width : crop.size.height;
    const QSize size = rasterSize(width, height);
    if (size.isEmpty())
        return {};

    QImage image(size, QImage::Format_RGB32);
    if (image.isNull())
        return {};
    image.fill(Qt::white);

    const auto flags = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host) | kCGImageAlphaNoneSkipFirst;
    const std::unique_ptr<CGContext, decltype(&CGContextRelease)> context(
        CGBitmapContextCreate(image.bits(), size.width(), size.height(), 8,
                              image.bytesPerLine(), colorSpace, flags), CGContextRelease);
    if (!context)
        return {};

    const double scale = qMin(size.width() / width, size.height() / height);
    CGContextTranslateCTM(context.get(), (size.width() - width * scale) / 2,
                          (size.height() - height * scale) / 2);
    CGContextScaleCTM(context.get(), scale, scale);
    const CGRect bounds = CGRectMake(0, 0, width, height);
    const CGAffineTransform transform = CGPDFPageGetDrawingTransform(page, kCGPDFCropBox, bounds, 0, true);
    CGContextConcatCTM(context.get(), transform);
    CGContextClipToRect(context.get(), crop);
    CGContextDrawPDFPage(context.get(), page);
    CGContextFlush(context.get());
    return image;
}

ImportedPdf rasterize(const QByteArray &data, QString &error, bool withText)
{
    if (withText)
        return pdfImportMac::importData(data, error);

    const std::unique_ptr<CGDataProvider, decltype(&CGDataProviderRelease)> provider(
        CGDataProviderCreateWithData(nullptr, data.constData(), data.size(), nullptr), CGDataProviderRelease);
    const std::unique_ptr<CGPDFDocument, decltype(&CGPDFDocumentRelease)> document(
        provider ? CGPDFDocumentCreateWithProvider(provider.get()) : nullptr, CGPDFDocumentRelease);
    return pdfImportMac::renderDocument(document.get(), error);
}
}

ImportedPdf pdfImportMac::renderDocument(CGPDFDocumentRef document, QString &error, const PageText &readText)
{
    if (!document) {
        error = QCoreApplication::translate("PdfImport", "Impossibile leggere il documento PDF.");
        return {};
    }
    if (!CGPDFDocumentIsUnlocked(document)) {
        error = QCoreApplication::translate("PdfImport", "Il PDF richiede una password.");
        return {};
    }

    const qsizetype count = CGPDFDocumentGetNumberOfPages(document);
    if (!checkPageCount(count, error))
        return {};
    const std::unique_ptr<CGColorSpace, decltype(&CGColorSpaceRelease)> colorSpace(
        CGColorSpaceCreateDeviceRGB(), CGColorSpaceRelease);
    if (!colorSpace) {
        error = QCoreApplication::translate("PdfImport", "Impossibile preparare i colori del PDF.");
        return {};
    }

    ImportedPdf result;
    result.pages.reserve(count);
    result.pageText.reserve(count);
    qint64 renderedSize = 0;
    qsizetype remainingText = maxTextPerDocument;
    for (qsizetype index = 0; index < count; ++index) {
        const auto page = CGPDFDocumentGetPage(document, index + 1);
        if (!storePage(drawPage(page, colorSpace.get()), index, result.pages, renderedSize, error))
            return {};
        const qsizetype limit = qMin(remainingText, maxTextPerPage);
        storeText(result, readText && limit ? readText(index, limit) : QString{}, remainingText);
    }
    return result;
}
namespace {
#else
// Types match the PDFium public C API. Ferrari supplies libpdfium at runtime.
using PdfDocument = struct fpdf_document_t__ *;
using PdfPage = struct fpdf_page_t__ *;
using PdfBitmap = struct fpdf_bitmap_t__ *;
using PdfText = struct fpdf_textpage_t__ *;

struct PdfRectangle {
    float left;
    float top;
    float right;
    float bottom;
};

struct Pdfium {
    explicit Pdfium(QLibrary &lib)
        : init(load<decltype(init)>(lib, "FPDF_InitLibrary")),
          destroy(load<decltype(destroy)>(lib, "FPDF_DestroyLibrary")),
          loadDocument(load<decltype(loadDocument)>(lib, "FPDF_LoadMemDocument")),
          closeDocument(load<decltype(closeDocument)>(lib, "FPDF_CloseDocument")),
          lastError(load<decltype(lastError)>(lib, "FPDF_GetLastError")),
          pageCount(load<decltype(pageCount)>(lib, "FPDF_GetPageCount")),
          loadPage(load<decltype(loadPage)>(lib, "FPDF_LoadPage")),
          closePage(load<decltype(closePage)>(lib, "FPDF_ClosePage")),
          pageWidth(load<decltype(pageWidth)>(lib, "FPDF_GetPageWidthF")),
          pageHeight(load<decltype(pageHeight)>(lib, "FPDF_GetPageHeightF")),
          createBitmap(load<decltype(createBitmap)>(lib, "FPDFBitmap_CreateEx")),
          destroyBitmap(load<decltype(destroyBitmap)>(lib, "FPDFBitmap_Destroy")),
          render(load<decltype(render)>(lib, "FPDF_RenderPageBitmap")),
          permissions(load<decltype(permissions)>(lib, "FPDF_GetDocPermissions")),
          pageBounds(load<decltype(pageBounds)>(lib, "FPDF_GetPageBoundingBox")),
          loadText(load<decltype(loadText)>(lib, "FPDFText_LoadPage")),
          closeText(load<decltype(closeText)>(lib, "FPDFText_ClosePage")),
          getText(load<decltype(getText)>(lib, "FPDFText_GetBoundedText"))
    {
        if (valid())
            init();
    }

    ~Pdfium()
    {
        if (valid())
            destroy();
    }

    bool valid() const
    {
        return init && destroy && loadDocument && closeDocument && lastError && pageCount && loadPage
            && closePage && pageWidth && pageHeight && createBitmap && destroyBitmap && render;
    }

    bool canExtract(PdfDocument document) const
    {
        constexpr unsigned long copyPermission = 0x10;
        return permissions && pageBounds && loadText && closeText && getText
            && (permissions(document) & copyPermission);
    }

    template<class Function>
    static Function load(QLibrary &lib, const char *symbol)
    {
        return reinterpret_cast<Function>(lib.resolve(symbol));
    }

    void (*init)();
    void (*destroy)();
    PdfDocument (*loadDocument)(const void *, int, const char *);
    void (*closeDocument)(PdfDocument);
    unsigned long (*lastError)();
    int (*pageCount)(PdfDocument);
    PdfPage (*loadPage)(PdfDocument, int);
    void (*closePage)(PdfPage);
    float (*pageWidth)(PdfPage);
    float (*pageHeight)(PdfPage);
    PdfBitmap (*createBitmap)(int, int, int, void *, int);
    void (*destroyBitmap)(PdfBitmap);
    void (*render)(PdfBitmap, PdfPage, int, int, int, int, int, int);
    unsigned long (*permissions)(PdfDocument);
    int (*pageBounds)(PdfPage, PdfRectangle *);
    PdfText (*loadText)(PdfPage);
    void (*closeText)(PdfText);
    int (*getText)(PdfText, double, double, double, double, unsigned short *, int);
};

QImage drawPage(const Pdfium &pdfium, PdfPage page)
{
    if (!page)
        return {};
    const QSize size = rasterSize(pdfium.pageWidth(page), pdfium.pageHeight(page));
    if (size.isEmpty())
        return {};

    QImage image(size, QImage::Format_RGB32);
    if (image.isNull())
        return {};
    image.fill(Qt::white);

    constexpr int bgrx = 3;
    constexpr int renderAnnotations = 0x01;
    const std::unique_ptr<fpdf_bitmap_t__, decltype(pdfium.destroyBitmap)> bitmap(
        pdfium.createBitmap(size.width(), size.height(), bgrx, image.bits(), image.bytesPerLine()),
        pdfium.destroyBitmap);
    if (!bitmap)
        return {};
    pdfium.render(bitmap.get(), page, 0, 0, size.width(), size.height(), 0, renderAnnotations);
    return image;
}

QString readPageText(const Pdfium &pdfium, PdfPage page, qsizetype limit)
{
    PdfRectangle bounds{};
    if (!pdfium.pageBounds(page, &bounds))
        return {};
    const std::unique_ptr<fpdf_textpage_t__, decltype(pdfium.closeText)> text(pdfium.loadText(page), pdfium.closeText);
    if (!text)
        return {};

    QList<unsigned short> buffer(limit + 1, 0);
    int length = pdfium.getText(text.get(), bounds.left, bounds.top, bounds.right, bounds.bottom,
                                buffer.data(), buffer.size());
    if (length <= 0 || length > buffer.size())
        return {};
    if (buffer.at(length - 1) == 0)
        --length;
    QString result;
    result.resize(length);
    QChar *characters = result.data();
    for (qsizetype index = 0; index < length; ++index)
        characters[index] = QChar(buffer.at(index));
    return result;
}

ImportedPdf rasterize(const QByteArray &data, QString &error, bool withText)
{
    static QMutex mutex;
    const QMutexLocker lock(&mutex);
    QLibrary library(QStringLiteral("pdfium"));
    const Pdfium pdfium(library);
    if (!pdfium.valid()) {
        error = QCoreApplication::translate("PdfImport", "Il lettore PDF non è disponibile su questo dispositivo.");
        return {};
    }

    constexpr unsigned long passwordError = 4;
    const std::unique_ptr<fpdf_document_t__, decltype(pdfium.closeDocument)> document(
        pdfium.loadDocument(data.constData(), data.size(), nullptr), pdfium.closeDocument);
    if (!document) {
        error = pdfium.lastError() == passwordError ? QCoreApplication::translate("PdfImport", "Il PDF richiede una password.")
                                                   : QCoreApplication::translate("PdfImport", "Impossibile leggere il documento PDF.");
        return {};
    }
    const int count = pdfium.pageCount(document.get());
    if (!checkPageCount(count, error))
        return {};

    ImportedPdf result;
    result.pages.reserve(count);
    result.pageText.reserve(count);
    qint64 renderedSize = 0;
    qsizetype remainingText = withText && pdfium.canExtract(document.get()) ? maxTextPerDocument : 0;
    for (int index = 0; index < count; ++index) {
        const std::unique_ptr<fpdf_page_t__, decltype(pdfium.closePage)> page(
            pdfium.loadPage(document.get(), index), pdfium.closePage);
        if (!storePage(drawPage(pdfium, page.get()), index, result.pages, renderedSize, error))
            return {};
        const qsizetype limit = qMin(remainingText, maxTextPerPage);
        storeText(result, limit ? readPageText(pdfium, page.get(), limit) : QString{}, remainingText);
    }
    return result;
}
#endif

ImportedPdf readPdfData(const QByteArray &data, QString &error, bool withText)
{
    error.clear();
    if (data.size() > maxPdfSize) {
        error = QCoreApplication::translate("PdfImport", "Il PDF supera il limite di 48 MB.");
        return {};
    }
    if (data.isEmpty()) {
        error = QCoreApplication::translate("PdfImport", "Il file PDF è vuoto.");
        return {};
    }
    return rasterize(data, error, withText);
}

ImportedPdf readPdfFile(const QString &path, QString &error, bool withText)
{
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QCoreApplication::translate("PdfImport", "Impossibile aprire il PDF. %1").arg(file.errorString());
        return {};
    }
    if (file.size() > maxPdfSize) {
        error = QCoreApplication::translate("PdfImport", "Il PDF supera il limite di 48 MB.");
        return {};
    }
    const QByteArray data = file.read(maxPdfSize + 1);
    if (file.error() != QFileDevice::NoError) {
        error = QCoreApplication::translate("PdfImport", "Impossibile leggere il PDF. %1").arg(file.errorString());
        return {};
    }
    return readPdfData(data, error, withText);
}
}

ImportedPdf importPdfPages(const QString &path, QString &error)
{
    return readPdfFile(path, error, true);
}

ImportedPdf importPdfData(const QByteArray &data, QString &error)
{
    return readPdfData(data, error, true);
}

QList<QByteArray> renderPdfPages(const QString &path, QString &error)
{
    return readPdfFile(path, error, false).pages;
}

QList<QByteArray> renderPdfData(const QByteArray &data, QString &error)
{
    return readPdfData(data, error, false).pages;
}

void paintPdfBackground(QPainter &painter, QSizeF pageSize, const QImage &image)
{
    if (image.isNull() || pageSize.isEmpty())
        return;
    const QSizeF fitted = QSizeF(image.size()).scaled(pageSize, Qt::KeepAspectRatio);
    const QPointF topLeft((pageSize.width() - fitted.width()) / 2,
                          (pageSize.height() - fitted.height()) / 2);
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRectF(topLeft, fitted), image);
    painter.restore();
}
