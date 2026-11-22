#include "chartitem.h"

#include "notebook.h"
#include "pagecontent.h"

void ChartItem::setChart(const QVariantMap &chart)
{
    if (m_chart == chart)
        return;
    m_chart = chart;
    emit chartChanged();
    update();
}

void ChartItem::paint(QPainter *painter)
{
    PageContent::paintChart(*painter, size(), m_chart);
}

QVariantMap Notebook::libraryItemCopy(const QString &bookId, const QString &pageId, const QString &itemId) const
{
    const int bookIndex = findBook(m_state, bookId);
    if (bookIndex < 0 || m_state.books.at(bookIndex).trashed)
        return {};
    const Book &book = m_state.books.at(bookIndex);
    const int pageIndex = findPage(book, pageId);
    if (pageIndex < 0)
        return {};
    const Page &page = book.pages.at(pageIndex);
    const int itemIndex = findPageItem(page, itemId);
    if (itemIndex < 0)
        return {};

    QVariantMap copy = PageContent::resolveTableCharts(page.items).at(itemIndex).toMap();
    const QString type = copy.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("chart") && type != QStringLiteral("table") && type != QStringLiteral("diagram"))
        return {};

    // A table-linked chart becomes an editable snapshot of its current values.
    copy.remove(QStringLiteral("sourceTable"));
    copy.remove(QStringLiteral("xColumn"));
    copy.remove(QStringLiteral("yColumn"));
    copy.remove(QStringLiteral("hasHeader"));
    // Copies are independent objects; source-book ink/layer identifiers do not travel with them.
    copy.remove(QStringLiteral("handwritingSource"));
    copy = PageContent::validatedItem(copy);
    for (const QString &key : {QStringLiteral("id"), QStringLiteral("x"), QStringLiteral("y"),
                               QStringLiteral("width"), QStringLiteral("height")})
        copy.remove(key);
    return copy;
}

QVariantMap Notebook::tableChartSnapshot(const QString &bookId, const QString &pageId, const QString &tableId,
                                        const QVariantMap &options) const
{
    const QVariantMap table = libraryItemCopy(bookId, pageId, tableId);
    if (table.value(QStringLiteral("type")) != QStringLiteral("table"))
        return {};
    QVariantMap chart = options;
    chart.insert(QStringLiteral("type"), QStringLiteral("chart"));
    chart = PageContent::chartFromTable(table, chart);
    for (const auto &key : {QStringLiteral("sourceTable"), QStringLiteral("xColumn"), QStringLiteral("yColumn"),
                            QStringLiteral("hasHeader")})
        chart.remove(key);
    if (chart.value(QStringLiteral("id")).toString().isEmpty())
        chart.remove(QStringLiteral("id"));
    return chart;
}
