#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include "pdfimport.h"

QList<QByteArray> renderEpubPages(const QString &path, QString &error);

// Creates a stable vector layout; pages are rasterized only when requested.
QByteArray epubSourceBytes(const QString &path, QString &error);
QByteArray convertEpubToPdf(const QByteArray &source, QString &error,
                            const DocumentCancellation &cancel = {}, const DocumentProgress &progress = {});
