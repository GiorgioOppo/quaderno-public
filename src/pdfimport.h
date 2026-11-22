#pragma once

#include <QByteArray>
#include <QList>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QImage>
#include <atomic>
#include <functional>
#include <memory>

class QImage;
class QPainter;

struct ImportedPdf {
    QList<QByteArray> pages;
    QStringList pageText;
};

using DocumentCancellation = std::shared_ptr<std::atomic_bool>;
using DocumentProgress = std::function<void(int, int)>;
struct PdfIndex {
    int pageCount = 0;
    QStringList pageText;
};

PdfIndex inspectPdfData(const QByteArray &data, QString &error,
                        const DocumentCancellation &cancel = {}, const DocumentProgress &progress = {});
QImage renderPdfPage(const QByteArray &data, int page, int maximumEdge, QString &error);

ImportedPdf importPdfPages(const QString &path, QString &error);
ImportedPdf importPdfData(const QByteArray &data, QString &error);
QList<QByteArray> renderPdfPages(const QString &path, QString &error);
QList<QByteArray> renderPdfData(const QByteArray &data, QString &error);
void paintPdfBackground(QPainter &painter, QSizeF pageSize, const QImage &image);
