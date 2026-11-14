#pragma once

#include <QByteArray>
#include <QList>
#include <QSizeF>
#include <QString>
#include <QStringList>

class QImage;
class QPainter;

struct ImportedPdf {
    QList<QByteArray> pages;
    QStringList pageText;
};

ImportedPdf importPdfPages(const QString &path, QString &error);
ImportedPdf importPdfData(const QByteArray &data, QString &error);
QList<QByteArray> renderPdfPages(const QString &path, QString &error);
QList<QByteArray> renderPdfData(const QByteArray &data, QString &error);
void paintPdfBackground(QPainter &painter, QSizeF pageSize, const QImage &image);
