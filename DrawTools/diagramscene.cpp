#include "diagramscene.h"

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

DiagramScene::DiagramScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_mode(SelectMode)
    , m_gridSize(20)
    , m_gridVisible(true)
    , m_drawingItem(nullptr)
    , m_tempLine(nullptr)
    , m_tempText(nullptr)
{
    setSceneRect(-2000, -2000, 4000, 4000);
    setBackgroundBrush(QColor("#f7f6f2"));

    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(100);
}

void DiagramScene::setMode(Mode mode)
{
    m_mode = mode;

    if (m_tempText && mode != InsertTextMode) {
        if (m_tempText->toPlainText().trimmed().isEmpty())
            removeItem(m_tempText);
        else {
            m_undoStack->push(new AddItemCommand(this, m_tempText));
            emit itemInserted();
        }
        m_tempText = nullptr;
    }
}

void DiagramScene::setGridSize(int size)
{
    m_gridSize = qMax(5, size);
    update();
}

void DiagramScene::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

void DiagramScene::setCurrentSymbol(const QPixmap &pixmap)
{
    m_currentSymbol = pixmap;
}

// ──────────────────────────────────────────────
//  Grid
// ──────────────────────────────────────────────
void DiagramScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
    if (!m_gridVisible) return;

    painter->save();
    painter->setPen(QPen(QColor(200, 198, 194, 180), 0));

    qreal left   = std::floor(rect.left()  / m_gridSize) * m_gridSize;
    qreal top    = std::floor(rect.top()   / m_gridSize) * m_gridSize;
    qreal right  = rect.right();
    qreal bottom = rect.bottom();

    for (qreal x = left; x <= right; x += m_gridSize)
        painter->drawLine(QPointF(x, top), QPointF(x, bottom));
    for (qreal y = top; y <= bottom; y += m_gridSize)
        painter->drawLine(QPointF(left, y), QPointF(right, y));

    painter->setPen(QPen(QColor(170, 168, 163, 220), 0));
    int   major = m_gridSize * 5;
    qreal leftM = std::floor(rect.left() / major) * major;
    qreal topM  = std::floor(rect.top()  / major) * major;

    for (qreal x = leftM; x <= right; x += major)
        painter->drawLine(QPointF(x, top), QPointF(x, bottom));
    for (qreal y = topM; y <= bottom; y += major)
        painter->drawLine(QPointF(left, y), QPointF(right, y));

    painter->restore();
}

// ──────────────────────────────────────────────
//  Snap
// ──────────────────────────────────────────────
QPointF DiagramScene::snapToGrid(const QPointF &pos) const
{
    return {
        std::round(pos.x() / m_gridSize) * m_gridSize,
        std::round(pos.y() / m_gridSize) * m_gridSize
    };
}

// ──────────────────────────────────────────────
//  Mouse press
// ──────────────────────────────────────────────
void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    m_startPos = snapToGrid(event->scenePos());

    switch (m_mode) {

    case InsertRectMode: {
        auto *item = new QGraphicsRectItem(QRectF(m_startPos, QSizeF(0, 0)));
        item->setPen(QPen(Qt::black, 1.5));
        item->setBrush(QColor(220, 235, 255, 180));
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        addItem(item);
        m_drawingItem = item;
        break;
    }

    case InsertEllipseMode: {
        auto *item = new QGraphicsEllipseItem(QRectF(m_startPos, QSizeF(0, 0)));
        item->setPen(QPen(Qt::black, 1.5));
        item->setBrush(QColor(220, 255, 225, 180));
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        addItem(item);
        m_drawingItem = item;
        break;
    }

    case InsertLineMode: {
        auto *item = new QGraphicsLineItem(QLineF(m_startPos, m_startPos));
        item->setPen(QPen(Qt::black, 1.5, Qt::SolidLine, Qt::RoundCap));
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        addItem(item);
        m_tempLine = item;
        break;
    }

    case InsertTextMode: {
        // Commit any in-progress text first
        if (m_tempText) {
            if (m_tempText->toPlainText().trimmed().isEmpty())
                removeItem(m_tempText);
            else {
                m_undoStack->push(new AddItemCommand(this, m_tempText));
                emit itemInserted();
            }
            m_tempText = nullptr;
        }

        auto *item = new QGraphicsTextItem();
        item->setFont(QFont("Segoe UI", 11));
        item->setDefaultTextColor(Qt::black);
        item->setPos(m_startPos);
        item->setTextInteractionFlags(Qt::TextEditorInteraction);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        addItem(item);
        item->setFocus();
        m_tempText = item;
        break;
    }

    case InsertSymbolMode: {
        if (!m_currentSymbol.isNull()) {
            auto *item = new QGraphicsPixmapItem(m_currentSymbol);
            item->setPos(m_startPos - QPointF(m_currentSymbol.width()  / 2.0,
                                              m_currentSymbol.height() / 2.0));
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setFlag(QGraphicsItem::ItemIsMovable);
            addItem(item);
            // item already on scene → AddItemCommand skips first redo()
            m_undoStack->push(new AddItemCommand(this, item));
            emit itemInserted();
        }
        break;
    }

    case SelectMode:
    default:
        // Record position BEFORE base class handles the press
        // (base class may set the mouse grabber)
        QGraphicsScene::mousePressEvent(event);
        if (auto *grabbed = mouseGrabberItem())
            m_itemOldPos = grabbed->pos();
        break;
    }
}

