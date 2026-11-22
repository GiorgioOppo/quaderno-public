#include "pdfimport_mac.h"

#import <PDFKit/PDFKit.h>

namespace pdfImportMac {
ImportedPdf importData(const QByteArray &data, QString &error)
{
    @autoreleasepool {
        NSData *bytes = [NSData dataWithBytesNoCopy:const_cast<char *>(data.constData())
                                            length:data.size()
                                      freeWhenDone:NO];
        PDFDocument *document = [[PDFDocument alloc] initWithData:bytes];
        const CGPDFDocumentRef reference = document.documentRef;
        if (!reference) {
            ImportedPdf result;
            result.pages = renderPdfData(data, error);
            for (qsizetype index = 0; index < result.pages.size(); ++index)
                result.pageText.append(QString{});
            return result;
        }

        if (!document.allowsCopying)
            return renderDocument(reference, error);

        return renderDocument(reference, error, [document](qsizetype index, qsizetype limit) {
            @autoreleasepool {
                PDFPage *page = [document pageAtIndex:index];
                if (!page)
                    return QString{};

                const CGRect bounds = CGRectIntersection([page boundsForBox:kPDFDisplayBoxCropBox],
                                                         [page boundsForBox:kPDFDisplayBoxMediaBox]);
                if (CGRectIsEmpty(bounds))
                    return QString{};

                NSString *string = [page selectionForRect:bounds].string;
                if (string.length > static_cast<NSUInteger>(limit))
                    string = [string substringToIndex:limit];
                return QString::fromNSString(string);
            }
        });
    }
}
}

namespace pdfImportMac {
PdfIndex inspectData(const QByteArray &data, QString &error, const DocumentCancellation &cancel, const DocumentProgress &progress)
{
    @autoreleasepool {
        NSData *bytes = [NSData dataWithBytesNoCopy:const_cast<char *>(data.constData()) length:data.size() freeWhenDone:NO];
        PDFDocument *document = [[PDFDocument alloc] initWithData:bytes];
        if (!document || document.isLocked || document.pageCount < 1 || document.pageCount > 100000) {
            error = QStringLiteral("Impossibile indicizzare il PDF. Verifica che non richieda una password.");
            return {};
        }
        PdfIndex result;
        result.pageCount = static_cast<int>(document.pageCount);
        result.pageText.reserve(result.pageCount);
        qsizetype remaining = document.allowsCopying ? 750000 : 0;
        for (int index = 0; index < result.pageCount; ++index) {
            if (cancel && cancel->load())
                return {};
            @autoreleasepool {
                QString text;
                if (remaining) {
                    PDFPage *page = [document pageAtIndex:index];
                    NSString *string = page.string;
                    const NSUInteger length = qMin<qsizetype>(remaining, 20000);
                    if (string.length > length)
                        string = [string substringToIndex:length];
                    text = QString::fromNSString(string).simplified();
                    remaining -= text.size();
                }
                result.pageText.append(text);
            }
            if (progress && (index % 16 == 0 || index + 1 == result.pageCount))
                progress(index + 1, result.pageCount);
        }
        return result;
    }
}
}
