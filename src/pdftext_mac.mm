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
