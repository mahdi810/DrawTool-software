#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include <QGraphicsScene>
#include <QGraphicsItem>

// ── Add item ──────────────────────────────────────
// NOTE: item must already be on the scene when this command is pushed.
// The first redo() is skipped because the item is already there.
class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(QGraphicsScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("Add Item", parent)
        , m_scene(scene)
        , m_item(item)
        , m_owned(false)
        , m_firstRedo(true)   // skip first redo — item already on scene
    {}

    ~AddItemCommand() override
    {
        if (m_owned) delete m_item;
    }

    void undo() override
    {
        m_scene->removeItem(m_item);
        m_owned = true;
    }

    void redo() override
    {
        if (m_firstRedo) {
            // Item was already added before the command was pushed
            m_firstRedo = false;
            m_owned = false;
            return;
        }
        m_scene->addItem(m_item);
        m_owned = false;
    }

private:
    QGraphicsScene *m_scene;
    QGraphicsItem  *m_item;
    bool            m_owned;
    bool            m_firstRedo;
};

// ── Remove item ───────────────────────────────────
class RemoveItemCommand : public QUndoCommand
{
public:
    RemoveItemCommand(QGraphicsScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("Remove Item", parent)
        , m_scene(scene)
        , m_item(item)
        , m_owned(false)
    {}

    ~RemoveItemCommand() override
    {
        if (m_owned) delete m_item;
    }

    void undo() override
    {
        m_scene->addItem(m_item);
        m_owned = false;
    }

    void redo() override
    {
        m_scene->removeItem(m_item);
        m_owned = true;
    }

private:
    QGraphicsScene *m_scene;
    QGraphicsItem  *m_item;
    bool            m_owned;
};

// ── Move item ─────────────────────────────────────
class MoveItemCommand : public QUndoCommand
{
public:
    MoveItemCommand(QGraphicsItem *item, const QPointF &oldPos, QUndoCommand *parent = nullptr)
        : QUndoCommand("Move Item", parent)
        , m_item(item)
        , m_oldPos(oldPos)
        , m_newPos(item->pos())
    {}

    void undo() override { m_item->setPos(m_oldPos); }
    void redo() override { m_item->setPos(m_newPos); }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *o = static_cast<const MoveItemCommand *>(other);
        if (o->m_item != m_item) return false;
        m_newPos = o->m_newPos;
        return true;
    }
    int id() const override { return 1; }

private:
    QGraphicsItem *m_item;
    QPointF        m_oldPos;
    QPointF        m_newPos;
};

#endif // COMMANDS_H