#include "diagramscene.h"
#include "commands.h"
#include "arrowitem.h"
#include "diagramrectitem.h"
#include "filemanager.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QInputDialog>
#include <QTextCursor>
#include <QKeyEvent>
#include <QPainter>
#include <QUndoStack>
#include <QJsonObject>
#include <QColor>
#include <cmath>

namespace {
constexpr int kSymbolComponentRole = 1001;
constexpr int kSymbolPortsRole = 1002;
constexpr int kSymbolRefRole = 1003;
constexpr int kSymbolValueRole = 1004;
constexpr int kSymbolLabelRole = 1005;
constexpr int kConnectionSnapRadius = 14;

qreal pointToSegmentDistanceSquared(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const qreal abx = b.x() - a.x();
    const qreal aby = b.y() - a.y();
    const qreal apx = p.x() - a.x();
    const qreal apy = p.y() - a.y();
    const qreal ab2 = abx * abx + aby * aby;
    if (ab2 < 1e-8) {
        const qreal dx = p.x() - a.x();
        const qreal dy = p.y() - a.y();
        return dx * dx + dy * dy;
    }
    const qreal t = qBound<qreal>(0.0, (apx * abx + apy * aby) / ab2, 1.0);
    const qreal qx = a.x() + t * abx;
    const qreal qy = a.y() + t * aby;
    const qreal dx = p.x() - qx;
    const qreal dy = p.y() - qy;
    return dx * dx + dy * dy;
}

QGraphicsItem *componentRoot(QGraphicsItem *it)
{
    QGraphicsItem *cur = it;
    while (cur && !cur->data(kSymbolComponentRole).toBool()) {
        cur = cur->parentItem();
    }
    return cur;
}

void updateComponentLabels(QGraphicsPixmapItem *item)
{
    if (!item) return;

    const QString ref = item->data(kSymbolRefRole).toString();
    const QString val = item->data(kSymbolValueRole).toString();

    QGraphicsSimpleTextItem *refText = nullptr;
    QGraphicsSimpleTextItem *valText = nullptr;

    const auto children = item->childItems();
    for (QGraphicsItem *c : children) {
        if (!c) continue;
        if (auto *st = qgraphicsitem_cast<QGraphicsSimpleTextItem *>(c)) {
            const QString role = st->data(kSymbolLabelRole).toString();
            if (role == QStringLiteral("ref")) refText = st;
            else if (role == QStringLiteral("val")) valText = st;
        }
    }

    if (!refText) {
        refText = new QGraphicsSimpleTextItem(item);
        refText->setFlag(QGraphicsItem::ItemIsSelectable, false);
        refText->setFlag(QGraphicsItem::ItemIsMovable, false);
        refText->setData(kSymbolLabelRole, QStringLiteral("ref"));
    }
    if (!valText) {
        valText = new QGraphicsSimpleTextItem(item);
        valText->setFlag(QGraphicsItem::ItemIsSelectable, false);
        valText->setFlag(QGraphicsItem::ItemIsMovable, false);
        valText->setData(kSymbolLabelRole, QStringLiteral("val"));
    }

    QFont f("Consolas", 7);
    refText->setFont(f);
    valText->setFont(f);
    refText->setBrush(QBrush(QColor(40, 40, 120)));
    valText->setBrush(QBrush(QColor(80, 80, 80)));
    refText->setText(ref);
    valText->setText(val);

    const QSizeF logicalSize = item->pixmap().deviceIndependentSize();
    const qreal w = logicalSize.width()  > 0.0 ? logicalSize.width()  : static_cast<qreal>(item->pixmap().width());
    const qreal h = logicalSize.height() > 0.0 ? logicalSize.height() : static_cast<qreal>(item->pixmap().height());

    const QRectF refBr = refText->boundingRect();
    const QRectF valBr = valText->boundingRect();
    refText->setPos((w - refBr.width()) * 0.5, -refBr.height() - 3.0);
    valText->setPos((w - valBr.width()) * 0.5, h + 2.0);
}
}

