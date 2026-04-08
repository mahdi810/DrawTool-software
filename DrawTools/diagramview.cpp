#include "diagramview.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QApplication>

DiagramView::DiagramView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setFrameShape(QFrame::NoFrame);
}

void DiagramView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
        emit zoomChanged(transform().m11());
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DiagramView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning  = true;
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void DiagramView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void DiagramView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        QPoint delta = event->pos() - m_panStart;
        m_panStart   = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar  ()->setValue(verticalScrollBar  ()->value() - delta.y());
        event->accept();
        return;
    }
    emit mouseMoved(mapToScene(event->pos()));
    QGraphicsView::mouseMoveEvent(event);
}