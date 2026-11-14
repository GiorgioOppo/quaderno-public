#pragma once

#include <QQuickPaintedItem>
#include <QRectF>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace InkSelectionGeometry {
QVariantList transformed(const QVariantList &strokes, QPointF pivot, QPointF offset, qreal scale, qreal angle);
}

class InkSelectionPreview : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList strokes READ strokes WRITE setStrokes NOTIFY strokesChanged)
    Q_PROPERTY(qreal scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY transformChanged)
    Q_PROPERTY(qreal angle READ angle WRITE setAngle NOTIFY transformChanged)
    Q_PROPERTY(QPointF offset READ offset WRITE setOffset NOTIFY placementChanged)
    Q_PROPERTY(QPointF appliedOffset READ appliedOffset NOTIFY placementChanged)
    Q_PROPERTY(QPointF drawOffset READ drawOffset NOTIFY placementChanged)
    Q_PROPERTY(QPointF pivot READ pivot NOTIFY transformChanged)
    Q_PROPERTY(QRectF bounds READ bounds NOTIFY placementChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY placementChanged)
    Q_PROPERTY(bool selectionSupported READ selectionSupported NOTIFY strokesChanged)
    Q_PROPERTY(bool quarterTurnsOnly READ quarterTurnsOnly NOTIFY transformChanged)

public:
    explicit InkSelectionPreview(QQuickItem *parent = nullptr);

    QVariantList strokes() const { return m_strokes; }
    void setStrokes(const QVariantList &strokes);
    qreal scaleFactor() const { return m_scale; }
    void setScaleFactor(qreal scale);
    qreal angle() const { return m_angle; }
    void setAngle(qreal angle);
    QPointF offset() const { return m_offset; }
    void setOffset(QPointF offset);
    QPointF appliedOffset() const { return m_appliedOffset; }
    QPointF drawOffset() const { return m_appliedOffset - m_baseOffset; }
    QPointF pivot() const { return m_pivot; }
    QRectF bounds() const { return m_displayBounds.translated(m_appliedOffset); }
    bool valid() const { return !m_preview.isEmpty(); }
    bool selectionSupported() const { return m_selectionSupported; }
    bool quarterTurnsOnly() const { return m_quarterTurnsOnly; }

    void paint(QPainter *painter) override;

signals:
    void strokesChanged();
    void transformChanged();
    void placementChanged();

private:
    void rebuild();
    void place();

    QVariantList m_strokes;
    QVariantList m_preview;
    QRectF m_displayBounds;
    QRectF m_fullBounds;
    QPointF m_offset;
    QPointF m_appliedOffset;
    QPointF m_baseOffset;
    QPointF m_pivot;
    qreal m_scale = 1;
    qreal m_angle = 0;
    bool m_selectionSupported = false;
    bool m_quarterTurnsOnly = false;
};
