#pragma once

#include "pdfimport.h"

#include <CoreGraphics/CoreGraphics.h>

#include <functional>

namespace pdfImportMac {
using PageText = std::function<QString(qsizetype index, qsizetype limit)>;
PdfIndex inspectData(const QByteArray &data, QString &error, const DocumentCancellation &cancel, const DocumentProgress &progress);
ImportedPdf importData(const QByteArray &data, QString &error);
ImportedPdf renderDocument(CGPDFDocumentRef document, QString &error, const PageText &readText = {});
}