DiagramScene::DiagramScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_undoStack(new QUndoStack(this))
{
    // Default page: A4 at ~96 DPI, centered at origin.
    m_pageRect = QRectF(-397.0, -561.5, 794.0, 1123.0);
    setSceneRect(m_pageRect.adjusted(-600, -600, 600, 600));

    m_pen.setColor(Qt::black);
    m_pen.setWidthF(1.5);
    m_pen.setCapStyle(Qt::RoundCap);

    m_brush = QBrush(QColor(220, 235, 255, 180));

    m_font.setFamily("Segoe UI");
    m_font.setPointSize(11);
}

void DiagramScene::setPageRect(const QRectF &rect)
{
    if (!rect.isValid() || rect.isEmpty()) return;
    m_pageRect = rect;
    setSceneRect(m_pageRect.adjusted(-600, -600, 600, 600));
    update();
}

void DiagramScene::setCurrentSymbol(const QPixmap &px,
                                    const QList<QPointF> &portsNormalized,
                                    const QString &symbolName)
{
    m_symbol = px;
    m_symbolPortsNormalized = portsNormalized;
    m_symbolName = symbolName;
}

void DiagramScene::setMode(Mode mode)
{
    m_mode = mode;
    emit modeChanged(mode);
}

void DiagramScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_mode == SelectMode) {
        if (QGraphicsItem *it = itemAt(event->scenePos(), QTransform())) {
            if (QGraphicsItem *root = componentRoot(it)) {
                if (auto *pi = qgraphicsitem_cast<QGraphicsPixmapItem *>(root)) {
                    bool ok = false;
                    const QString ref0 = pi->data(kSymbolRefRole).toString();
                    const QString val0 = pi->data(kSymbolValueRole).toString();
                    const QString ref = QInputDialog::getText(nullptr,
                                                              "Component Properties",
                                                              "Reference:",
                                                              QLineEdit::Normal,
                                                              ref0,
                                                              &ok).trimmed();
                    if (!ok || ref.isEmpty()) {
                        event->accept();
                        return;
                    }
                    const QString val = QInputDialog::getText(nullptr,
                                                              "Component Properties",
                                                              "Value:",
                                                              QLineEdit::Normal,
                                                              val0,
                                                              &ok).trimmed();
                    if (!ok) {
                        event->accept();
                        return;
                    }

                    pi->setData(kSymbolRefRole, ref);
                    pi->setData(kSymbolValueRole, val);
                    updateComponentLabels(pi);
                    event->accept();
                    return;
                }
            }

            if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(it)) {
                ti->setTextInteractionFlags(Qt::TextEditorInteraction);
                ti->setFocus(Qt::MouseFocusReason);

                QTextCursor c = ti->textCursor();
                c.movePosition(QTextCursor::End);
                ti->setTextCursor(c);

                event->accept();
                return;
            }
        }
    }
    QGraphicsScene::mouseDoubleClickEvent(event);
}

// ── Grid drawing ──────────────────────────────────
void DiagramScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);

    const QRectF page = m_pageRect.isValid() ? m_pageRect : rect;
    painter->fillRect(rect, QColor(235, 235, 235));
    painter->fillRect(page, Qt::white);

    QPen pageBorderPen(QColor(175, 175, 175), 1.0);
    painter->setPen(pageBorderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(page);

    if (!m_gridVisible || m_gridSize <= 0) return;

    QPen minorPen(QColor(228, 228, 228), 1);
    QPen majorPen(QColor(205, 205, 205), 1);

    QRectF gridRect = rect.intersected(page);
    if (!gridRect.isValid() || gridRect.isEmpty()) return;

    int left   = static_cast<int>(std::floor(gridRect.left()   / m_gridSize)) * m_gridSize;
    int top    = static_cast<int>(std::floor(gridRect.top()    / m_gridSize)) * m_gridSize;
    int right  = static_cast<int>(std::ceil (gridRect.right()  / m_gridSize)) * m_gridSize;
    int bottom = static_cast<int>(std::ceil (gridRect.bottom() / m_gridSize)) * m_gridSize;

    const int majorStep = m_gridSize * 5;

    for (int x = left; x <= right; x += m_gridSize) {
        const bool major = (majorStep > 0) && (x % majorStep == 0);
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(x, top, x, bottom);
    }

    for (int y = top; y <= bottom; y += m_gridSize) {
        const bool major = (majorStep > 0) && (y % majorStep == 0);
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(left, y, right, y);
    }
}

