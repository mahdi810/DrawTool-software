#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QString>
#include <QGraphicsScene>

class FileManager
{
public:
    // Returns true on success
    static bool save(QGraphicsScene *scene, const QString &filePath);
    static bool load(QGraphicsScene *scene, const QString &filePath);
};

#endif // FILEMANAGER_H