#pragma once

#include "pdfimport.h"
#include <QVariantMap>
#include <QVariantList>

// Rebuildable document copies and rendered pages only; the library's immutable
// originals are never part of this cache.
namespace DocumentCache {
int limitMiB();
void setLimitMiB(int value);
quint64 generation();
QImage image(const QString &key);
void insertImage(const QString &key, const QImage &image, quint64 generation);
QVariantMap memoryStatus();
void clearMemory();
QVariantMap sourceStatus(const QString &directory, QString *error = nullptr);
bool clearSources(const QString &directory, QString *error);
}

struct PreparedDocument {
    QVariantMap sources;
    QVariantList pages;
    QString error;
};

PreparedDocument prepareDocument(const QString &path, const DocumentCancellation &cancel = {},
                                 const DocumentProgress &progress = {});
QImage renderDocumentPage(const QVariantMap &source, int maximumEdge, QString &error);

Q_DECLARE_METATYPE(PreparedDocument)
DocumentCancellation startDocumentPreparation(const QString &path, QObject *context,
                                               const DocumentProgress &progress,
                                               std::function<void(PreparedDocument)> finished);
