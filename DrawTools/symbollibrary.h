#ifndef SYMBOLLIBRARY_H
#define SYMBOLLIBRARY_H

#include <QDockWidget>
#include <QList>
#include <QPixmap>
#include <QString>

class QListWidget;

struct Symbol {
    QString name;
    QPixmap thumbnail;
};

class SymbolLibrary : public QDockWidget
{
    Q_OBJECT

public:
    explicit SymbolLibrary(QWidget *parent = nullptr);
    void addSymbol(const QString &name, const QPixmap &thumbnail);
    const Symbol &symbolAt(int index) const { return m_symbols[index]; }

signals:
    void symbolActivated(int index);

private:
    QListWidget  *m_listWidget;
    QList<Symbol> m_symbols;
};

#endif // SYMBOLLIBRARY_H