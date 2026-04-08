#include "filemanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QByteArray>
#include <QBuffer>
#include <QPixmap>
#include <QFont>
#include <QColor>

// ── Helpers ───────────────────────────────────────

static QJsonObject penToJson(const QPen &pen)
{
    return {
        {"color",  pen.color().name(QColor::HexArgb)},
        {"width",  pen.widthF()},
        {"style",  static_cast<int>(pen.style())},
        {"capStyle", static_cast<int>(pen.capStyle())}
    };
}

static QPen penFromJson(const QJsonObject &o)
{
    QPen pen;
    pen.setColor(QColor(o["color"].toString()));
    pen.setWidthF(o["width"].toDouble());
    pen.setStyle(static_cast<Qt::PenStyle>(o["style"].toInt()));
    pen.setCapStyle(static_cast<Qt::PenCapStyle>(o["capStyle"].toInt()));
    return pen;
}

static QJsonObject brushToJson(const QBrush &brush)
{
    return {{"color", brush.color().name(QColor::HexArgb)}};
}

static QBrush brushFromJson(const QJsonObject &o)
{
    return QBrush(QColor(o["color"].toString()));
}

static QString pixmapToBase64(const QPixmap &px)
{
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    px.save(&buf, "PNG");
    return QString::fromLatin1(ba.toBase64());
}

static QPixmap pixmapFromBase64(const QString &s)
{
    QByteArray ba = QByteArray::fromBase64(s.toLatin1());
    QPixmap px;
    px.loadFromData(ba, "PNG");
    return px;
}

// ── Save ──────────────────────────────────────────

bool FileManager::save(QGraphicsScene *scene, const QString &filePath)
{
    QJsonArray items;

    for (QGraphicsItem *item : scene->items()) {
        QJsonObject obj;
        obj["x"] = item->pos().x();
        obj["y"] = item->pos().y();
        obj["z"] = item->zValue();

        switch (item->type()) {

        case QGraphicsRectItem::Type: {
            auto *ri = static_cast<QGraphicsRectItem *>(item);
            obj["type"]  = "rect";
            obj["rx"]    = ri->rect().x();
            obj["ry"]    = ri->rect().y();
            obj["rw"]    = ri->rect().width();
            obj["rh"]    = ri->rect().height();
            obj["pen"]   = penToJson(ri->pen());
            obj["brush"] = brushToJson(ri->brush());
            break;
        }

        case QGraphicsEllipseItem::Type: {
            auto *ei = static_cast<QGraphicsEllipseItem *>(item);
            obj["type"]  = "ellipse";
            obj["rx"]    = ei->rect().x();
            obj["ry"]    = ei->rect().y();
            obj["rw"]    = ei->rect().width();
            obj["rh"]    = ei->rect().height();
            obj["pen"]   = penToJson(ei->pen());
            obj["brush"] = brushToJson(ei->brush());
            break;
        }

        case QGraphicsLineItem::Type: {
            auto *li = static_cast<QGraphicsLineItem *>(item);
            obj["type"] = "line";
            obj["x1"]   = li->line().x1();
            obj["y1"]   = li->line().y1();
            obj["x2"]   = li->line().x2();
            obj["y2"]   = li->line().y2();
            obj["pen"]  = penToJson(li->pen());
            break;
        }

        case QGraphicsTextItem::Type: {
            auto *ti = static_cast<QGraphicsTextItem *>(item);
            obj["type"]      = "text";
            obj["text"]      = ti->toPlainText();
            obj["fontFamily"]= ti->font().family();
            obj["fontSize"]  = ti->font().pointSizeF();
            obj["color"]     = ti->defaultTextColor().name(QColor::HexArgb);
            break;
        }

        case QGraphicsPixmapItem::Type: {
            auto *pi = static_cast<QGraphicsPixmapItem *>(item);
            obj["type"]   = "pixmap";
            obj["pixmap"] = pixmapToBase64(pi->pixmap());
            break;
        }

        default:
            continue;   // skip unknown item types
        }

        items.append(obj);
    }

    QJsonObject root;
    root["version"] = 1;
    root["items"]   = items;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// ── Load ──────────────────────────────────────────

bool FileManager::load(QGraphicsScene *scene, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    // Clear scene (keep undo history clean)
    scene->clear();

    const QJsonArray items = doc.object()["items"].toArray();
    for (const QJsonValue &val : items) {
        QJsonObject o = val.toObject();
        QString type = o["type"].toString();
        QPointF pos(o["x"].toDouble(), o["y"].toDouble());

        if (type == "rect") {
            QRectF r(o["rx"].toDouble(), o["ry"].toDouble(),
                     o["rw"].toDouble(), o["rh"].toDouble());
            auto *item = new QGraphicsRectItem(r);
            item->setPen(penFromJson(o["pen"].toObject()));
            item->setBrush(brushFromJson(o["brush"].toObject()));
            item->setPos(pos);
            item->setZValue(o["z"].toDouble());
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setFlag(QGraphicsItem::ItemIsMovable);
            scene->addItem(item);

        } else if (type == "ellipse") {
            QRectF r(o["rx"].toDouble(), o["ry"].toDouble(),
                     o["rw"].toDouble(), o["rh"].toDouble());
            auto *item = new QGraphicsEllipseItem(r);
            item->setPen(penFromJson(o["pen"].toObject()));
            item->setBrush(brushFromJson(o["brush"].toObject()));
            item->setPos(pos);
            item->setZValue(o["z"].toDouble());
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setFlag(QGraphicsItem::ItemIsMovable);
            scene->addItem(item);

        } else if (type == "line") {
            QLineF l(o["x1"].toDouble(), o["y1"].toDouble(),
                     o["x2"].toDouble(), o["y2"].toDouble());
            auto *item = new QGraphicsLineItem(l);
            item->setPen(penFromJson(o["pen"].toObject()));
            item->setPos(pos);
            item->setZValue(o["z"].toDouble());
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setFlag(QGraphicsItem::ItemIsMovable);
            scene->addItem(item);

        } else if (type == "text") {
            auto *item = new QGraphicsTextItem(o["text"].toString());
            QFont f(o["fontFamily"].toString());
            f.setPointSizeF(o["fontSize"].toDouble());
            item->setFont(f);
            item->setDefaultTextColor(QColor(o["color"].toString()));
            item->setPos(pos);
            item->setZValue(o["z"].toDouble());
            item->setTextInteractionFlags(Qt::TextEditorInteraction);
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setFlag(QGraphicsItem::ItemIsMovable);
            scene->addItem(item);

        } else if (type == "pixmap") {
            QPixmap px = pixmapFromBase64(o["pixmap"].toString());
            if (!px.isNull()) {
                auto *item = new QGraphicsPixmapItem(px);
                item->setPos(pos);
                item->setZValue(o["z"].toDouble());
                item->setFlag(QGraphicsItem::ItemIsSelectable);
                item->setFlag(QGraphicsItem::ItemIsMovable);
                scene->addItem(item);
            }
        }
    }

    return true;
}