// ──────────────────────────────────────────────
//  Mouse move
// ──────────────────────────────────────────────
void DiagramScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF cur = snapToGrid(event->scenePos());

    if (m_drawingItem) {
        QRectF r = QRectF(m_startPos, cur).normalized();
        if (auto *ri = qgraphicsitem_cast<QGraphicsRectItem *>(m_drawingItem))
            ri->setRect(r);
        else if (auto *ei = qgraphicsitem_cast<QGraphicsEllipseItem *>(m_drawingItem))
            ei->setRect(r);
        return;
    }

    if (m_tempLine) {
        m_tempLine->setLine(QLineF(m_startPos, cur));
        return;
    }

    QGraphicsScene::mouseMoveEvent(event);
}

// ──────────────────────────────────────────────
//  Mouse release
// ──────────────────────────────────────────────
void DiagramScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mouseReleaseEvent(event);
        return;
    }

    // ── Finish rect / ellipse ──────────────────
    if (m_drawingItem) {
        QRectF r;
        if (auto *ri = qgraphicsitem_cast<QGraphicsRectItem *>(m_drawingItem))
            r = ri->rect();
        else if (auto *ei = qgraphicsitem_cast<QGraphicsEllipseItem *>(m_drawingItem))
            r = ei->rect();

        if (r.width() < 4 && r.height() < 4) {
            removeItem(m_drawingItem);
            delete m_drawingItem;
        } else {
            // item already on scene → AddItemCommand skips first redo()
            m_undoStack->push(new AddItemCommand(this, m_drawingItem));
            emit itemInserted();
        }
        m_drawingItem = nullptr;
        return;
    }

    // ── Finish line ───────────────────────────
    if (m_tempLine) {
        if (m_tempLine->line().length() < 4) {
            removeItem(m_tempLine);
            delete m_tempLine;
        } else {
            m_undoStack->push(new AddItemCommand(this, m_tempLine));
            emit itemInserted();
        }
        m_tempLine = nullptr;
        return;
    }

    // ── Finish move (SelectMode) ──────────────
    // IMPORTANT: capture grabber BEFORE calling base class,
    // because base class releases the grab and mouseGrabberItem() → nullptr
    if (m_mode == SelectMode) {
        QGraphicsItem *movedItem = mouseGrabberItem();
        QPointF oldPos = m_itemOldPos;

        QGraphicsScene::mouseReleaseEvent(event);

        if (movedItem && movedItem->pos() != oldPos)
            m_undoStack->push(new MoveItemCommand(movedItem, oldPos));
        return;
    }

    QGraphicsScene::mouseReleaseEvent(event);
}

// ──────────────────────────────────────────────
//  Key press
// ──────────────────────────────────────────────
void DiagramScene::keyPressEvent(QKeyEvent *event)
{
    if (m_tempText) {
        bool commit  = (event->key() == Qt::Key_Escape);
        bool confirm = (event->key() == Qt::Key_Return &&
                        !(event->modifiers() & Qt::ShiftModifier));

        if (commit || confirm) {
            if (m_tempText->toPlainText().trimmed().isEmpty())
                removeItem(m_tempText);
            else {
                m_undoStack->push(new AddItemCommand(this, m_tempText));
                emit itemInserted();
            }
            m_tempText = nullptr;
            return;
        }
    }

    QGraphicsScene::keyPressEvent(event);
}