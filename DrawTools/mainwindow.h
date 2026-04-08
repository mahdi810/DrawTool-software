#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "diagramscene.h"

class QMenu;
class QAction;
class QActionGroup;
class QSpinBox;
class DiagramView;
class SymbolLibrary;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void saveSelectionAsSymbol();
    void onSymbolActivated(int index);
    void onGridSizeChanged(int value);
    void deleteSelectedItems();

    // ← FILE SLOTS
    void newDiagram();
    void openDiagram();
    void saveDiagram();
    void saveDiagramAs();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createDocks();
    void setTool(DiagramScene::Mode mode);

    DiagramScene  *scene;
    DiagramView   *view;
    SymbolLibrary *symbolLibrary;

    QString m_currentFile;   // ← tracks open file path

    QMenu *fileMenu, *editMenu, *viewMenu, *helpMenu;

    QAction *newAction, *openAction, *saveAction, *saveAsAction, *exitAction;
    QAction *undoAction, *redoAction, *deleteAction, *saveSymbolAction;
    QAction *zoomInAction, *zoomOutAction, *resetZoomAction, *toggleGridAction;
    QAction *selectAction, *insertRectAction, *insertEllipseAction;
    QAction *insertLineAction, *insertTextAction;
    QAction *aboutAction;

    QActionGroup *toolGroup;
    QSpinBox     *gridSpinBox;
};

#endif // MAINWINDOW_H