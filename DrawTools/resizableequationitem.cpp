#include "resizableequationitem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QSvgRenderer>

ResizableEquationItem::ResizableEquationItem(const QByteArray &svgData, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_svgRenderer(new QSvgRenderer())
    , m_svgData(svgData)
{
    m_svgRenderer->load(m_svgData);
    QSize s = m_svgRenderer->defaultSize();
    if (s.width() <= 0 || s.height() <= 0) s = QSize(600, 180);
    m_baseSize = s;

    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setAcceptHoverEvents(true);
}

ResizableEquationItem::ResizableEquationItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_fallbackPixmap(pixmap)
{
    QSizeF s = pixmap.deviceIndependentSize();
    if (s.width() <= 0 || s.height() <= 0) s = QSizeF(qMax(1, pixmap.width()), qMax(1, pixmap.height()));
    m_baseSize = s;

    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setAcceptHoverEvents(true);
}

ResizableEquationItem::~ResizableEquationItem()
{
    delete m_svgRenderer;
}

QRectF ResizableEquationItem::contentRect() const
{
    return QRectF(0.0, 0.0, m_baseSize.width(), m_baseSize.height());
}

QRectF ResizableEquationItem::boundingRect() const
{
    const QRectF r = contentRect();
    const qreal s = qMax<qreal>(0.05, scale());
    const qreal m = (HANDLE_SIZE / s) * 0.5;
    return r.adjusted(-m, -m, m, m);
}

QPainterPath ResizableEquationItem::shape() const
{
    QPainterPath p;
    p.addRect(contentRect());
    for (ResizeHandle h : {HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight}) {
        p.addRect(handleRect(h));
    }
    return p;
}

QRectF ResizableEquationItem::handleRect(ResizeHandle handle) const
{
    const QRectF r = contentRect();
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

ResizableEquationItem::ResizeHandle ResizableEquationItem::handleAt(const QPointF &localPos) const
{
    if (!isSelected()) return HandleNone;
    for (ResizeHandle h : {HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight}) {
        if (handleRect(h).contains(localPos)) return h;
    }
    return HandleNone;
}

QPointF ResizableEquationItem::oppositeCorner(ResizeHandle handle) const
{
    const QRectF r = contentRect();
    switch (handle) {
    case HandleTopLeft: return r.bottomRight();
    case HandleTopRight: return r.bottomLeft();
    case HandleBottomLeft: return r.topRight();
    case HandleBottomRight: return r.topLeft();
    default: return r.center();
    }
}

Qt::CursorShape ResizableEquationItem::cursorForHandle(ResizeHandle handle) const
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

void ResizableEquationItem::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (m_svgRenderer && m_svgRenderer->isValid()) {
        m_svgRenderer->render(painter, contentRect());
    } else if (!m_fallbackPixmap.isNull()) {
        painter->drawPixmap(contentRect().toRect(), m_fallbackPixmap);
    }

    if (!isSelected()) return;

    const qreal s = qMax<qreal>(0.05, scale());
    QPen borderPen(QColor(0, 120, 215), 1.0 / s, Qt::DashLine);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(contentRect());

    painter->setPen(QPen(QColor(0, 120, 215), 1.0 / s));
    painter->setBrush(Qt::white);
    for (ResizeHandle h : {HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight}) {
        painter->drawRect(handleRect(h));
    }
}

void ResizableEquationItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(cursorForHandle(handleAt(event->pos())));
    QGraphicsItem::hoverMoveEvent(event);
}

void ResizableEquationItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

void ResizableEquationItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        ResizeHandle h = handleAt(event->pos());
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
    QGraphicsItem::mousePressEvent(event);
}

void ResizableEquationItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_resizing) {
        QGraphicsItem::mouseMoveEvent(event);
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

void ResizableEquationItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizing) {
        m_resizing = false;
        m_activeHandle = HandleNone;
        m_startDistance = 1.0;
        m_originScene = QPointF();
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

bool ResizableEquationItem::hasSvgData() const
{
    return !m_svgData.isEmpty();
}

QByteArray ResizableEquationItem::svgData() const
{
    return m_svgData;
}

QPixmap ResizableEquationItem::fallbackPixmap() const
{
    return m_fallbackPixmap;
}
