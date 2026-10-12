#pragma once

#include <QGraphicsView>

namespace bb {

class DiagramView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DiagramView(QGraphicsScene *scene, QWidget *parent = nullptr);

    void zoomIn();
    void zoomOut();
    void fitToContent();
    void zoomBy(qreal factor);

signals:
    void zoomChanged(qreal scaleFactor);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void panBy(const QPoint &delta);

    bool m_panArmed = false;  // space bar held: left-drag pans
    bool m_panning = false;
    QPoint m_lastPan;

    bool m_rightPanning = false;   // right-drag pans the canvas
    QPoint m_rightPressPos;
    bool m_rightMoved = false;
    bool m_contextMenuSynthesized = false;
};

} // namespace bb