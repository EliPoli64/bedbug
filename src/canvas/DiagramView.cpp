#include "DiagramView.h"

#include "DiagramScene.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

namespace bb {

namespace {
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 8.0;
constexpr qreal kZoomStep = 1.25;
} // namespace

DiagramView::DiagramView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setBackgroundBrush(Qt::transparent);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
}

void DiagramView::zoomIn()
{
    zoomBy(kZoomStep);
}

void DiagramView::zoomOut()
{
    zoomBy(1.0 / kZoomStep);
}

void DiagramView::zoomBy(qreal factor)
{
    const qreal current = transform().m11();
    const qreal next = std::clamp(current * factor, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(current, next))
        return;
    scale(next / current, next / current);
    emit zoomChanged(next);
}

void DiagramView::fitToContent()
{
    const QRectF bounds = scene()->itemsBoundingRect().adjusted(-40, -40, 40, 40);
    if (bounds.isEmpty())
        return;
    fitInView(bounds, Qt::KeepAspectRatio);
}

void DiagramView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const qreal factor = std::pow(1.0015, event->angleDelta().y());
        zoomBy(factor);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void DiagramView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_panArmed = true;
        setDragMode(QGraphicsView::NoDrag);
        viewport()->setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void DiagramView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_panArmed = false;
        m_panning = false;
        setDragMode(QGraphicsView::RubberBandDrag);
        viewport()->unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void DiagramView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && m_panArmed)) {
        m_panning = true;
        m_lastPan = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void DiagramView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        panBy(event->pos() - m_lastPan);
        m_lastPan = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void DiagramView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        viewport()->setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void DiagramView::panBy(const QPoint &delta)
{
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
}

void DiagramView::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);

    const qreal grid = DiagramScene::kGridSize;
    const QRectF visible =
        mapToScene(viewport()->rect()).boundingRect().adjusted(-grid, -grid, grid, grid);

    // Dotted grid — subtle, keeps focus on the diagram.
    QColor dotColor = palette().color(QPalette::Mid);
    dotColor.setAlpha(70);
    painter->setPen(QPen(dotColor, 0));
    painter->setBrush(dotColor);

    const qreal left = std::floor(visible.left() / grid) * grid;
    const qreal top = std::floor(visible.top() / grid) * grid;
    for (qreal x = left; x <= visible.right(); x += grid)
        for (qreal y = top; y <= visible.bottom(); y += grid)
            painter->drawEllipse(QPointF(x, y), 1.1, 1.1);
}

} // namespace bb