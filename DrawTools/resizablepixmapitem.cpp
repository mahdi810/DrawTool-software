#include "resizablepixmapitem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

ResizablePixmapItem::ResizablePixmapItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setAcceptHoverEvents(true);
    setTransformationMode(Qt::SmoothTransformation);
}

QRectF ResizablePixmapItem::boundingRect() const
{
    const QRectF r = QGraphicsPixmapItem::boundingRect();
    const qreal s = qMax<qreal>(0.05, scale());
    const qreal m = (HANDLE_SIZE / s) * 0.5;
    return r.adjusted(-m, -m, m, m);
}

QPainterPath ResizablePixmapItem::shape() const
{
    QPainterPath p = QGraphicsPixmapItem::shape();
    for (ResizeHandle h : {HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight}) {
        p.addRect(handleRect(h));
    }
    return p;
}

QRectF ResizablePixmapItem::handleRect(ResizeHandle handle) const
{
    const QRectF r = QGraphicsPixmapItem::boundingRect();
    const qreal s = qMax<qreal>(0.05, scale());
    const qreal hs = HANDLE_SIZE / s;

    QPointF c;
    switch (handle) {
    case HandleTopLeft: c = r.topLeft(); break;
    case HandleTopRight: c = r.topRight(); break;
    case HandleBottomLeft: c = r.bottomLeft(); break;
    case HandleBottomRight: c = r.bottomRight(); break;
    default: return QRectF();
    }
    return QRectF(c.x() - hs / 2.0, c.y() - hs / 2.0, hs, hs);
}

ResizablePixmapItem::ResizeHandle ResizablePixmapItem::handleAt(const QPointF &localPos) const
{
    if (!isSelected()) return HandleNone;
    for (ResizeHandle h : {HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight}) {
        if (handleRect(h).contains(localPos)) return h;
    }
    return HandleNone;
}

QPointF ResizablePixmapItem::oppositeCorner(ResizeHandle handle) const
{
    const QRectF r = boundingRect();
    switch (handle) {
    case HandleTopLeft: return r.bottomRight();
    case HandleTopRight: return r.bottomLeft();
    case HandleBottomLeft: return r.topRight();
    case HandleBottomRight: return r.topLeft();
    default: return r.center();
    }
}

Qt::CursorShape ResizablePixmapItem::cursorForHandle(ResizeHandle handle) const
{
    switch (handle) {
    case HandleTopLeft:
    case HandleBottomRight:
        return Qt::SizeFDiagCursor;
    case HandleTopRight:
    case HandleBottomLeft:
        return Qt::SizeBDiagCursor;
    default:
        return Qt::ArrowCursor;
    }
}

void ResizablePixmapItem::paint(QPainter *painter,
                                const QStyleOptionGraphicsItem *option,
                                QWidget *widget)
{
    QGraphicsPixmapItem::paint(painter, option, widget);

    if (!isSelected()) return;

    QPen borderPen(QColor(0, 120, 215), 1.0 / qMax<qreal>(0.05, scale()), Qt::DashLine);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(QGraphicsPixmapItem::boundingRect());

    painter->setPen(QPen(QColor(0, 120, 215), 1.0 / qMax<qreal>(0.05, scale())));
    painter->setBrush(Qt::white);
    for (ResizeHandle h : {HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight}) {
        painter->drawRect(handleRect(h));
    }
}

void ResizablePixmapItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const ResizeHandle h = handleAt(event->pos());
    setCursor(cursorForHandle(h));
    QGraphicsPixmapItem::hoverMoveEvent(event);
}

void ResizablePixmapItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsPixmapItem::hoverLeaveEvent(event);
}

void ResizablePixmapItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const ResizeHandle h = handleAt(event->pos());
        if (h != HandleNone) {
            m_resizing = true;
            m_activeHandle = h;
            m_startScale = scale();
            setTransformOriginPoint(oppositeCorner(h));

            m_originScene = mapToScene(transformOriginPoint());
            m_startDistance = QLineF(m_originScene, event->scenePos()).length();
            if (m_startDistance < 1e-3) m_startDistance = 1.0;

            event->accept();
            return;
        }
    }

    QGraphicsPixmapItem::mousePressEvent(event);
}

void ResizablePixmapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_resizing) {
        QGraphicsPixmapItem::mouseMoveEvent(event);
        return;
    }

    const qreal currentDistance = qMax<qreal>(1e-3, QLineF(m_originScene, event->scenePos()).length());
    const qreal factor = currentDistance / m_startDistance;
    qreal nextScale = m_startScale * factor;
    nextScale = qBound<qreal>(0.05, nextScale, 30.0);

    setScale(nextScale);
    update();
    event->accept();
}

void ResizablePixmapItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizing) {
        m_resizing = false;
        m_activeHandle = HandleNone;
        m_startDistance = 1.0;
        m_originScene = QPointF();
        event->accept();
        return;
    }

    QGraphicsPixmapItem::mouseReleaseEvent(event);
}
