#include "diagramrectitem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

DiagramRectItem::DiagramRectItem(const QRectF &rect, QGraphicsItem *parent)
    : QGraphicsRectItem(rect, parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
}

void DiagramRectItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen());
    painter->setBrush(brush());

    if (m_radius > 0.0)
        painter->drawRoundedRect(rect(), m_radius, m_radius);
    else
        painter->drawRect(rect());

    if (isSelected()) {
        QPen selPen(Qt::blue, 1.0, Qt::DashLine);
        painter->setPen(selPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect());
    }
}