QPointF DiagramScene::snapToGrid(const QPointF &pt) const
{
    if (m_gridSize <= 0) return pt;
    qreal x = qRound(pt.x() / m_gridSize) * m_gridSize;
    qreal y = qRound(pt.y() / m_gridSize) * m_gridSize;
    return {x, y};
}

QPointF DiagramScene::snapToConnectionPoint(const QPointF &pt) const
{
    const QRectF searchRect(pt.x() - kConnectionSnapRadius,
                            pt.y() - kConnectionSnapRadius,
                            2.0 * kConnectionSnapRadius,
                            2.0 * kConnectionSnapRadius);

    bool found = false;
    qreal bestDist2 = static_cast<qreal>(kConnectionSnapRadius * kConnectionSnapRadius) + 1.0;
    QPointF best = pt;

    const auto nearby = items(searchRect, Qt::IntersectsItemBoundingRect, Qt::DescendingOrder, QTransform());
    for (QGraphicsItem *it : nearby) {
        if (!it) continue;

        if (it->data(kSymbolComponentRole).toBool()) {
            const QVariantList ports = it->data(kSymbolPortsRole).toList();
            for (const QVariant &v : ports) {
                const QPointF local = v.toPointF();
                const QPointF scenePt = it->mapToScene(local);
                const qreal dx = scenePt.x() - pt.x();
                const qreal dy = scenePt.y() - pt.y();
                const qreal d2 = dx * dx + dy * dy;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    best = scenePt;
                    found = true;
                }
            }
        } else if (it->data(1).toString() == QStringLiteral("junctionDot")) {
            const QPointF scenePt = it->scenePos();
            const qreal dx = scenePt.x() - pt.x();
            const qreal dy = scenePt.y() - pt.y();
            const qreal d2 = dx * dx + dy * dy;
            if (d2 < bestDist2) {
                bestDist2 = d2;
                best = scenePt;
                found = true;
            }
        }
    }

    return found ? best : snapToGrid(pt);
}

QPointF DiagramScene::constrainOrthogonal(const QPointF &from, const QPointF &to) const
{
    const qreal dx = std::abs(to.x() - from.x());
    const qreal dy = std::abs(to.y() - from.y());
    if (dx >= dy) {
        return QPointF(to.x(), from.y());
    }
    return QPointF(from.x(), to.y());
}

