#include "mainwindow.h"
#include "diagramview.h"
#include "symbollibrary.h"
#include "commands.h"
#include "filemanager.h"

#include <QAction>
#include <QActionGroup>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QSpinBox>
#include <QLabel>
#include <QPainter>
#include <QGraphicsItem>
#include <QFileDialog>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    scene = new DiagramScene(this);
    view  = new DiagramView(this);
    view->setScene(scene);
    setCentralWidget(view);

    createActions();
    createMenus();
    createToolBars();
    createDocks();

    connect(scene, &DiagramScene::itemInserted, this, [this]() {
        setTool(DiagramScene::SelectMode);
        selectAction->setChecked(true);
    });

    setWindowTitle("DrawTools");
    resize(1200, 800);
    statusBar()->showMessage("Ready  |  Ctrl+Wheel: Zoom  |  Middle Mouse: Pan");
}

MainWindow::~MainWindow() {}

void MainWindow::setTool(DiagramScene::Mode mode)
{
    scene->setMode(mode);
    view->setDragMode(mode == DiagramScene::SelectMode
                          ? QGraphicsView::RubberBandDrag
                          : QGraphicsView::NoDrag);
}

void MainWindow::createActions()
{
    // ── File ──────────────────────────────────────
    newAction    = new QAction("&New",        this); newAction->setShortcut(QKeySequence::New);
    openAction   = new QAction("&Open...",    this); openAction->setShortcut(QKeySequence::Open);
    saveAction   = new QAction("&Save",       this); saveAction->setShortcut(QKeySequence::Save);
    saveAsAction = new QAction("Save &As...", this);
    exitAction   = new QAction("E&xit",       this); exitAction->setShortcut(QKeySequence::Quit);

    // ← THE 4 MISSING CONNECTS
    connect(newAction,    &QAction::triggered, this, &MainWindow::newDiagram);
    connect(openAction,   &QAction::triggered, this, &MainWindow::openDiagram);
    connect(saveAction,   &QAction::triggered, this, &MainWindow::saveDiagram);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveDiagramAs);
    connect(exitAction,   &QAction::triggered, this, &QWidget::close);

    // ── Edit ──────────────────────────────────────
    undoAction = new QAction("&Undo", this);
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(false);

    redoAction = new QAction("&Redo", this);
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(false);

    connect(undoAction, &QAction::triggered, scene->undoStack(), &QUndoStack::undo);
    connect(redoAction, &QAction::triggered, scene->undoStack(), &QUndoStack::redo);

    connect(scene->undoStack(), &QUndoStack::canUndoChanged, undoAction, &QAction::setEnabled);
    connect(scene->undoStack(), &QUndoStack::canRedoChanged, redoAction, &QAction::setEnabled);

    deleteAction = new QAction("&Delete", this);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedItems);

    saveSymbolAction = new QAction("&Save Selection as Symbol", this);
    connect(saveSymbolAction, &QAction::triggered, this, &MainWindow::saveSelectionAsSymbol);

    // ── View ──────────────────────────────────────
    zoomInAction    = new QAction("Zoom &In",    this);
    zoomInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    zoomOutAction   = new QAction("Zoom &Out",   this);
    zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    resetZoomAction = new QAction("&Reset Zoom", this);
    resetZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    connect(zoomInAction,    &QAction::triggered, this, [this]() { view->scale(1.2, 1.2); });
    connect(zoomOutAction,   &QAction::triggered, this, [this]() { view->scale(1.0/1.2, 1.0/1.2); });
    connect(resetZoomAction, &QAction::triggered, this, [this]() { view->resetTransform(); });

    toggleGridAction = new QAction("Show &Grid", this);
    toggleGridAction->setCheckable(true);
    toggleGridAction->setChecked(true);
    connect(toggleGridAction, &QAction::toggled, scene, &DiagramScene::setGridVisible);

    // ── Tools ─────────────────────────────────────
    toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);

    selectAction = new QAction("Select", this);
    selectAction->setCheckable(true);
    selectAction->setChecked(true);
    toolGroup->addAction(selectAction);
    connect(selectAction, &QAction::triggered, this,
            [this]() { setTool(DiagramScene::SelectMode); });

    insertRectAction = new QAction("Rectangle", this);
    insertRectAction->setCheckable(true);
    toolGroup->addAction(insertRectAction);
    connect(insertRectAction, &QAction::triggered, this,
            [this]() { setTool(DiagramScene::InsertRectMode); });

    insertEllipseAction = new QAction("Ellipse", this);
    insertEllipseAction->setCheckable(true);
    toolGroup->addAction(insertEllipseAction);
    connect(insertEllipseAction, &QAction::triggered, this,
            [this]() { setTool(DiagramScene::InsertEllipseMode); });

    insertLineAction = new QAction("Line", this);
    insertLineAction->setCheckable(true);
    toolGroup->addAction(insertLineAction);
    connect(insertLineAction, &QAction::triggered, this,
            [this]() { setTool(DiagramScene::InsertLineMode); });

    insertTextAction = new QAction("Text", this);
    insertTextAction->setCheckable(true);
    toolGroup->addAction(insertTextAction);
    connect(insertTextAction, &QAction::triggered, this,
            [this]() { setTool(DiagramScene::InsertTextMode); });

    // ── Help ──────────────────────────────────────
    aboutAction = new QAction("&About", this);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "DrawTools",
                           "DrawTools\nEngineering Diagram Editor\n\n"
                           "Ctrl+Z: Undo\nCtrl+Y: Redo\n"
                           "Ctrl+Wheel: Zoom\nMiddle Mouse: Pan");
    });
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(deleteAction);
    editMenu->addSeparator();
    editMenu->addAction(saveSymbolAction);

    viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(resetZoomAction);
    viewMenu->addSeparator();
    viewMenu->addAction(toggleGridAction);

    helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(aboutAction);
}

