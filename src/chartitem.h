#pragma once

#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class ChartItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap chart READ chart WRITE setChart NOTIFY chartChanged)

public:
    using QQuickPaintedItem::QQuickPaintedItem;
    QVariantMap chart() const { return m_chart; }
    void setChart(const QVariantMap &chart);
    void paint(QPainter *painter) override;

signals:
    void chartChanged();

private:
    QVariantMap m_chart;
};
