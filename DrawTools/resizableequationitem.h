#ifndef RESIZABLEEQUATIONITEM_H
#define RESIZABLEEQUATIONITEM_H

#include <QGraphicsItem>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QByteArray>

class QSvgRenderer;

class ResizableEquationItem : public QGraphicsItem
{
public:
    explicit ResizableEquationItem(const QByteArray &svgData, QGraphicsItem *parent = nullptr);
    explicit ResizableEquationItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);
    ~ResizableEquationItem() override;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    bool hasSvgData() const;
    QByteArray svgData() const;
    QPixmap fallbackPixmap() const;
    QSizeF baseSize() const;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum ResizeHandle { HandleNone, HandleTopLeft, HandleTopRight, HandleBottomLeft, HandleBottomRight };

    QRectF contentRect() const;
    ResizeHandle handleAt(const QPointF &localPos) const;
    QRectF handleRect(ResizeHandle handle) const;
    QPointF oppositeCorner(ResizeHandle handle) const;
    Qt::CursorShape cursorForHandle(ResizeHandle handle) const;

    static constexpr qreal HANDLE_SIZE = 10.0;

    QSvgRenderer *m_svgRenderer = nullptr;
    QByteArray m_svgData;
    QPixmap m_fallbackPixmap;
    QSizeF m_baseSize;

    bool m_resizing = false;
    ResizeHandle m_activeHandle = HandleNone;
    qreal m_startScale = 1.0;
    qreal m_startDistance = 1.0;
    QPointF m_originScene;
};

#endif // RESIZABLEEQUATIONITEM_H
