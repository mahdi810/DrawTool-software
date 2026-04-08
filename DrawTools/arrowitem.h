#ifndef ARROWITEM_H
#define ARROWITEM_H

#include <QGraphicsLineItem>
#include <QPen>

class ArrowItem : public QGraphicsLineItem
{
public:
    enum ArrowType { NoArrow = 0, OpenArrow = 1, FilledArrow = 2 };

    explicit ArrowItem(const QLineF &line, QGraphicsItem *parent = nullptr);

    void      setStartArrow(ArrowType t) { m_start = t; update(); }
    void      setEndArrow  (ArrowType t) { m_end   = t; update(); }
    ArrowType startArrow()  const { return m_start; }
    ArrowType endArrow()    const { return m_end;   }

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void         paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    static constexpr int Type = UserType + 10;
    int type() const override { return Type; }

private:
    void drawArrowHead(QPainter *painter, const QPointF &tip,
                       const QPointF &tail, ArrowType t) const;

    ArrowType m_start = NoArrow;
    ArrowType m_end   = FilledArrow;
    static constexpr qreal ARROW_SIZE = 12.0;
};

#endif // ARROWITEM_H