#pragma once

#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

class PageTemplateItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString paper READ paper WRITE setPaper NOTIFY paperChanged)

public:
    explicit PageTemplateItem(QQuickItem *parent = nullptr);

    QString paper() const { return m_paper; }
    void setPaper(const QString &paper);
    void paint(QPainter *painter) override;

signals:
    void paperChanged();

private:
    QString m_paper = QStringLiteral("blank");
};
