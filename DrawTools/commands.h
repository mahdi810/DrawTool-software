#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPointF>

// ── Add item ──────────────────────────────────────
class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(QGraphicsScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("Add Item", parent), m_scene(scene), m_item(item)
    {}
    ~AddItemCommand() override
    {
        if (!m_scene->items().contains(m_item))
            delete m_item;
    }
    void redo() override
    {
        if (!m_scene->items().contains(m_item))
            m_scene->addItem(m_item);
    }
    void undo() override
    {
        if (m_scene->items().contains(m_item))
            m_scene->removeItem(m_item);
    }

private:
    QGraphicsScene *m_scene;
    QGraphicsItem  *m_item;
};

// ── Remove item ───────────────────────────────────
class RemoveItemCommand : public QUndoCommand
{
public:
    RemoveItemCommand(QGraphicsScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("Remove Item", parent), m_scene(scene), m_item(item)
    {}
    ~RemoveItemCommand() override
    {
        if (!m_scene->items().contains(m_item))
            delete m_item;
    }
    void redo() override { m_scene->removeItem(m_item); }
    void undo() override { m_scene->addItem(m_item); }

private:
    QGraphicsScene *m_scene;
    QGraphicsItem  *m_item;
};

// ── Move item ─────────────────────────────────────
class MoveItemCommand : public QUndoCommand
{
public:
    MoveItemCommand(QGraphicsItem *item, const QPointF &oldPos, const QPointF &newPos,
                    QUndoCommand *parent = nullptr)
        : QUndoCommand("Move Item", parent)
        , m_item(item), m_oldPos(oldPos), m_newPos(newPos)
    {}
    void redo() override { m_item->setPos(m_newPos); }
    void undo() override { m_item->setPos(m_oldPos); }

private:
    QGraphicsItem *m_item;
    QPointF        m_oldPos, m_newPos;
};

#endif // COMMANDS_H