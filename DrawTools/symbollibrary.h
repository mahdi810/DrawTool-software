#ifndef SYMBOLLIBRARY_H
#define SYMBOLLIBRARY_H

#include <QDockWidget>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QString>

class QListWidget;
class QComboBox;
class QPushButton;

struct Symbol {
    QString name;
    QPixmap thumbnail;
    QList<QPointF> portsNormalized;
};

struct SymbolGroup {
    QString name;
    bool custom = false;
    QList<Symbol> symbols;
};

class SymbolLibrary : public QDockWidget
{
    Q_OBJECT

public:
    explicit SymbolLibrary(QWidget *parent = nullptr);
    void addSymbol(const QString &name, const QPixmap &thumbnail);
    void addSymbolToLibrary(const QString &libraryName,
                            const QString &name,
                            const QPixmap &thumbnail,
                            const QList<QPointF> &portsNormalized = {},
                            bool customLibrary = true);
    bool hasSymbolAt(int index) const;
    const Symbol &symbolAt(int index) const;

signals:
    void symbolActivated(int index);

private:
    int currentLibraryIndex() const;
    int findLibraryIndex(const QString &name) const;
    int firstCustomLibraryIndex() const;

    void addLibraryInternal(const QString &name, bool custom);
    void rebuildLibraryCombo();
    void refreshSymbolList();
    void updateButtons();

    void loadDefaultLibraries();
    void loadCustomLibraries();
    void saveCustomLibraries() const;

    QString promptForCustomLibrary();

    QPixmap makeAndGateSymbol() const;
    QPixmap makeOrGateSymbol() const;
    QPixmap makeNotGateSymbol() const;
    QPixmap makeNandGateSymbol() const;
    QPixmap makeNorGateSymbol() const;
    QPixmap makeXorGateSymbol() const;

    QPixmap makeInputPinSymbol() const;
    QPixmap makeOutputPinSymbol() const;
    QPixmap makeClockSymbol() const;
    QPixmap makeGroundSymbol() const;
    QPixmap makeVccSymbol() const;

    QComboBox   *m_libraryCombo;
    QPushButton *m_addLibraryBtn;
    QPushButton *m_removeLibraryBtn;
    QPushButton *m_removeSymbolBtn;
    QListWidget  *m_listWidget;
    QList<SymbolGroup> m_groups;
};

#endif // SYMBOLLIBRARY_H