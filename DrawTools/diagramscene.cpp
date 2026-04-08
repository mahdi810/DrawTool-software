#include "diagramscene.h"
#include "commands.h"
#include "arrowitem.h"
#include "diagramrectitem.h"
#include "filemanager.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QPainter>
#include <QUndoStack>
#include <QJsonObject>
#include <QColor>
#include <cmath>

DiagramScene::DiagramScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_undoStack(new QUndoStack(this))
{
    setSceneRect(-2000, -2000, 4000, 4000);

    m_pen.setColor(Qt::black);
    m_pen.setWidthF(1.5);
    m_pen.setCapStyle(Qt::RoundCap);

    m_brush = QBrush(QColor(220, 235, 255, 180));

    m_font.setFamily("Segoe UI");
    m_font.setPointSize(11);
}

void DiagramScene::setMode(Mode mode)
{
    m_mode = mode;
    emit modeChanged(mode);
}

// ── Grid drawing ──────────────────────────────────
void DiagramScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
    painter->fillRect(rect, QColor(250, 250, 250));

    if (!m_gridVisible || m_gridSize <= 0) return;

    QPen dotPen(QColor(210, 210, 210), 1);
    painter->setPen(dotPen);

    int left   = static_cast<int>(std::floor(rect.left()   / m_gridSize)) * m_gridSize;
    int top    = static_cast<int>(std::floor(rect.top()    / m_gridSize)) * m_gridSize;
    int right  = static_cast<int>(std::ceil (rect.right()  / m_gridSize)) * m_gridSize;
    int bottom = static_cast<int>(std::ceil (rect.bottom() / m_gridSize)) * m_gridSize;

    for (int x = left; x <= right; x += m_gridSize)
        for (int y = top; y <= bottom; y += m_gridSize)
            painter->drawPoint(x, y);
}

QPointF DiagramScene::snapToGrid(const QPointF &pt) const
{
    if (m_gridSize <= 0) return pt;
    qreal x = qRound(pt.x() / m_gridSize) * m_gridSize;
    qreal y = qRound(pt.y() / m_gridSize) * m_gridSize;
    return {x, y};
}

// ── Mouse Press ───────────────────────────────────
void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    QPointF pt = snapToGrid(event->scenePos());

    if (m_mode == SelectMode) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    if (m_mode == InsertSymbolMode) {
        if (!m_symbol.isNull()) {
            auto *item = new QGraphicsPixmapItem(m_symbol);
            item->setPos(pt - QPointF(m_symbol.width() / 2.0, m_symbol.height() / 2.0));
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setFlag(QGraphicsItem::ItemIsMovable);
            m_undoStack->push(new AddItemCommand(this, item));
            emit itemInserted(item);
        }
        return;
    }

    if (m_mode == InsertTextMode) {
        auto *ti = new QGraphicsTextItem;
        ti->setPlainText("Text");
        ti->setFont(m_font);
        ti->setDefaultTextColor(m_textColor);
        ti->setTextInteractionFlags(Qt::TextEditorInteraction);
        ti->setFlag(QGraphicsItem::ItemIsSelectable);
        ti->setFlag(QGraphicsItem::ItemIsMovable);
        ti->setPos(pt);
        m_undoStack->push(new AddItemCommand(this, ti));
        emit itemInserted(ti);
        return;
    }

    m_drawing  = true;
    m_startPt  = pt;
    m_tempItem = nullptr;

    switch (m_mode) {
    case InsertRectMode: {
        auto *ri = new DiagramRectItem(QRectF(pt, QSizeF(1, 1)));
        ri->setPen(m_pen); ri->setBrush(m_brush);
        addItem(ri);
        m_tempItem = ri;
        break;
    }
    case InsertEllipseMode: {
        auto *ei = new QGraphicsEllipseItem(QRectF(pt, QSizeF(1, 1)));
        ei->setPen(m_pen); ei->setBrush(m_brush);
        ei->setFlag(QGraphicsItem::ItemIsSelectable);
        ei->setFlag(QGraphicsItem::ItemIsMovable);
        addItem(ei);
        m_tempItem = ei;
        break;
    }
    case InsertLineMode: {
        auto *li = new QGraphicsLineItem(QLineF(pt, pt));
        li->setPen(m_pen);
        li->setFlag(QGraphicsItem::ItemIsSelectable);
        li->setFlag(QGraphicsItem::ItemIsMovable);
        addItem(li);
        m_tempItem = li;
        break;
    }
    case InsertArrowMode: {
        auto *ai = new ArrowItem(QLineF(pt, pt));
        ai->setPen(m_pen);
        ai->setStartArrow(m_startArrow);
        ai->setEndArrow(m_endArrow);
        addItem(ai);
        m_tempItem = ai;
        break;
    }
    default: break;
    }
}

