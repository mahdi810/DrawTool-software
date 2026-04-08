#ifndef DIAGRAMRECTITEM_H
#define DIAGRAMRECTITEM_H

#include <QGraphicsRectItem>

class DiagramRectItem : public QGraphicsRectItem
{
public:
    explicit DiagramRectItem(const QRectF &rect, QGraphicsItem *parent = nullptr);

    void   setCornerRadius(qreal r) { m_radius = r; update(); }
    qreal  cornerRadius()  const    { return m_radius; }

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    static constexpr int Type = UserType + 11;
    int type() const override { return Type; }

private:
    qreal m_radius = 0.0;
};

#endif // DIAGRAMRECTITEM_H