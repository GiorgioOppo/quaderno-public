#pragma once

#include "inkrender.h"
#include "shaperecognition.h"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class PaperProMarker;
class QPointingDevice;

class InkItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList strokes READ strokes WRITE setStrokes NOTIFY strokesChanged)
    Q_PROPERTY(QColor inkColor READ inkColor WRITE setInkColor NOTIFY inkColorChanged)
    Q_PROPERTY(qreal inkWidth READ inkWidth WRITE setInkWidth NOTIFY inkWidthChanged)
    Q_PROPERTY(QString inkTool READ inkTool WRITE setInkTool NOTIFY inkToolChanged)
    Q_PROPERTY(bool smartShapes READ smartShapes WRITE setSmartShapes NOTIFY smartShapesChanged)
    Q_PROPERTY(qreal pressureExponent READ pressureExponent WRITE setPressureExponent NOTIFY inputPreferencesChanged)
    Q_PROPERTY(qreal shapeTolerance READ shapeTolerance WRITE setShapeTolerance NOTIFY inputPreferencesChanged)
    Q_PROPERTY(QString palmRejection READ palmRejection WRITE setPalmRejection NOTIFY inputPreferencesChanged)
    Q_PROPERTY(bool touchBlocked READ touchBlocked NOTIFY touchBlockedChanged)
    Q_PROPERTY(bool lassoMode READ lassoMode WRITE setLassoMode NOTIFY lassoModeChanged)
    Q_PROPERTY(int erasableFrom READ erasableFrom WRITE setErasableFrom NOTIFY erasableRangeChanged)
    Q_PROPERTY(int erasableCount READ erasableCount WRITE setErasableCount NOTIFY erasableRangeChanged)
    Q_PROPERTY(bool acceptsMouse READ acceptsMouse WRITE setAcceptsMouse NOTIFY acceptsMouseChanged)
    Q_PROPERTY(QVariantList pageItems MEMBER m_pageItems NOTIFY pageItemsChanged)
    Q_PROPERTY(bool penAvailable READ penAvailable NOTIFY penAvailableChanged)
    Q_PROPERTY(bool drawing READ drawing NOTIFY drawingChanged)
    Q_PROPERTY(QQuickItem *inputOverlay MEMBER m_inputOverlay NOTIFY inputOverlayChanged)
    Q_PROPERTY(bool pointPicking READ pointPicking WRITE setPointPicking NOTIFY pointPickingChanged)
    Q_PROPERTY(bool nativeMarkerEnabled READ nativeMarkerEnabled WRITE setNativeMarkerEnabled NOTIFY nativeMarkerEnabledChanged)
    Q_PROPERTY(InkItem *markerInkTarget READ markerInkTarget WRITE setMarkerInkTarget NOTIFY markerInkTargetChanged)

public:
    explicit InkItem(QQuickItem *parent = nullptr);
    ~InkItem() override;

    QVariantList strokes() const { return m_strokes; }
    void setStrokes(const QVariantList &strokes);
    QColor inkColor() const { return m_inkColor; }
    void setInkColor(QColor color);
    qreal inkWidth() const { return m_inkWidth; }
    void setInkWidth(qreal width);
    QString inkTool() const;
    void setInkTool(const QString &name);
    bool smartShapes() const { return m_smartShapes; }
    void setSmartShapes(bool enabled);
    qreal pressureExponent() const { return m_pressureExponent; }
    void setPressureExponent(qreal value);
    qreal shapeTolerance() const { return m_shapeTolerance; }
    void setShapeTolerance(qreal value);
    QString palmRejection() const { return m_palmRejection; }
    void setPalmRejection(const QString &value);
    bool touchBlocked() const;
    bool lassoMode() const { return m_lassoMode; }
    void setLassoMode(bool enabled);
    int erasableFrom() const { return m_erasableFrom; }
    void setErasableFrom(int first);
    int erasableCount() const { return m_erasableCount; }
    void setErasableCount(int count);
    bool acceptsMouse() const { return m_acceptsMouse; }
    void setAcceptsMouse(bool enabled);
    bool penAvailable() const;
    bool drawing() const;
    bool pointPicking() const { return m_pointPicking; }
    void setPointPicking(bool value);
    bool nativeMarkerEnabled() const { return m_nativeMarkerEnabled; }
    void setNativeMarkerEnabled(bool value);
    InkItem *markerInkTarget() const { return m_markerInkTarget.data(); }
    void setMarkerInkTarget(InkItem *target);

    Q_INVOKABLE void cancelDrawing();
    Q_INVOKABLE QString textItemAt(QPointF position) const;
    Q_INVOKABLE QVariantMap tableCellAt(QPointF position) const;
    Q_INVOKABLE QString chartItemAt(QPointF position) const;
    Q_INVOKABLE QString diagramItemAt(QPointF position) const;
    Q_INVOKABLE QString noteItemAt(QPointF position) const;
    void paint(QPainter *painter) override;