void MainWindow::createToolBars()
{
    QToolBar *drawBar = addToolBar("Drawing Tools");
    drawBar->setMovable(false);
    drawBar->addWidget(new QLabel(" Tool: "));
    drawBar->addAction(selectAction);
    drawBar->addSeparator();
    drawBar->addAction(insertRectAction);
    drawBar->addAction(insertEllipseAction);
    drawBar->addAction(insertLineAction);
    drawBar->addAction(insertTextAction);
    drawBar->addSeparator();
    drawBar->addWidget(new QLabel(" Grid: "));

    gridSpinBox = new QSpinBox(this);
    gridSpinBox->setRange(5, 100);
    gridSpinBox->setValue(20);
    gridSpinBox->setSuffix(" px");
    gridSpinBox->setFixedWidth(75);
    connect(gridSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onGridSizeChanged);
    drawBar->addWidget(gridSpinBox);
}

void MainWindow::createDocks()
{
    symbolLibrary = new SymbolLibrary(this);
    addDockWidget(Qt::RightDockWidgetArea, symbolLibrary);
    connect(symbolLibrary, &SymbolLibrary::symbolActivated,
            this, &MainWindow::onSymbolActivated);
}

void MainWindow::onGridSizeChanged(int value)
{
    scene->setGridSize(value);
    statusBar()->showMessage(QString("Grid size: %1 px").arg(value));
}

void MainWindow::deleteSelectedItems()
{
    const auto items = scene->selectedItems();
    if (items.isEmpty()) return;

    scene->undoStack()->beginMacro("Delete Items");
    for (auto *item : items)
        scene->undoStack()->push(new RemoveItemCommand(scene, item));
    scene->undoStack()->endMacro();

    statusBar()->showMessage(QString("%1 item(s) deleted").arg(items.size()));
}

void MainWindow::saveSelectionAsSymbol()
{
    const auto selected = scene->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Select one or more items first.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "Save Symbol",
                                         "Symbol name:", QLineEdit::Normal, "MySymbol", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QRectF bounds;
    for (auto *item : selected)
        bounds = bounds.united(item->mapToScene(item->boundingRect()).boundingRect());
    bounds = bounds.adjusted(-4, -4, 4, 4);

    QPixmap pixmap(bounds.size().toSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    scene->render(&painter, QRectF(), bounds);
    painter.end();

    symbolLibrary->addSymbol(name, pixmap);
    statusBar()->showMessage("Symbol '" + name + "' saved to library.");
}

void MainWindow::onSymbolActivated(int index)
{
    scene->setCurrentSymbol(symbolLibrary->symbolAt(index).thumbnail);
    setTool(DiagramScene::InsertSymbolMode);
    statusBar()->showMessage("Click on canvas to place symbol.");
}

// ── File operations ───────────────────────────────

void MainWindow::newDiagram()
{
    if (!scene->items().isEmpty()) {
        auto btn = QMessageBox::question(this, "New Diagram",
                                         "Discard current diagram and start fresh?",
                                         QMessageBox::Yes | QMessageBox::No);
        if (btn != QMessageBox::Yes) return;
    }
    scene->clear();
    scene->undoStack()->clear();
    m_currentFile.clear();
    setWindowTitle("DrawTools — New Diagram");
    statusBar()->showMessage("New diagram created.");
}

void MainWindow::openDiagram()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open Diagram", QString(),
        "DrawTools Files (*.dtj);;JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    if (!FileManager::load(scene, path)) {
        QMessageBox::critical(this, "Open Failed",
                              "Could not read file:\n" + path);
        return;
    }

    scene->undoStack()->clear();
    m_currentFile = path;
    setWindowTitle("DrawTools — " + QFileInfo(path).fileName());
    statusBar()->showMessage("Opened: " + path);
}

void MainWindow::saveDiagram()
{
    if (m_currentFile.isEmpty()) {
        saveDiagramAs();
        return;
    }

    if (!FileManager::save(scene, m_currentFile)) {
        QMessageBox::critical(this, "Save Failed",
                              "Could not write file:\n" + m_currentFile);
        return;
    }
    statusBar()->showMessage("Saved: " + m_currentFile);
}

void MainWindow::saveDiagramAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save Diagram As", QString(),
        "DrawTools Files (*.dtj);;JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    if (!path.endsWith(".dtj") && !path.endsWith(".json"))
        path += ".dtj";

    if (!FileManager::save(scene, path)) {
        QMessageBox::critical(this, "Save Failed",
                              "Could not write file:\n" + path);
        return;
    }

    m_currentFile = path;
    setWindowTitle("DrawTools — " + QFileInfo(path).fileName());
    statusBar()->showMessage("Saved: " + path);
}