// ── Mouse Move ────────────────────────────────────
void DiagramScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_drawing || !m_tempItem) {
        QGraphicsScene::mouseMoveEvent(event);
        return;
    }

    QPointF pt = snapToGrid(event->scenePos());

    switch (m_mode) {
    case InsertRectMode:
        if (auto *ri = dynamic_cast<DiagramRectItem *>(m_tempItem))
            ri->setRect(QRectF(m_startPt, pt).normalized());
        break;
    case InsertEllipseMode:
        if (auto *ei = qgraphicsitem_cast<QGraphicsEllipseItem *>(m_tempItem))
            ei->setRect(QRectF(m_startPt, pt).normalized());
        break;
    case InsertLineMode:
        if (auto *li = qgraphicsitem_cast<QGraphicsLineItem *>(m_tempItem))
            li->setLine(QLineF(m_startPt, pt));
        break;
    case InsertArrowMode:
        if (auto *ai = dynamic_cast<ArrowItem *>(m_tempItem))
            ai->setLine(QLineF(m_startPt, pt));
        break;
    default: break;
    }
}

// ── Mouse Release ─────────────────────────────────
void DiagramScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_drawing || !m_tempItem) {
        QGraphicsScene::mouseReleaseEvent(event);
        return;
    }
    m_drawing = false;

    // Remove if too small
    QRectF br = m_tempItem->boundingRect();
    if (br.width() < 4 && br.height() < 4 &&
        m_mode != InsertLineMode && m_mode != InsertArrowMode)
    {
        removeItem(m_tempItem);
        delete m_tempItem;
        m_tempItem = nullptr;
        return;
    }

    // Push to undo stack (item already in scene, use a remove/add pair)
    removeItem(m_tempItem);
    m_undoStack->push(new AddItemCommand(this, m_tempItem));
    emit itemInserted(m_tempItem);
    m_tempItem = nullptr;
}

// ── Key Press ─────────────────────────────────────
void DiagramScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        const auto sel = selectedItems();
        if (!sel.isEmpty()) {
            m_undoStack->beginMacro("Delete Items");
            for (auto *item : sel)
                m_undoStack->push(new RemoveItemCommand(this, item));
            m_undoStack->endMacro();
        }
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

// ── Clipboard ─────────────────────────────────────
void DiagramScene::copySelection(bool cut)
{
    for (auto *item : m_clipboard) {
        if (!items().contains(item)) delete item;
    }
    m_clipboard.clear();
    m_pasteOffset = QPointF(20, 20);

    const auto sel = selectedItems();
    for (auto *item : sel) {
        QJsonObject obj = FileManager::itemToJson(item);
        QGraphicsItem *copy = FileManager::itemFromJson(obj);
        if (copy) m_clipboard.append(copy);
        if (cut) m_undoStack->push(new RemoveItemCommand(this, item));
    }
}

void DiagramScene::paste()
{
    if (m_clipboard.isEmpty()) return;
    clearSelection();
    m_undoStack->beginMacro("Paste");
    for (auto *item : m_clipboard) {
        QJsonObject obj = FileManager::itemToJson(item);
        QGraphicsItem *copy = FileManager::itemFromJson(obj);
        if (!copy) continue;
        copy->setPos(item->pos() + m_pasteOffset);
        m_undoStack->push(new AddItemCommand(this, copy));
        copy->setSelected(true);
    }
    m_undoStack->endMacro();
    m_pasteOffset += QPointF(20, 20);
}

void DiagramScene::duplicate()
{
    const auto sel = selectedItems();
    if (sel.isEmpty()) return;
    clearSelection();
    m_undoStack->beginMacro("Duplicate");
    for (auto *item : sel) {
        QJsonObject obj = FileManager::itemToJson(item);
        QGraphicsItem *copy = FileManager::itemFromJson(obj);
        if (!copy) continue;
        copy->setPos(item->pos() + QPointF(20, 20));
        m_undoStack->push(new AddItemCommand(this, copy));
        copy->setSelected(true);
    }
    m_undoStack->endMacro();
}

void DiagramScene::selectAll()
{
    const auto all = items();
    for (auto *item : all) item->setSelected(true);
}

// ── Z-order ───────────────────────────────────────
void DiagramScene::bringToFront()
{
    qreal maxZ = 0;
    const auto all = items();
    for (auto *i : all) maxZ = qMax(maxZ, i->zValue());
    for (auto *i : selectedItems()) i->setZValue(maxZ + 1);
}

void DiagramScene::sendToBack()
{
    qreal minZ = 0;
    const auto all = items();
    for (auto *i : all) minZ = qMin(minZ, i->zValue());
    for (auto *i : selectedItems()) i->setZValue(minZ - 1);
}

void DiagramScene::bringForward()
{
    for (auto *i : selectedItems()) i->setZValue(i->zValue() + 1);
}

void DiagramScene::sendBackward()
{
    for (auto *i : selectedItems()) i->setZValue(i->zValue() - 1);
}