QList<QGraphicsItem *> DiagramScene::makeAutoJunctionsForWire(const QLineF &wireLine, QGraphicsItem *excludeItem) const
{
    QList<QGraphicsItem *> dots;
    const QList<QPointF> endpoints = {wireLine.p1(), wireLine.p2()};
    constexpr qreal hitRadius = 4.5;
    const qreal hitRadius2 = hitRadius * hitRadius;

    for (const QPointF &pt : endpoints) {
        bool dotExists = false;
        int lineConnections = 0;
        int componentConnections = 0;

        const QRectF area(pt.x() - kConnectionSnapRadius,
                          pt.y() - kConnectionSnapRadius,
                          2.0 * kConnectionSnapRadius,
                          2.0 * kConnectionSnapRadius);
        const auto nearby = items(area, Qt::IntersectsItemBoundingRect, Qt::DescendingOrder, QTransform());
        for (QGraphicsItem *it : nearby) {
            if (!it || it == excludeItem) continue;

            if (it->data(1).toString() == QStringLiteral("junctionDot")) {
                const QPointF jp = it->scenePos();
                const qreal dx = jp.x() - pt.x();
                const qreal dy = jp.y() - pt.y();
                if (dx * dx + dy * dy <= hitRadius2) {
                    dotExists = true;
                    break;
                }
                continue;
            }

            if (auto *ai = dynamic_cast<ArrowItem *>(it)) {
                const QPointF a = ai->mapToScene(ai->line().p1());
                const QPointF b = ai->mapToScene(ai->line().p2());
                if (pointToSegmentDistanceSquared(pt, a, b) <= hitRadius2) {
                    ++lineConnections;
                }
                continue;
            }

            if (auto *li = qgraphicsitem_cast<QGraphicsLineItem *>(it)) {
                const QPointF a = li->mapToScene(li->line().p1());
                const QPointF b = li->mapToScene(li->line().p2());
                if (pointToSegmentDistanceSquared(pt, a, b) <= hitRadius2) {
                    ++lineConnections;
                }
                continue;
            }

            if (it->data(kSymbolComponentRole).toBool()) {
                const QVariantList ports = it->data(kSymbolPortsRole).toList();
                for (const QVariant &v : ports) {
                    const QPointF scenePt = it->mapToScene(v.toPointF());
                    const qreal dx = scenePt.x() - pt.x();
                    const qreal dy = scenePt.y() - pt.y();
                    if (dx * dx + dy * dy <= hitRadius2) {
                        ++componentConnections;
                        break;
                    }
                }
            }
        }

        if (dotExists) continue;
        const bool shouldCreate = (lineConnections >= 1) || ((lineConnections + componentConnections) >= 2);
        if (!shouldCreate) continue;

        bool duplicateLocal = false;
        for (QGraphicsItem *d : dots) {
            const QPointF jp = d->scenePos();
            const qreal dx = jp.x() - pt.x();
            const qreal dy = jp.y() - pt.y();
            if (dx * dx + dy * dy <= hitRadius2) {
                duplicateLocal = true;
                break;
            }
        }
        if (duplicateLocal) continue;

        auto *dot = new QGraphicsEllipseItem(QRectF(-4.0, -4.0, 8.0, 8.0));
        dot->setPen(QPen(Qt::black, 1.0));
        dot->setBrush(QBrush(Qt::black));
        dot->setData(1, QStringLiteral("junctionDot"));
        dot->setFlag(QGraphicsItem::ItemIsSelectable);
        dot->setFlag(QGraphicsItem::ItemIsMovable);
        dot->setPos(pt);
        dots.append(dot);
    }

    return dots;
}

