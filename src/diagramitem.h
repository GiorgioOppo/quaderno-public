#pragma once

#include <QPainterPath>
#include <QQuickPaintedItem>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QPainter;

QPainterPath diagramNodeOutline(const QString &shape, const QRectF &bounds);
bool diagramShapeRequiresNewFormat(const QString &shape);
bool diagramShapeRequiresUmlFormat(const QString &shape);
void drawDiagram(QPainter &painter, const QRectF &bounds, const QVariantMap &diagram);

class DiagramItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap diagram READ diagram WRITE setDiagram NOTIFY diagramChanged)
    Q_PROPERTY(QString selectedNode READ selectedNode WRITE setSelectedNode NOTIFY selectedNodeChanged)
    Q_PROPERTY(QStringList selectedNodes READ selectedNodes WRITE setSelectedNodes NOTIFY selectedNodesChanged)
    Q_PROPERTY(QString selectedEdge READ selectedEdge WRITE setSelectedEdge NOTIFY selectedEdgeChanged)
    Q_PROPERTY(QRectF viewRect READ viewRect WRITE setViewRect NOTIFY viewRectChanged)

public:
    using QQuickPaintedItem::QQuickPaintedItem;

    QVariantMap diagram() const { return m_diagram; }
    QString selectedNode() const { return m_selectedNode; }
    QStringList selectedNodes() const { return m_selectedNodes; }
    QString selectedEdge() const { return m_selectedEdge; }
    QRectF viewRect() const { return m_viewRect; }
    void setDiagram(const QVariantMap &diagram);
    void setSelectedNode(const QString &id);
    void setSelectedNodes(const QStringList &ids);
    void setSelectedEdge(const QString &id);
    void setViewRect(const QRectF &rect);
    void paint(QPainter *painter) override;

    Q_INVOKABLE QString nodeAt(qreal x, qreal y) const;
    Q_INVOKABLE QString edgeAt(qreal x, qreal y) const;
    Q_INVOKABLE QStringList nodesInLasso(const QVariantList &points) const;

signals:
    void diagramChanged();
    void selectedNodeChanged();
    void selectedNodesChanged();
    void selectedEdgeChanged();
    void viewRectChanged();

private:
    QRectF diagramBounds() const;

    QVariantMap m_diagram;
    QString m_selectedNode;
    QStringList m_selectedNodes;
    QString m_selectedEdge;
    QRectF m_viewRect;
};
