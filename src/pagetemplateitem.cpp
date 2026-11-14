#include "pagetemplateitem.h"

#include "pagecontent.h"

#include <QPainter>

PageTemplateItem::PageTemplateItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setOpaquePainting(true);
}

void PageTemplateItem::setPaper(const QString &paper)
{
    if (paper == m_paper || !PageContent::isTemplate(paper))
        return;
    m_paper = paper;
    emit paperChanged();
    update();
}

void PageTemplateItem::paint(QPainter *painter)
{
    // The Paper Pro scene graph ignores QQuickPaintedItem's fill color.
    painter->fillRect(boundingRect(), Qt::white);
    PageContent::paintTemplate(*painter, size(), m_paper);
}