// ── Mouse Press ───────────────────────────────────
void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton && m_mode == SelectMode) {
        if (QGraphicsItem *src = itemAt(event->scenePos(), QTransform())) {
            const QJsonObject obj = FileManager::itemToJson(src);
            if (!obj.isEmpty()) {
                if (QGraphicsItem *copy = FileManager::itemFromJson(obj)) {
                    addItem(copy);
                    clearSelection();
                    copy->setSelected(true);

                    m_rightDupDragging = true;
                    m_rightDupItem = copy;
                    m_rightDupOffset = event->scenePos() - copy->pos();

                    event->accept();
                    return;
                }
            }
        }
    }

    if (m_mode == SelectMode && event->button() == Qt::LeftButton) {
        QGraphicsItem *hit = itemAt(event->scenePos(), QTransform());
        auto *hitText = qgraphicsitem_cast<QGraphicsTextItem *>(hit);

        const auto all = items();
        for (QGraphicsItem *it : all) {
            if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(it)) {
                if (ti != hitText) {
                    ti->setTextInteractionFlags(Qt::NoTextInteraction);
                    ti->clearFocus();
                }
            }
        }
    }

    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    QPointF pt = (m_mode == InsertLineMode || m_mode == InsertArrowMode)
                     ? snapToConnectionPoint(event->scenePos())
                     : snapToGrid(event->scenePos());

    if (m_mode == SelectMode) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    if (m_mode == InsertSymbolMode) {
        if (!m_symbol.isNull()) {
            auto *item = new QGraphicsPixmapItem(m_symbol);
            const QSizeF logicalSize = m_symbol.deviceIndependentSize();
            const qreal w = logicalSize.width()  > 0.0 ? logicalSize.width()  : static_cast<qreal>(m_symbol.width());
            const qreal h = logicalSize.height() > 0.0 ? logicalSize.height() : static_cast<qreal>(m_symbol.height());
            item->setPos(pt - QPointF(w / 2.0, h / 2.0));
            item->setScale(0.78);
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setFlag(QGraphicsItem::ItemIsMovable);
            item->setTransformationMode(Qt::SmoothTransformation);

            QVariantList portsLocal;
            for (const QPointF &np : m_symbolPortsNormalized) {
                portsLocal << QPointF(np.x() * w, np.y() * h);
            }
            item->setData(kSymbolComponentRole, true);
            item->setData(kSymbolPortsRole, portsLocal);
            item->setData(kSymbolRefRole, QStringLiteral("U?"));
            item->setData(kSymbolValueRole, m_symbolName.isEmpty() ? QStringLiteral("COMP") : m_symbolName);
            updateComponentLabels(item);

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
        ti->setTextInteractionFlags(Qt::NoTextInteraction);
        ti->setFlag(QGraphicsItem::ItemIsSelectable);
        ti->setFlag(QGraphicsItem::ItemIsMovable);
        ti->setPos(pt);
        m_undoStack->push(new AddItemCommand(this, ti));
        emit itemInserted(ti);
        return;
    }

    if (m_mode == InsertJunctionDotMode) {
        constexpr qreal d = 8.0;
        auto *ei = new QGraphicsEllipseItem(QRectF(-d / 2.0, -d / 2.0, d, d));
        ei->setPen(QPen(Qt::black, 1.0));
        ei->setBrush(QBrush(Qt::black));
        ei->setData(1, QStringLiteral("junctionDot"));
        ei->setFlag(QGraphicsItem::ItemIsSelectable);
        ei->setFlag(QGraphicsItem::ItemIsMovable);
        ei->setPos(pt);
        m_undoStack->push(new AddItemCommand(this, ei));
        emit itemInserted(ei);
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
        QPen arrowPen = m_pen;
        // Guard against invalid/transparent/disabled pen states.
        if (arrowPen.style() == Qt::NoPen || arrowPen.widthF() <= 0.0 || arrowPen.color().alpha() == 0) {
            arrowPen = QPen(Qt::black, 1.5);
            arrowPen.setCapStyle(Qt::RoundCap);
        }
        ai->setPen(arrowPen);
        ai->setStartArrow(m_startArrow);
        ai->setEndArrow(m_endArrow);
        ai->setFlag(QGraphicsItem::ItemIsSelectable, false);
        ai->setFlag(QGraphicsItem::ItemIsMovable, false);
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
    if (m_rightDupDragging && m_rightDupItem) {
        const QPointF pt = snapToGrid(event->scenePos() - m_rightDupOffset);
        m_rightDupItem->setPos(pt);
        event->accept();
        return;
    }

    if (!m_drawing || !m_tempItem) {
        QGraphicsScene::mouseMoveEvent(event);
        return;
    }

    QPointF pt = (m_mode == InsertLineMode || m_mode == InsertArrowMode)
                     ? snapToConnectionPoint(event->scenePos())
                     : snapToGrid(event->scenePos());

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
            li->setLine(QLineF(m_startPt, constrainOrthogonal(m_startPt, pt)));
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
    if (event->button() == Qt::RightButton && m_rightDupDragging) {
        m_rightDupDragging = false;
        if (m_rightDupItem) {
            removeItem(m_rightDupItem);
            m_undoStack->push(new AddItemCommand(this, m_rightDupItem));
            emit itemInserted(m_rightDupItem);
            m_rightDupItem->setSelected(true);
            m_rightDupItem = nullptr;
        }
        event->accept();
        return;
    }

    if (!m_drawing || !m_tempItem) {
        QGraphicsScene::mouseReleaseEvent(event);

        if (m_mode == SelectMode && event->button() == Qt::LeftButton) {
            const auto sel = selectedItems();
            for (QGraphicsItem *item : sel) {
                if (!item || !(item->flags() & QGraphicsItem::ItemIsMovable)) continue;
                const QPointF snapped = snapToGrid(item->pos());
                if (snapped != item->pos())
                    item->setPos(snapped);
            }
        }

        return;
    }
    m_drawing = false;

    // Final endpoint snap/constraints at release position.
    if (m_mode == InsertLineMode) {
        if (auto *li = qgraphicsitem_cast<QGraphicsLineItem *>(m_tempItem)) {
            QPointF end = snapToConnectionPoint(event->scenePos());
            end = constrainOrthogonal(m_startPt, end);
            li->setLine(QLineF(m_startPt, end));
        }
    } else if (m_mode == InsertArrowMode) {
        if (auto *ai = dynamic_cast<ArrowItem *>(m_tempItem)) {
            const QPointF end = snapToConnectionPoint(event->scenePos());
            ai->setLine(QLineF(m_startPt, end));
        }
    }

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

    if (m_mode == InsertLineMode) {
        if (auto *li = qgraphicsitem_cast<QGraphicsLineItem *>(m_tempItem)) {
            if (li->line().length() < 4.0) {
                removeItem(m_tempItem);
                delete m_tempItem;
                m_tempItem = nullptr;
                return;
            }
        }
    }

    if (m_mode == InsertArrowMode) {
        if (auto *ai = dynamic_cast<ArrowItem *>(m_tempItem)) {
            if (ai->line().length() < 4.0) {
                removeItem(m_tempItem);
                delete m_tempItem;
                m_tempItem = nullptr;
                return;
            }
        }
    }

    if (m_mode == InsertArrowMode) {
        if (auto *ai = dynamic_cast<ArrowItem *>(m_tempItem)) {
            ai->setFlag(QGraphicsItem::ItemIsSelectable, true);
            ai->setFlag(QGraphicsItem::ItemIsMovable, true);
        }
    }

    if (m_mode == InsertLineMode) {
        auto *li = qgraphicsitem_cast<QGraphicsLineItem *>(m_tempItem);
        if (li) {
            const QLineF sceneLine(li->mapToScene(li->line().p1()),
                                   li->mapToScene(li->line().p2()));
            const auto autoDots = makeAutoJunctionsForWire(sceneLine, li);

            m_undoStack->beginMacro("Add Wire");
            m_undoStack->push(new AddItemCommand(this, m_tempItem));
            emit itemInserted(m_tempItem);
            for (QGraphicsItem *dot : autoDots) {
                m_undoStack->push(new AddItemCommand(this, dot));
            }
            m_undoStack->endMacro();

            m_tempItem = nullptr;
            return;
        }
    }

    // Register draw action for undo without removing the currently released item.
    m_undoStack->push(new AddItemCommand(this, m_tempItem));
    emit itemInserted(m_tempItem);
    m_tempItem = nullptr;
}

// ── Key Press ─────────────────────────────────────
void DiagramScene::keyPressEvent(QKeyEvent *event)
{
    if (QGraphicsItem *fi = focusItem()) {
        if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(fi)) {
            if (ti->textInteractionFlags() != Qt::NoTextInteraction) {
                QGraphicsScene::keyPressEvent(event);
                return;
            }
        }
    }

    if (event->key() == Qt::Key_Escape) {
        bool edited = false;
        const auto all = items();
        for (QGraphicsItem *it : all) {
            if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(it)) {
                if (ti->textInteractionFlags() != Qt::NoTextInteraction) {
                    ti->setTextInteractionFlags(Qt::NoTextInteraction);
                    ti->clearFocus();
                    edited = true;
                }
            }
        }
        if (edited) {
            clearFocus();
            event->accept();
            return;
        }
    }

    if ((event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space) && m_mode != SelectMode) {
        setMode(SelectMode);
        event->accept();
        return;
    }

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

    const auto selected = selectedItems();
    if (!selected.isEmpty() && event->key() == Qt::Key_R) {
        for (auto *item : selected) {
            if (!item) continue;
            item->setTransformOriginPoint(item->boundingRect().center());
            item->setRotation(item->rotation() + 90.0);
        }
        event->accept();
        return;
    }

    const bool zoomInKey  = (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal);
    const bool zoomOutKey = (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore);
    if (!selected.isEmpty() && (zoomInKey || zoomOutKey)) {
        const qreal factor = zoomInKey ? 1.1 : 1.0 / 1.1;
        for (auto *item : selected) {
            if (item->data(kSymbolComponentRole).toBool()) {
                continue;
            }
            item->setTransformOriginPoint(item->boundingRect().center());
            const qreal current = item->scale();
            const qreal next = qBound<qreal>(0.1, current * factor, 20.0);
            item->setScale(next);
        }
        event->accept();
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