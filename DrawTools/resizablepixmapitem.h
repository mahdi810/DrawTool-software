#ifndef RESIZABLEPIXMAPITEM_H
#define RESIZABLEPIXMAPITEM_H

#include <QGraphicsPixmapItem>
#include <QPainterPath>
#include <QPointF>

class ResizablePixmapItem : public QGraphicsPixmapItem
{
public:
    explicit ResizablePixmapItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum ResizeHandle { HandleNone, HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight };

    static constexpr qreal HANDLE_SIZE = 10.0;

    ResizeHandle handleAt(const QPointF &localPos) const;
    QRectF handleRect(ResizeHandle handle) const;
    QPointF oppositeCorner(ResizeHandle handle) const;
    Qt::CursorShape cursorForHandle(ResizeHandle handle) const;

    bool m_resizing = false;
    ResizeHandle m_activeHandle = HandleNone;
    qreal m_startScale = 1.0;
    qreal m_startDistance = 1.0;
    QPointF m_originScene;
};

#endif // RESIZABLEPIXMAPITEM_H
