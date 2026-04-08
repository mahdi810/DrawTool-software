#ifndef DIAGRAMSCENE_H
#define DIAGRAMSCENE_H

#include <QGraphicsScene>
#include <QUndoStack>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QColor>
#include <QPixmap>
#include <QPointF>
#include <QList>
#include <QVector>
#include "arrowitem.h"

class DiagramScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum Mode {
        SelectMode,
        InsertRectMode,
        InsertEllipseMode,
        InsertLineMode,
        InsertArrowMode,
        InsertJunctionDotMode,
        InsertTextMode,
        InsertSymbolMode
    };
    Q_ENUM(Mode)

    explicit DiagramScene(QObject *parent = nullptr);

    QUndoStack *undoStack() const { return m_undoStack; }

    void setMode(Mode mode);
    Mode mode()  const { return m_mode; }

    void setGridSize   (int s)              { m_gridSize = s; update(); }
    int  gridSize()    const                { return m_gridSize; }
    void setGridVisible(bool v)             { m_gridVisible = v; update(); }
    bool gridVisible() const                { return m_gridVisible; }
    void setPageRect(const QRectF &rect);
    QRectF pageRect() const                 { return m_pageRect; }

    void setCurrentPen        (const QPen   &p) { m_pen   = p; }
    void setCurrentBrush      (const QBrush &b) { m_brush = b; }
    void setCurrentFont       (const QFont  &f) { m_font  = f; }
    void setCurrentTextColor  (const QColor &c) { m_textColor = c; }
    void setCurrentStartArrow (ArrowItem::ArrowType t) { m_startArrow = t; }
    void setCurrentEndArrow   (ArrowItem::ArrowType t) { m_endArrow   = t; }
    void setCurrentSymbol(const QPixmap &px,
                          const QList<QPointF> &portsNormalized = {},
                          const QString &symbolName = QString());

    QPen   currentPen()   const { return m_pen; }
    QBrush currentBrush() const { return m_brush; }
    QFont  currentFont()  const { return m_font; }
    QColor currentTextColor() const { return m_textColor; }

    void copySelection(bool cut);
    void paste();
    void duplicate();
    void selectAll();
    void bringToFront();
    void sendToBack();
    void bringForward();
    void sendBackward();

signals:
    void itemInserted(QGraphicsItem *item);
    void modeChanged (Mode mode);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent      (QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent       (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent    (QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent        (QKeyEvent *event)               override;

private:
    QPointF snapToGrid(const QPointF &pt) const;
    QPointF snapToConnectionPoint(const QPointF &pt) const;
    QPointF constrainOrthogonal(const QPointF &from, const QPointF &to) const;
    QList<QGraphicsItem *> makeAutoJunctionsForWire(const QLineF &wireLine, QGraphicsItem *excludeItem) const;

    QUndoStack           *m_undoStack;
    Mode                  m_mode       = SelectMode;
    int                   m_gridSize   = 20;
    bool                  m_gridVisible= true;
    bool                  m_drawing    = false;
    QPointF               m_startPt;

    QPen                  m_pen;
    QBrush                m_brush;
    QFont                 m_font;
    QColor                m_textColor  = Qt::black;
    ArrowItem::ArrowType  m_startArrow = ArrowItem::NoArrow;
    ArrowItem::ArrowType  m_endArrow   = ArrowItem::FilledArrow;
    QPixmap               m_symbol;
    QList<QPointF>        m_symbolPortsNormalized;
    QString               m_symbolName;

    QGraphicsItem        *m_tempItem   = nullptr;

    bool                  m_rightDupDragging = false;
    QGraphicsItem        *m_rightDupItem     = nullptr;
    QPointF               m_rightDupOffset;

    QVector<QGraphicsItem*> m_clipboard;
    QPointF                 m_pasteOffset;

    QRectF                  m_pageRect;
};

#endif // DIAGRAMSCENE_H