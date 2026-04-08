#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QString>
#include <QJsonObject>
#include <QGraphicsScene>
#include <QGraphicsItem>

class FileManager
{
public:
    static bool save(QGraphicsScene *scene, const QString &filePath);
    static bool load(QGraphicsScene *scene, const QString &filePath);

    // Used by clipboard and file I/O
    static QJsonObject   itemToJson  (QGraphicsItem *item);
    static QGraphicsItem *itemFromJson(const QJsonObject &obj);
};

#endif // FILEMANAGER_H