#include "DiagramView.h"

#include "DiagramScene.h"

#include <QContextMenuEvent>
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
    setAlignment(Qt::AlignLeft | Qt::AlignTop);   // never pin the page to the centre
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

    // Shift+wheel pans sideways. Trackpads may report pixel deltas or a horizontal
    // angle delta; prefer whichever axis the device actually moved.
    if (event->modifiers() & Qt::ShiftModifier) {
        QPoint delta = event->pixelDelta();
        if (delta.isNull())
            delta = event->angleDelta();
        const int step = delta.x() != 0 ? delta.x() : delta.y();
        QScrollBar *bar = horizontalScrollBar();
        bar->setValue(bar->value() - step);
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

    // Right-drag pans the canvas. A right click without movement still opens the table
    // context menu (synthesized on release below).
    if (event->button() == Qt::RightButton) {
        m_rightPanning = true;
        m_rightMoved = false;
        m_rightPressPos = event->pos();
        m_lastPan = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void DiagramView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_rightPanning) {
        if ((event->pos() - m_rightPressPos).manhattanLength() > 3)
            m_rightMoved = true;
        panBy(event->pos() - m_lastPan);
        m_lastPan = event->pos();
        event->accept();
        return;
    }
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
    if (m_rightPanning && event->button() == Qt::RightButton) {
        m_rightPanning = false;
        viewport()->setCursor(Qt::ArrowCursor);
        if (!m_rightMoved) {
            // Show the context menu ourselves; suppress the platform duplicate.
            m_suppressContextMenu = true;
            QContextMenuEvent menuEvent(QContextMenuEvent::Mouse, event->pos(),
                                        event->globalPosition().toPoint());
            QGraphicsView::contextMenuEvent(&menuEvent);
            m_suppressContextMenu = false;
        }
        event->accept();
        return;
    }
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        viewport()->setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void DiagramView::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_suppressContextMenu) {
        m_suppressContextMenu = false;
        event->accept();
        return;
    }
    if (m_rightPanning) {
        event->accept();
        return;
    }
    QGraphicsView::contextMenuEvent(event);
}

void DiagramView::panBy(const QPoint &delta)
{
    auto *horizontal = horizontalScrollBar();
    auto *vertical = verticalScrollBar();
    if (horizontal->maximum() > horizontal->minimum()
        || vertical->maximum() > vertical->minimum()) {
        horizontal->setValue(horizontal->value() - delta.x());
        vertical->setValue(vertical->value() - delta.y());
    } else {
        // Scene fits in the viewport: pan by translating the view so dragging
        // always moves the canvas, regardless of scroll-bar range.
        translate(delta.x(), delta.y());
    }
}

void DiagramView::drawBackground(QPainter *painter, const QRectF &rect)
{
    // Workspace background (theme-adaptive), with A4 sheets laid out on a grid.
    const QColor workspace = palette().color(QPalette::Window);
    painter->fillRect(rect, workspace);

    auto *diagramScene = dynamic_cast<DiagramScene *>(scene());
    if (!diagramScene)
        return;

    const int pagesX = diagramScene->pagesX();
    const int pagesY = diagramScene->pagesY();
    const int originX = diagramScene->originX();
    const int originY = diagramScene->originY();
    const qreal sheetWidth = DiagramScene::kSheetWidth;
    const qreal sheetHeight = DiagramScene::kSheetHeight;

    // A stylesheet can flatten QPalette::Base onto the window colour; make sure the
    // page still stands out from the workspace in that case.
    QColor pageColor = palette().color(QPalette::Base);
    if (qAbs(pageColor.lightness() - workspace.lightness()) < 8)
        pageColor = workspace.lightness() > 127 ? QColor(Qt::white) : workspace.lighter(118);
    const QColor pageBorder = palette().color(QPalette::Mid);
    QColor pageShadow = Qt::black;
    pageShadow.setAlpha(18);

    for (int ix = 0; ix < pagesX; ++ix) {
        for (int iy = 0; iy < pagesY; ++iy) {
            const QRectF pageRect(originX * sheetWidth + ix * sheetWidth,
                                  originY * sheetHeight + iy * sheetHeight,
                                  sheetWidth, sheetHeight);
            if (!pageRect.intersects(rect))
                continue;

            // Soft drop shadow.
            painter->fillRect(pageRect.adjusted(3, 3, 3, 3), pageShadow);
            // Page body.
            painter->fillRect(pageRect, pageColor);
            // Border.
            painter->setPen(QPen(pageBorder, 1));
            painter->drawRect(pageRect);

            // Page number label, top-left inside the sheet.
            painter->setPen(palette().color(QPalette::WindowText));
            QFont font = painter->font();
            font.setPointSizeF(8.0);
            painter->setFont(font);
            painter->drawText(pageRect.adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft,
                             QStringLiteral("%1").arg(ix + iy * pagesX + 1));
        }
    }

    // Grid dots inside the canvas only.
    QColor dotColor = pageBorder;
    dotColor.setAlpha(90);
    painter->setPen(QPen(dotColor, 0));
    painter->setBrush(dotColor);

    const QRectF canvas = diagramScene->canvasRect();
    const QRectF visible = canvas.intersected(rect.adjusted(-sheetWidth, -sheetHeight, sheetWidth,
                                                            sheetHeight));
    const qreal left = std::floor(visible.left() / DiagramScene::kGridSize) * DiagramScene::kGridSize;
    const qreal top = std::floor(visible.top() / DiagramScene::kGridSize) * DiagramScene::kGridSize;
    for (qreal x = left; x <= visible.right(); x += DiagramScene::kGridSize) {
        for (qreal y = top; y <= visible.bottom(); y += DiagramScene::kGridSize) {
            if (canvas.contains(QPointF(x, y)))
                painter->drawEllipse(QPointF(x, y), 1.0, 1.0);
        }
    }
}

} // namespace bb