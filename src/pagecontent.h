#pragma once

#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QPainter;
class QUrl;

namespace PageContent {
inline constexpr int maxTableRows = 12;
inline constexpr int maxTableColumns = 8;
inline constexpr qsizetype maxTableCellLength = 500;
inline constexpr int maxImagesPerPage = 12;

bool isTemplate(const QString &key);
QVariantMap imageFromFile(const QUrl &file, QString &error);
QVariantMap validatedItem(const QVariantMap &item);
QVariantList resolvedTableCells(const QVariantMap &table);
QVariantMap chartFromTable(const QVariantMap &table, const QVariantMap &options);
QVariantList resolveTableCharts(const QVariantList &items);
qreal preferredItemHeight(const QVariantMap &item, qreal width);
QRectF textRectAtInk(const QString &text, const QRectF &inkBounds);
void paintTemplate(QPainter &painter, QSizeF pageSize, const QString &key);
void paintChart(QPainter &painter, QSizeF size, const QVariantMap &chart);
void paintItems(QPainter &painter, QSizeF pageSize, const QVariantList &items);
}
