#include "arrowitem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <cmath>

static bool isFinitePoint(const QPointF &p)
{
    return std::isfinite(p.x()) && std::isfinite(p.y());
}

ArrowItem::ArrowItem(const QLineF &line, QGraphicsItem *parent)
    : QGraphicsLineItem(line, parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
    QPen p(Qt::black, 1.5);
    p.setCapStyle(Qt::RoundCap);
    setPen(p);
}

QRectF ArrowItem::boundingRect() const
{
    const QLineF ln = line();
    if (!isFinitePoint(ln.p1()) || !isFinitePoint(ln.p2())) {
        return QRectF();
    }
    return QGraphicsLineItem::boundingRect().adjusted(
        -ARROW_SIZE, -ARROW_SIZE, ARROW_SIZE, ARROW_SIZE);
}

QPainterPath ArrowItem::shape() const
{
    QPainterPath path = QGraphicsLineItem::shape();
    path.addRect(boundingRect());
    return path;
}

void ArrowItem::drawArrowHead(QPainter *painter, const QPointF &tip,
                              const QPointF &tail, ArrowType t) const
{
    if (t == NoArrow) return;
    if (!isFinitePoint(tip) || !isFinitePoint(tail)) return;

    QLineF ln(tail, tip);
    if (ln.length() < 1e-6) return;
    double angle = std::atan2(-ln.dy(), ln.dx());

    double a1 = angle + M_PI / 6.0;
    double a2 = angle - M_PI / 6.0;

    QPointF p1 = tip - QPointF(std::cos(a1) * ARROW_SIZE, -std::sin(a1) * ARROW_SIZE);
    QPointF p2 = tip - QPointF(std::cos(a2) * ARROW_SIZE, -std::sin(a2) * ARROW_SIZE);

    QPolygonF head;
    head << tip << p1 << p2;

    if (t == FilledArrow) {
        painter->setBrush(pen().color());
        painter->drawPolygon(head);
    } else {
        painter->setBrush(Qt::NoBrush);
        painter->drawPolyline(head);
    }
}

void ArrowItem::paint(QPainter *painter,
                      const QStyleOptionGraphicsItem *option,
                      QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    const QLineF ln = line();
    if (!isFinitePoint(ln.p1()) || !isFinitePoint(ln.p2())) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->drawLine(ln);

    drawArrowHead(painter, ln.p2(), ln.p1(), m_end);
    drawArrowHead(painter, ln.p1(), ln.p2(), m_start);

    if (isSelected()) {
        QPen selPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect());
    }
}