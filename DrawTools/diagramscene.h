#ifndef DIAGRAMSCENE_H
#define DIAGRAMSCENE_H

#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QPixmap>
#include <QKeyEvent>
#include <QUndoStack>
#include "commands.h"

class DiagramScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum Mode {
        SelectMode,
        InsertRectMode,
        InsertEllipseMode,
        InsertLineMode,
        InsertTextMode,
        InsertSymbolMode
    };

    explicit DiagramScene(QObject *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setGridSize(int size);
    int  gridSize()    const { return m_gridSize; }

    void setGridVisible(bool visible);
    bool gridVisible() const { return m_gridVisible; }

    void setCurrentSymbol(const QPixmap &pixmap);

    QUndoStack *undoStack() const { return m_undoStack; }

signals:
    void itemInserted();

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;

    void mousePressEvent  (QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent   (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent    (QKeyEvent                *event) override;

private:
    QPointF snapToGrid(const QPointF &pos) const;

    Mode    m_mode;
    int     m_gridSize;
    bool    m_gridVisible;
    QPointF m_startPos;
    QPointF m_itemOldPos;
    QPixmap m_currentSymbol;

    QGraphicsItem     *m_drawingItem;
    QGraphicsLineItem *m_tempLine;
    QGraphicsTextItem *m_tempText;

    QUndoStack *m_undoStack;
};

#endif // DIAGRAMSCENE_H