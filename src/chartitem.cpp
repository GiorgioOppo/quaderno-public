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
    if (type != QStringLiteral("chart") && type != QStringLiteral("table"))
        return {};

    // A table-linked chart becomes an editable snapshot of its current values.
    copy.remove(QStringLiteral("sourceTable"));
    copy.remove(QStringLiteral("xColumn"));
    copy.remove(QStringLiteral("yColumn"));
    copy.remove(QStringLiteral("hasHeader"));
    copy = PageContent::validatedItem(copy);
    for (const QString &key : {QStringLiteral("id"), QStringLiteral("x"), QStringLiteral("y"),
                               QStringLiteral("width"), QStringLiteral("height")})
        copy.remove(key);
    return copy;
}
