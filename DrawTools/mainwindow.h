#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "diagramscene.h"

class QMenu;
class QAction;
class QActionGroup;
class QSpinBox;
class QLabel;
class DiagramView;
class SymbolLibrary;
class PropertiesPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // File
    void newDiagram();
    void openDiagram();
    void saveDiagram();
    void saveDiagramAs();
    void exportPng();
    void exportSvg();

    // Edit
    void deleteSelectedItems();
    void copyItems();
    void cutItems();
    void pasteItems();
    void duplicateItems();
    void selectAll();
    void insertEquation();

    // Arrange
    void bringToFront();
    void sendToBack();
    void bringForward();
    void sendBackward();
    void alignLeft();
    void alignRight();
    void alignTop();
    void alignBottom();
    void alignHCenter();
    void alignVCenter();
    void distributeH();
    void distributeV();

    // View / tools
    void saveSelectionAsSymbol();
    void onSymbolActivated(int index);
    void onGridSizeChanged(int value);
    void updateStatusBar();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createDocks();
    void setTool(DiagramScene::Mode mode);
    bool maybeSave();

    DiagramScene    *scene;
    DiagramView     *view;
    SymbolLibrary   *symbolLibrary;
    PropertiesPanel *propertiesPanel;

    QString m_currentFile;

    // Status bar labels
    QLabel *m_coordLabel;
    QLabel *m_zoomLabel;

    // Menus
    QMenu *fileMenu, *editMenu, *viewMenu, *arrangeMenu, *helpMenu;

    // File
    QAction *newAction, *openAction, *saveAction, *saveAsAction;
    QAction *exportPngAction, *exportSvgAction, *exitAction;

    // Edit
    QAction *undoAction, *redoAction;
    QAction *cutAction, *copyAction, *pasteAction, *duplicateAction;
    QAction *deleteAction, *selectAllAction;
    QAction *saveSymbolAction;
    QAction *insertEquationAction;

    // Arrange
    QAction *bringToFrontAction, *sendToBackAction;
    QAction *bringForwardAction, *sendBackwardAction;
    QAction *alignLeftAction, *alignRightAction;
    QAction *alignTopAction,  *alignBottomAction;
    QAction *alignHCenterAction, *alignVCenterAction;
    QAction *distributeHAction,  *distributeVAction;

    // View
    QAction *zoomInAction, *zoomOutAction, *resetZoomAction, *toggleGridAction;

    // Tools
    QAction *selectAction, *insertRectAction, *insertEllipseAction;
    QAction *insertLineAction, *insertArrowAction, *insertTextAction;
    QAction *aboutAction;

    QActionGroup *toolGroup;
    QSpinBox     *gridSpinBox;
};

#endif // MAINWINDOW_H