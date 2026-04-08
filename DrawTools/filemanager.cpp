#include "filemanager.h"
#include "arrowitem.h"
#include "diagramrectitem.h"
#include "resizablepixmapitem.h"
#include "resizableequationitem.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QByteArray>
#include <QBuffer>
#include <QFont>
#include <QColor>

namespace {
constexpr int kSymbolComponentRole = 1001;
constexpr int kSymbolPortsRole = 1002;
constexpr int kSymbolRefRole = 1003;
constexpr int kSymbolValueRole = 1004;
constexpr int kSymbolLabelRole = 1005;

void updateComponentLabels(QGraphicsPixmapItem *item)
{
    if (!item) return;

    const QString ref = item->data(kSymbolRefRole).toString();
    const QString val = item->data(kSymbolValueRole).toString();

    QGraphicsSimpleTextItem *refText = nullptr;
    QGraphicsSimpleTextItem *valText = nullptr;
    for (QGraphicsItem *c : item->childItems()) {
        if (auto *st = qgraphicsitem_cast<QGraphicsSimpleTextItem *>(c)) {
            const QString role = st->data(kSymbolLabelRole).toString();
            if (role == QStringLiteral("ref")) refText = st;
            else if (role == QStringLiteral("val")) valText = st;
        }
    }

    if (!refText) {
        refText = new QGraphicsSimpleTextItem(item);
        refText->setData(kSymbolLabelRole, QStringLiteral("ref"));
    }
    if (!valText) {
        valText = new QGraphicsSimpleTextItem(item);
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

// ──────────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────────

static QJsonObject penToJson(const QPen &p)
{
    return {
        {"color",    p.color().name(QColor::HexArgb)},
        {"width",    p.widthF()},
        {"style",    static_cast<int>(p.style())},
        {"capStyle", static_cast<int>(p.capStyle())}
    };
}

static QPen penFromJson(const QJsonObject &o)
{
    QPen p;
    p.setColor   (QColor(o["color"].toString()));
    p.setWidthF  (o["width"].toDouble(1.5));
    p.setStyle   (static_cast<Qt::PenStyle>  (o["style"].toInt(Qt::SolidLine)));
    p.setCapStyle(static_cast<Qt::PenCapStyle>(o["capStyle"].toInt(Qt::RoundCap)));
    return p;
}

static QJsonObject brushToJson(const QBrush &b)
{
    return {
        {"color", b.color().name(QColor::HexArgb)},
        {"style", static_cast<int>(b.style())}
    };
}

static QBrush brushFromJson(const QJsonObject &o)
{
    if (o.isEmpty()) return Qt::NoBrush;
    QBrush b;
    b.setColor(QColor(o["color"].toString()));
    b.setStyle(static_cast<Qt::BrushStyle>(o["style"].toInt(Qt::SolidPattern)));
    return b;
}

static QString pixmapToBase64(const QPixmap &px)
{
    QByteArray ba;
    QBuffer    buf(&ba);
    buf.open(QIODevice::WriteOnly);
    px.save(&buf, "PNG");
    return QString::fromLatin1(ba.toBase64());
}

static QPixmap pixmapFromBase64(const QString &s)
{
    QByteArray ba = QByteArray::fromBase64(s.toLatin1());
    QPixmap    px;
    px.loadFromData(ba, "PNG");
    return px;
}

static void applyCommonFlags(QGraphicsItem *item)
{
    item->setFlag(QGraphicsItem::ItemIsSelectable);
    item->setFlag(QGraphicsItem::ItemIsMovable);
}

// ──────────────────────────────────────────────────
//  itemToJson  (used by save + clipboard)
// ──────────────────────────────────────────────────

QJsonObject FileManager::itemToJson(QGraphicsItem *item)
{
    QJsonObject obj;
        if (auto *eq = dynamic_cast<ResizableEquationItem *>(item)) {
            obj["type"] = "equation";
            obj["x"] = item->pos().x();
            obj["y"] = item->pos().y();
            obj["z"] = item->zValue();
            obj["scale"] = item->scale();
            if (eq->hasSvgData()) {
                obj["svg"] = QString::fromLatin1(eq->svgData().toBase64());
            } else {
                obj["pixmap"] = pixmapToBase64(eq->fallbackPixmap());
            }
            if (item->data(0).isValid())
                obj["latex"] = item->data(0).toString();
            return obj;
        }

    obj["x"] = item->pos().x();
    obj["y"] = item->pos().y();
    obj["z"] = item->zValue();

    // ── DiagramRectItem (must be before QGraphicsRectItem) ──
    if (auto *ri = dynamic_cast<DiagramRectItem *>(item)) {
        obj["type"]   = "rect";
        obj["rx"]     = ri->rect().x();
        obj["ry"]     = ri->rect().y();
        obj["rw"]     = ri->rect().width();
        obj["rh"]     = ri->rect().height();
        obj["radius"] = ri->cornerRadius();
        obj["pen"]    = penToJson(ri->pen());
        obj["brush"]  = brushToJson(ri->brush());
        return obj;
    }

    // ── ArrowItem (must be before QGraphicsLineItem) ──
    if (auto *ai = dynamic_cast<ArrowItem *>(item)) {
        obj["type"]        = "arrow";
        obj["x1"]          = ai->line().x1();
        obj["y1"]          = ai->line().y1();
        obj["x2"]          = ai->line().x2();
        obj["y2"]          = ai->line().y2();
        obj["pen"]         = penToJson(ai->pen());
        obj["startArrow"]  = static_cast<int>(ai->startArrow());
        obj["endArrow"]    = static_cast<int>(ai->endArrow());
        return obj;
    }

    switch (item->type()) {

    case QGraphicsRectItem::Type: {
        auto *ri  = static_cast<QGraphicsRectItem *>(item);
        obj["type"]  = "rect";
        obj["rx"]    = ri->rect().x();
        obj["ry"]    = ri->rect().y();
        obj["rw"]    = ri->rect().width();
        obj["rh"]    = ri->rect().height();
        obj["radius"]= 0.0;
        obj["pen"]   = penToJson(ri->pen());
        obj["brush"] = brushToJson(ri->brush());
        break;
    }

    case QGraphicsEllipseItem::Type: {
        auto *ei  = static_cast<QGraphicsEllipseItem *>(item);
        if (ei->data(1).toString() == QStringLiteral("junctionDot")) {
            obj["type"] = "junction_dot";
            obj["d"]    = ei->rect().width();
            obj["pen"]  = penToJson(ei->pen());
            obj["brush"] = brushToJson(ei->brush());
        } else {
            obj["type"]  = "ellipse";
            obj["rx"]    = ei->rect().x();
            obj["ry"]    = ei->rect().y();
            obj["rw"]    = ei->rect().width();
            obj["rh"]    = ei->rect().height();
            obj["pen"]   = penToJson(ei->pen());
            obj["brush"] = brushToJson(ei->brush());
        }
        break;
    }

    case QGraphicsLineItem::Type: {
        auto *li  = static_cast<QGraphicsLineItem *>(item);
        obj["type"] = "line";
        obj["x1"]   = li->line().x1();
        obj["y1"]   = li->line().y1();
        obj["x2"]   = li->line().x2();
        obj["y2"]   = li->line().y2();
        obj["pen"]  = penToJson(li->pen());
        break;
    }

    case QGraphicsTextItem::Type: {
        auto *ti       = static_cast<QGraphicsTextItem *>(item);
        QFont f        = ti->font();
        obj["type"]       = "text";
        obj["text"]       = ti->toPlainText();
        obj["textHtml"]   = ti->toHtml();
        if (ti->data(0).isValid())
            obj["latex"]  = ti->data(0).toString();
        obj["fontFamily"] = f.family();
        obj["fontSize"]   = f.pointSizeF();
        obj["bold"]       = f.bold();
        obj["italic"]     = f.italic();
        obj["underline"]  = f.underline();
        obj["color"]      = ti->defaultTextColor().name(QColor::HexArgb);
        break;
    }

    case QGraphicsPixmapItem::Type: {
        auto *pi  = static_cast<QGraphicsPixmapItem *>(item);
        obj["type"]   = "pixmap";
        obj["pixmap"] = pixmapToBase64(pi->pixmap());
        if (pi->data(0).isValid())
            obj["latex"] = pi->data(0).toString();

        if (pi->data(kSymbolComponentRole).toBool()) {
            obj["isComponent"] = true;
            obj["componentRef"] = pi->data(kSymbolRefRole).toString();
            obj["componentValue"] = pi->data(kSymbolValueRole).toString();
            QJsonArray ports;
            const QVariantList localPorts = pi->data(kSymbolPortsRole).toList();
            for (const QVariant &v : localPorts) {
                const QPointF p = v.toPointF();
                ports.append(QJsonObject{{"x", p.x()}, {"y", p.y()}});
            }
            obj["ports"] = ports;
        }
        break;
    }

    default:
        break;
    }

    return obj;
}

// ──────────────────────────────────────────────────
//  itemFromJson  (used by load + clipboard)
// ──────────────────────────────────────────────────

QGraphicsItem *FileManager::itemFromJson(const QJsonObject &o)
{
    const QString type = o["type"].toString();
    const QPointF pos  (o["x"].toDouble(), o["y"].toDouble());
    const qreal   z     = o["z"].toDouble();

    QGraphicsItem *item = nullptr;

    if (type == "rect") {
        QRectF r(o["rx"].toDouble(), o["ry"].toDouble(),
                 o["rw"].toDouble(), o["rh"].toDouble());
        auto *ri = new DiagramRectItem(r);
        ri->setPen  (penFromJson  (o["pen"].toObject()));
        ri->setBrush(brushFromJson(o["brush"].toObject()));
        ri->setCornerRadius(o["radius"].toDouble(0.0));
        item = ri;

    } else if (type == "equation") {
        ResizableEquationItem *eq = nullptr;
        if (o.contains("svg")) {
            const QByteArray svg = QByteArray::fromBase64(o["svg"].toString().toLatin1());
            if (!svg.isEmpty()) eq = new ResizableEquationItem(svg);
        }
        if (!eq) {
            QPixmap px;
            if (o.contains("pixmap")) px = pixmapFromBase64(o["pixmap"].toString());
            if (px.isNull()) return nullptr;
            eq = new ResizableEquationItem(px);
        }
        if (o.contains("latex")) eq->setData(0, o["latex"].toString());
        eq->setScale(o["scale"].toDouble(1.0));
        item = eq;

    } else if (type == "junction_dot") {
        const qreal d = o["d"].toDouble(8.0);
        auto *ei = new QGraphicsEllipseItem(QRectF(-d / 2.0, -d / 2.0, d, d));
        ei->setPen  (penFromJson  (o["pen"].toObject()));
        ei->setBrush(brushFromJson(o["brush"].toObject()));
        ei->setData(1, QStringLiteral("junctionDot"));
        item = ei;

    } else if (type == "ellipse") {
        QRectF r(o["rx"].toDouble(), o["ry"].toDouble(),
                 o["rw"].toDouble(), o["rh"].toDouble());
        auto *ei = new QGraphicsEllipseItem(r);
        ei->setPen  (penFromJson  (o["pen"].toObject()));
        ei->setBrush(brushFromJson(o["brush"].toObject()));
        item = ei;

    } else if (type == "line") {
        QLineF l(o["x1"].toDouble(), o["y1"].toDouble(),
                 o["x2"].toDouble(), o["y2"].toDouble());
        auto *li = new QGraphicsLineItem(l);
        li->setPen(penFromJson(o["pen"].toObject()));
        item = li;

    } else if (type == "arrow") {
        QLineF l(o["x1"].toDouble(), o["y1"].toDouble(),
                 o["x2"].toDouble(), o["y2"].toDouble());
        auto *ai = new ArrowItem(l);
        ai->setPen       (penFromJson(o["pen"].toObject()));
        ai->setStartArrow(static_cast<ArrowItem::ArrowType>(o["startArrow"].toInt(0)));
        ai->setEndArrow  (static_cast<ArrowItem::ArrowType>(o["endArrow"].toInt(1)));
        item = ai;

    } else if (type == "text") {
        auto *ti = new QGraphicsTextItem;
        const QString html = o["textHtml"].toString();
        if (!html.isEmpty()) ti->setHtml(html);
        else                 ti->setPlainText(o["text"].toString());
        QFont f(o["fontFamily"].toString("Segoe UI"));
        f.setPointSizeF(o["fontSize"].toDouble(11.0));
        f.setBold     (o["bold"].toBool(false));
        f.setItalic   (o["italic"].toBool(false));
        f.setUnderline(o["underline"].toBool(false));
        ti->setFont(f);
        ti->setDefaultTextColor(QColor(o["color"].toString("#ff000000")));
        ti->setTextInteractionFlags(Qt::NoTextInteraction);
        if (o.contains("latex")) ti->setData(0, o["latex"].toString());
        item = ti;

    } else if (type == "pixmap") {
        QPixmap px = pixmapFromBase64(o["pixmap"].toString());
        if (px.isNull()) return nullptr;
        auto *pi = new ResizablePixmapItem(px);
        if (o.contains("latex")) pi->setData(0, o["latex"].toString());

        if (o["isComponent"].toBool(false)) {
            QVariantList ports;
            const QJsonArray pa = o["ports"].toArray();
            for (const auto &pv : pa) {
                const QJsonObject po = pv.toObject();
                ports << QPointF(po["x"].toDouble(), po["y"].toDouble());
            }
            pi->setData(kSymbolComponentRole, true);
            pi->setData(kSymbolPortsRole, ports);
            pi->setData(kSymbolRefRole, o["componentRef"].toString("U?"));
            pi->setData(kSymbolValueRole, o["componentValue"].toString("COMP"));
            updateComponentLabels(pi);
        }
        item = pi;
    }

    if (item) {
        item->setPos   (pos);
        item->setZValue(z);
        applyCommonFlags(item);
    }

    return item;
}

// ──────────────────────────────────────────────────
//  Save
// ──────────────────────────────────────────────────

bool FileManager::save(QGraphicsScene *scene, const QString &filePath)
{
    QJsonArray arr;
    // Iterate in reverse so front items are last in file (load order = z-order)
    const auto allItems = scene->items(Qt::AscendingOrder);
    for (QGraphicsItem *item : allItems) {
        QJsonObject obj = itemToJson(item);
        if (!obj["type"].toString().isEmpty())
            arr.append(obj);
    }

    QJsonObject root;
    root["version"] = 2;
    root["items"]   = arr;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// ──────────────────────────────────────────────────
//  Load
// ──────────────────────────────────────────────────

bool FileManager::load(QGraphicsScene *scene, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError err;
    QJsonDocument   doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    scene->clear();

    const QJsonArray items = doc.object()["items"].toArray();
    for (const QJsonValue &val : items) {
        QGraphicsItem *item = itemFromJson(val.toObject());
        if (item) scene->addItem(item);
    }

    return true;
}