signals:
    void strokesChanged();
    void inkColorChanged();
    void inkWidthChanged();
    void inkToolChanged();
    void smartShapesChanged();
    void inputPreferencesChanged();
    void touchBlockedChanged();
    void lassoModeChanged();
    void erasableRangeChanged();
    void acceptsMouseChanged();
    void pageItemsChanged();
    void penAvailableChanged();
    void drawingChanged();
    void strokeStarted();
    void inputActivity();
    void inputOverlayChanged();
    void pointPickingChanged();
    void nativeMarkerEnabledChanged();
    void markerInkTargetChanged();
    void pagePointPicked(QPointF position);
    void noteItemTapped(const QString &id);
    void strokeCompleted(const QVariantMap &stroke);
    void textItemTapped(const QString &id);
    void tableCellTapped(const QString &id, int row, int column);
    void chartItemTapped(const QString &id);
    void diagramItemTapped(const QString &id);
    void correctedStrokeCompleted(const QVariantMap &shape, const QVariantMap &original);
    void lassoCompleted(const QVariantList &polygon);
    void strokesErased(const QVariantList &indexes);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void geometryChange(const QRectF &geometry, const QRectF &previousGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;

private:
    friend class PaperProMarker;

    enum class Source { None, Mouse, Tablet, Marker };
    enum class Phase { Press, Move, Release };
    enum class MarkerContact { None, Ink, Control };

    struct Stroke {
        Source source = Source::None;
        QColor color;
        qreal width = 0;
        InkTool tool = InkTool::Pen;
        QList<InkPoint> points;
        QString textTapId;
        QVariantMap tableTapCell;
        QString chartTapId;
        QString diagramTapId;
        QString noteTapId;
        bool pointPick = false;
        bool pointPickCancelled = false;
        QPointF textTapStart;
        qreal textTapPressure = 0;
        QElapsedTimer textTapTime;
    };

    struct ErasableStroke {
        QList<QPointF> points;
        QRectF bounds;
        QList<qreal> radii;
    };

    void observeWindow(QQuickWindow *window);
    void dispatchMarker(Phase phase, QPointF windowPoint, qreal pressure, bool eraserTip);
    void cancelMarker();
    void releaseMarkerWindow();
    bool inputOverlayContains(QPointF position) const;
    bool handleInput(Source source, Phase phase, QPointF position, qreal pressure, bool eraserTip = false);
    void addPoint(QPointF position, qreal pressure);
    void addLassoPoint(const InkPoint &point);
    bool prepareShapeSample(QPointF position);
    void previewHeldShape();
    void restoreFreehand();
    void finishLasso();
    void updateShapeAnchor(InkPoint point);
    void finishStroke();
    void prepareEraser();
    void eraseNear(const InkPoint &from, const InkPoint &to);
    void resetDrawing();
    void rebuildImage();

    QPointer<QQuickItem> m_inputOverlay;
    QPointer<InkItem> m_markerInkTarget;
    QPointer<InkItem> m_markerStrokeTarget;
    QElapsedTimer m_inputActivity;
    QPointer<QQuickWindow> m_observedWindow;
    QPointer<QQuickWindow> m_markerWindow;
    std::unique_ptr<QPointingDevice> m_markerPointer;
    QPointF m_markerPosition;
    MarkerContact m_markerContact = MarkerContact::None;
    QVariantList m_strokes;
    QVariantList m_pageItems;
    QVariantMap m_pendingStroke;
    Stroke m_live;
    QTimer m_shapeHold;
    QPointF m_holdPosition;
    std::optional<RecognizedShape> m_shapePreview;
    QList<ErasableStroke> m_erasable;
    QSet<int> m_erased;
    QImage m_image;
    QColor m_inkColor = QColor(QStringLiteral("#252523"));
    qreal m_inkWidth = 2.0;
    InkTool m_inkTool = InkTool::Pen;
    int m_erasableFrom = 0;
    int m_erasableCount = -1;
    int m_eraserOffset = 0;
    bool m_acceptsMouse = true;
    bool m_smartShapes = false;
    qreal m_pressureExponent = 1;
    qreal m_shapeTolerance = 1;
    QString m_palmRejection = QStringLiteral("writing");
    QTimer m_palmHold;
    bool m_lassoMode = false;
    bool m_epaper = false;
    bool m_pointPicking = false;
    bool m_nativeMarkerEnabled = true;

#ifdef Q_OS_LINUX
    std::unique_ptr<PaperProMarker> m_marker;
#endif
};
