#include "mainwindow.h"
#include "diagramview.h"
#include "symbollibrary.h"
#include "propertiespanel.h"
#include "commands.h"
#include "filemanager.h"

#include <QAction>
#include <QActionGroup>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPainter>
#include <QGraphicsItem>
#include <QFileDialog>
#include <QFileInfo>
#include <QCloseEvent>
#include <QSvgGenerator>
#include <QGraphicsTextItem>
#include <QRegularExpression>
#include <QTextDocument>
#include <algorithm>

static QFont standardEquationFont()
{
    QFont chosen("Times New Roman");

    chosen.setStyleStrategy(QFont::PreferAntialias);
    chosen.setPointSize(14);
    chosen.setItalic(true);
    return chosen;
}

static QString latexToHtml(QString latex)
{
    latex.replace("\r\n", "\n");
    QString out = latex.toHtmlEscaped();
    out.replace("\n", "<br/>");

    const std::pair<const char *, const char *> commandMap[] = {
        {"\\alpha", "&alpha;"}, {"\\beta", "&beta;"}, {"\\gamma", "&gamma;"},
        {"\\delta", "&delta;"}, {"\\epsilon", "&epsilon;"}, {"\\theta", "&theta;"},
        {"\\lambda", "&lambda;"}, {"\\mu", "&mu;"}, {"\\pi", "&pi;"},
        {"\\sigma", "&sigma;"}, {"\\phi", "&phi;"}, {"\\omega", "&omega;"},
        {"\\Gamma", "&Gamma;"}, {"\\Delta", "&Delta;"}, {"\\Theta", "&Theta;"},
        {"\\Lambda", "&Lambda;"}, {"\\Pi", "&Pi;"}, {"\\Sigma", "&Sigma;"},
        {"\\Phi", "&Phi;"}, {"\\Omega", "&Omega;"},
        {"\\times", "&times;"}, {"\\cdot", "&middot;"}, {"\\pm", "&plusmn;"},
        {"\\neq", "&ne;"}, {"\\leq", "&le;"}, {"\\geq", "&ge;"},
        {"\\approx", "&asymp;"}, {"\\infty", "&infin;"}, {"\\rightarrow", "&rarr;"},
        {"\\leftarrow", "&larr;"}, {"\\leftrightarrow", "&harr;"},
        {"\\sum", "&sum;"}, {"\\prod", "&prod;"}, {"\\int", "&int;"},
        {"\\partial", "&part;"}, {"\\nabla", "&nabla;"}
    };
    for (const auto &entry : commandMap)
        out.replace(QString::fromLatin1(entry.first), QString::fromLatin1(entry.second));

    // Common LaTeX spacing and delimiter helpers.
    out.replace("\\left", "");
    out.replace("\\right", "");
    out.replace("\\,", "&thinsp;");
    out.replace("\\:", "&thinsp;");
    out.replace("\\;", "&nbsp;");
    out.replace("\\quad", "&nbsp;&nbsp;");
    out.replace("\\qquad", "&nbsp;&nbsp;&nbsp;&nbsp;");

    const QRegularExpression textRe(R"(\\text\{([^{}]+)\})");
    while (true) {
        const auto m = textRe.match(out);
        if (!m.hasMatch()) break;
        const QString repl = QString("<span style=\"font-style:normal;\">%1</span>").arg(m.captured(1));
        out.replace(m.capturedStart(), m.capturedLength(), repl);
    }

    const QRegularExpression fracRe(R"(\\frac\{([^{}]+)\}\{([^{}]+)\})");
    while (true) {
        const auto m = fracRe.match(out);
        if (!m.hasMatch()) break;
        const QString repl = QString("<span style=\"white-space:nowrap;\"><sup>%1</sup>&#8260;<sub>%2</sub></span>")
                                 .arg(m.captured(1), m.captured(2));
        out.replace(m.capturedStart(), m.capturedLength(), repl);
    }

    const QRegularExpression sqrtRe(R"(\\sqrt\{([^{}]+)\})");
    while (true) {
        const auto m = sqrtRe.match(out);
        if (!m.hasMatch()) break;
        const QString repl = QString("&#8730;(%1)").arg(m.captured(1));
        out.replace(m.capturedStart(), m.capturedLength(), repl);
    }

    const QRegularExpression supRe(R"(\^\{([^{}]+)\}|\^(&[A-Za-z0-9#]+;|[A-Za-z0-9+\-*/=()]))");
    while (true) {
        const auto m = supRe.match(out);
        if (!m.hasMatch()) break;
        const QString cap = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        out.replace(m.capturedStart(), m.capturedLength(), QString("<sup>%1</sup>").arg(cap));
    }

    const QRegularExpression subRe(R"(_\{([^{}]+)\}|_(&[A-Za-z0-9#]+;|[A-Za-z0-9+\-*/=()]))");
    while (true) {
        const auto m = subRe.match(out);
        if (!m.hasMatch()) break;
        const QString cap = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        out.replace(m.capturedStart(), m.capturedLength(), QString("<sub>%1</sub>").arg(cap));
    }

    out.replace("\\{", "&#123;");
    out.replace("\\}", "&#125;");
    return out;
}

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

    m_coordLabel = new QLabel("  x: 0  y: 0  ");
    m_zoomLabel  = new QLabel("  100%  ");
    statusBar()->addPermanentWidget(m_coordLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->showMessage("Ready  |  Ctrl+Wheel: Zoom  |  Middle Mouse: Pan");

    connect(scene, &DiagramScene::itemInserted, this, [this]() {
        setTool(DiagramScene::SelectMode);
        selectAction->setChecked(true);
    });
    connect(view, &DiagramView::mouseMoved, this, [this](const QPointF &p) {
        m_coordLabel->setText(QString("  x: %1  y: %2  ")
                                  .arg(qRound(p.x())).arg(qRound(p.y())));
    });
    connect(view, &DiagramView::zoomChanged, this, [this](qreal f) {
        m_zoomLabel->setText(QString("  %1%  ").arg(qRound(f * 100)));
    });

    setWindowTitle("DrawTools");
    resize(1280, 820);
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) event->accept();
    else             event->ignore();
}

bool MainWindow::maybeSave()
{
    if (scene->undoStack()->isClean()) return true;
    auto btn = QMessageBox::question(this, "DrawTools",
                                     "Unsaved changes. Save before closing?",
                                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (btn == QMessageBox::Save)    { saveDiagram(); return scene->undoStack()->isClean(); }
    if (btn == QMessageBox::Discard) return true;
    return false;
}

void MainWindow::setTool(DiagramScene::Mode mode)
{
    scene->setMode(mode);
    view->setDragMode(mode == DiagramScene::SelectMode
                          ? QGraphicsView::RubberBandDrag
                          : QGraphicsView::NoDrag);
    propertiesPanel->onModeChanged(mode);
}

void MainWindow::createActions()
{
    newAction       = new QAction("&New",           this); newAction->setShortcut(QKeySequence::New);
    openAction      = new QAction("&Open...",       this); openAction->setShortcut(QKeySequence::Open);
    saveAction      = new QAction("&Save",          this); saveAction->setShortcut(QKeySequence::Save);
    saveAsAction    = new QAction("Save &As...",    this); saveAsAction->setShortcut(QKeySequence::SaveAs);
    exportPngAction = new QAction("Export &PNG...", this);
    exportSvgAction = new QAction("Export &SVG...", this);
    exitAction      = new QAction("E&xit",          this); exitAction->setShortcut(QKeySequence::Quit);

    connect(newAction,       &QAction::triggered, this, &MainWindow::newDiagram);
    connect(openAction,      &QAction::triggered, this, &MainWindow::openDiagram);
    connect(saveAction,      &QAction::triggered, this, &MainWindow::saveDiagram);
    connect(saveAsAction,    &QAction::triggered, this, &MainWindow::saveDiagramAs);
    connect(exportPngAction, &QAction::triggered, this, &MainWindow::exportPng);
    connect(exportSvgAction, &QAction::triggered, this, &MainWindow::exportSvg);
    connect(exitAction,      &QAction::triggered, this, &QWidget::close);

    undoAction      = new QAction("&Undo",       this); undoAction->setShortcut(QKeySequence::Undo); undoAction->setEnabled(false);
    redoAction      = new QAction("&Redo",       this); redoAction->setShortcut(QKeySequence::Redo); redoAction->setEnabled(false);
    cutAction       = new QAction("Cu&t",        this); cutAction->setShortcut(QKeySequence::Cut);
    copyAction      = new QAction("&Copy",       this); copyAction->setShortcut(QKeySequence::Copy);
    pasteAction     = new QAction("&Paste",      this); pasteAction->setShortcut(QKeySequence::Paste);
    duplicateAction = new QAction("&Duplicate",  this); duplicateAction->setShortcut(Qt::CTRL | Qt::Key_D);
    deleteAction    = new QAction("&Delete",     this); deleteAction->setShortcut(QKeySequence::Delete);
    selectAllAction = new QAction("Select &All", this); selectAllAction->setShortcut(QKeySequence::SelectAll);
    saveSymbolAction= new QAction("Save Selection as &Symbol", this);
    insertEquationAction = new QAction("Insert &Equation (LaTeX)...", this);
    insertEquationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));

    connect(undoAction,        &QAction::triggered, scene->undoStack(), &QUndoStack::undo);
    connect(redoAction,        &QAction::triggered, scene->undoStack(), &QUndoStack::redo);
    connect(scene->undoStack(),&QUndoStack::canUndoChanged, undoAction, &QAction::setEnabled);
    connect(scene->undoStack(),&QUndoStack::canRedoChanged, redoAction, &QAction::setEnabled);
    connect(cutAction,         &QAction::triggered, this, &MainWindow::cutItems);
    connect(copyAction,        &QAction::triggered, this, &MainWindow::copyItems);
    connect(pasteAction,       &QAction::triggered, this, &MainWindow::pasteItems);
    connect(duplicateAction,   &QAction::triggered, this, &MainWindow::duplicateItems);
    connect(deleteAction,      &QAction::triggered, this, &MainWindow::deleteSelectedItems);
    connect(selectAllAction,   &QAction::triggered, this, &MainWindow::selectAll);
    connect(saveSymbolAction,  &QAction::triggered, this, &MainWindow::saveSelectionAsSymbol);
    connect(insertEquationAction, &QAction::triggered, this, &MainWindow::insertEquation);

    bringToFrontAction = new QAction("Bring to &Front",    this);
    sendToBackAction   = new QAction("Send to &Back",      this);
    bringForwardAction = new QAction("Bring &Forward",     this);
    sendBackwardAction = new QAction("Send Back&ward",     this);
    alignLeftAction    = new QAction("Align &Left",        this);
    alignRightAction   = new QAction("Align &Right",       this);
    alignTopAction     = new QAction("Align &Top",         this);
    alignBottomAction  = new QAction("Align &Bottom",      this);
    alignHCenterAction = new QAction("Center &Horizontal", this);
    alignVCenterAction = new QAction("Center &Vertical",   this);
    distributeHAction  = new QAction("Distribute &H",      this);
    distributeVAction  = new QAction("Distribute &V",      this);

    connect(bringToFrontAction, &QAction::triggered, this, &MainWindow::bringToFront);
    connect(sendToBackAction,   &QAction::triggered, this, &MainWindow::sendToBack);
    connect(bringForwardAction, &QAction::triggered, this, &MainWindow::bringForward);
    connect(sendBackwardAction, &QAction::triggered, this, &MainWindow::sendBackward);
    connect(alignLeftAction,    &QAction::triggered, this, &MainWindow::alignLeft);
    connect(alignRightAction,   &QAction::triggered, this, &MainWindow::alignRight);
    connect(alignTopAction,     &QAction::triggered, this, &MainWindow::alignTop);
    connect(alignBottomAction,  &QAction::triggered, this, &MainWindow::alignBottom);
    connect(alignHCenterAction, &QAction::triggered, this, &MainWindow::alignHCenter);
    connect(alignVCenterAction, &QAction::triggered, this, &MainWindow::alignVCenter);
    connect(distributeHAction,  &QAction::triggered, this, &MainWindow::distributeH);
    connect(distributeVAction,  &QAction::triggered, this, &MainWindow::distributeV);

    zoomInAction    = new QAction("Zoom &In",    this); zoomInAction->setShortcut(Qt::CTRL | Qt::Key_Plus);
    zoomOutAction   = new QAction("Zoom &Out",   this); zoomOutAction->setShortcut(Qt::CTRL | Qt::Key_Minus);
    resetZoomAction = new QAction("&Reset Zoom", this); resetZoomAction->setShortcut(Qt::CTRL | Qt::Key_0);
    toggleGridAction= new QAction("Show &Grid",  this); toggleGridAction->setCheckable(true); toggleGridAction->setChecked(true);

    connect(zoomInAction,    &QAction::triggered, this, [this](){ view->scale(1.2,1.2); updateStatusBar(); });
    connect(zoomOutAction,   &QAction::triggered, this, [this](){ view->scale(1.0/1.2,1.0/1.2); updateStatusBar(); });
    connect(resetZoomAction, &QAction::triggered, this, [this](){ view->resetTransform(); updateStatusBar(); });
    connect(toggleGridAction,&QAction::toggled,   scene, &DiagramScene::setGridVisible);

    toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    auto makeTool = [&](const QString &name, DiagramScene::Mode mode) -> QAction * {
        QAction *a = new QAction(name, this);
        a->setCheckable(true);
        toolGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode](){ setTool(mode); });
        return a;
    };
    selectAction        = makeTool("Select",    DiagramScene::SelectMode);
    insertRectAction    = makeTool("Rectangle", DiagramScene::InsertRectMode);
    insertEllipseAction = makeTool("Ellipse",   DiagramScene::InsertEllipseMode);
    insertLineAction    = makeTool("Line",       DiagramScene::InsertLineMode);
    insertArrowAction   = makeTool("Arrow",      DiagramScene::InsertArrowMode);
    insertTextAction    = makeTool("Text",       DiagramScene::InsertTextMode);
    selectAction->setChecked(true);

    aboutAction = new QAction("&About", this);
    connect(aboutAction, &QAction::triggered, this, [this](){
        QMessageBox::about(this,"DrawTools","DrawTools — Diagram Editor\n\nCtrl+Z/Y Undo/Redo\nCtrl+Wheel Zoom\nMiddle Mouse Pan");
    });
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(newAction); fileMenu->addAction(openAction);
    fileMenu->addAction(saveAction); fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exportPngAction); fileMenu->addAction(exportSvgAction);
    fileMenu->addSeparator(); fileMenu->addAction(exitAction);

    editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoAction); editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(cutAction); editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction); editMenu->addAction(duplicateAction);
    editMenu->addSeparator();
    editMenu->addAction(deleteAction); editMenu->addAction(selectAllAction);
    editMenu->addSeparator();
    editMenu->addAction(insertEquationAction);
    editMenu->addAction(saveSymbolAction);

    arrangeMenu = menuBar()->addMenu("&Arrange");
    arrangeMenu->addAction(bringToFrontAction); arrangeMenu->addAction(bringForwardAction);
    arrangeMenu->addAction(sendBackwardAction); arrangeMenu->addAction(sendToBackAction);
    arrangeMenu->addSeparator();
    arrangeMenu->addAction(alignLeftAction); arrangeMenu->addAction(alignRightAction);
    arrangeMenu->addAction(alignTopAction);  arrangeMenu->addAction(alignBottomAction);
    arrangeMenu->addAction(alignHCenterAction); arrangeMenu->addAction(alignVCenterAction);
    arrangeMenu->addSeparator();
    arrangeMenu->addAction(distributeHAction); arrangeMenu->addAction(distributeVAction);

    viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(zoomInAction); viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(resetZoomAction); viewMenu->addSeparator();
    viewMenu->addAction(toggleGridAction);

    menuBar()->addMenu("&Help")->addAction(aboutAction);
}

void MainWindow::createToolBars()
{
    QToolBar *fb = addToolBar("File"); fb->setMovable(false);
    fb->addAction(newAction); fb->addAction(openAction); fb->addAction(saveAction);
    fb->addSeparator();
    fb->addAction(undoAction); fb->addAction(redoAction);
    fb->addSeparator();
    fb->addAction(cutAction); fb->addAction(copyAction); fb->addAction(pasteAction);

    QToolBar *tb = addToolBar("Tools"); tb->setMovable(false);
    tb->addWidget(new QLabel(" Tool: "));
    tb->addAction(selectAction); tb->addSeparator();
    tb->addAction(insertRectAction); tb->addAction(insertEllipseAction);
    tb->addAction(insertLineAction); tb->addAction(insertArrowAction);
    tb->addAction(insertTextAction);
    tb->addSeparator(); tb->addWidget(new QLabel(" Grid: "));
    gridSpinBox = new QSpinBox(this);
    gridSpinBox->setRange(5,100); gridSpinBox->setValue(20);
    gridSpinBox->setSuffix(" px"); gridSpinBox->setFixedWidth(75);
    connect(gridSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onGridSizeChanged);
    tb->addWidget(gridSpinBox);

    QToolBar *ab = addToolBar("Arrange"); ab->setMovable(false);
    ab->addAction(bringToFrontAction); ab->addAction(bringForwardAction);
    ab->addAction(sendBackwardAction); ab->addAction(sendToBackAction);
    ab->addSeparator();
    ab->addAction(alignLeftAction); ab->addAction(alignHCenterAction);
    ab->addAction(alignRightAction); ab->addAction(alignTopAction);
    ab->addAction(alignVCenterAction); ab->addAction(alignBottomAction);
}

void MainWindow::createDocks()
{
    symbolLibrary = new SymbolLibrary(this);
    addDockWidget(Qt::RightDockWidgetArea, symbolLibrary);
    connect(symbolLibrary, &SymbolLibrary::symbolActivated,
            this, &MainWindow::onSymbolActivated);

    propertiesPanel = new PropertiesPanel(scene, this);
    addDockWidget(Qt::LeftDockWidgetArea, propertiesPanel);
    connect(scene, &DiagramScene::modeChanged,
            propertiesPanel, &PropertiesPanel::onModeChanged);
}

void MainWindow::newDiagram()
{
    if (!maybeSave()) return;
    scene->clear(); scene->undoStack()->clear(); m_currentFile.clear();
    setWindowTitle("DrawTools — New Diagram");
}

void MainWindow::openDiagram()
{
    if (!maybeSave()) return;
    QString path = QFileDialog::getOpenFileName(this,"Open Diagram",{},
                                                "DrawTools Files (*.dtj);;JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    if (!FileManager::load(scene, path)) {
        QMessageBox::critical(this,"Open Failed","Could not read:\n"+path); return;
    }
    scene->undoStack()->clear(); m_currentFile = path;
    setWindowTitle("DrawTools — " + QFileInfo(path).fileName());
}

void MainWindow::saveDiagram()
{
    if (m_currentFile.isEmpty()) { saveDiagramAs(); return; }
    if (!FileManager::save(scene, m_currentFile)) {
        QMessageBox::critical(this,"Save Failed","Could not write:\n"+m_currentFile); return;
    }
    scene->undoStack()->setClean();
}

void MainWindow::saveDiagramAs()
{
    QString path = QFileDialog::getSaveFileName(this,"Save As",{},
                                                "DrawTools Files (*.dtj);;JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".dtj") && !path.endsWith(".json")) path += ".dtj";
    if (!FileManager::save(scene, path)) {
        QMessageBox::critical(this,"Save Failed","Could not write:\n"+path); return;
    }
    scene->undoStack()->setClean(); m_currentFile = path;
    setWindowTitle("DrawTools — " + QFileInfo(path).fileName());
}

void MainWindow::exportPng()
{
    QString path = QFileDialog::getSaveFileName(this,"Export PNG",{},"PNG (*.png)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".png")) path += ".png";
    QRectF bounds = scene->itemsBoundingRect().adjusted(-20,-20,20,20);
    if (!bounds.isValid() || bounds.isEmpty()) {
        QMessageBox::warning(this, "Export PNG", "Nothing to export.");
        return;
    }
    QImage img(bounds.size().toSize(), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img); p.setRenderHint(QPainter::Antialiasing);
    scene->render(&p, QRectF(), bounds); p.end();
    if (!img.save(path))
        QMessageBox::critical(this,"Export Failed","Could not save:\n"+path);
}

void MainWindow::exportSvg()
{
    QString path = QFileDialog::getSaveFileName(this,"Export SVG",{},"SVG (*.svg)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".svg")) path += ".svg";
    QRectF bounds = scene->itemsBoundingRect().adjusted(-20,-20,20,20);
    if (!bounds.isValid() || bounds.isEmpty()) {
        QMessageBox::warning(this, "Export SVG", "Nothing to export.");
        return;
    }
    QSvgGenerator gen;
    gen.setFileName(path); gen.setSize(bounds.size().toSize());
    gen.setViewBox(bounds);
    QPainter p(&gen); p.setRenderHint(QPainter::Antialiasing);
    scene->render(&p, bounds, bounds); p.end();
}

void MainWindow::deleteSelectedItems()
{
    const auto sel = scene->selectedItems(); if (sel.isEmpty()) return;
    scene->undoStack()->beginMacro("Delete");
    for (auto *i : sel) scene->undoStack()->push(new RemoveItemCommand(scene, i));
    scene->undoStack()->endMacro();
}

void MainWindow::copyItems()      { scene->copySelection(false); }
void MainWindow::cutItems()       { scene->copySelection(true); }
void MainWindow::pasteItems()     { scene->paste(); }
void MainWindow::duplicateItems() { scene->duplicate(); }
void MainWindow::selectAll()      { scene->selectAll(); }

void MainWindow::bringToFront()  { scene->bringToFront(); }
void MainWindow::sendToBack()    { scene->sendToBack(); }
void MainWindow::bringForward()  { scene->bringForward(); }
void MainWindow::sendBackward()  { scene->sendBackward(); }

void MainWindow::alignLeft()
{ auto s=scene->selectedItems(); if(s.size()<2)return; qreal r=s.first()->sceneBoundingRect().left(); for(auto*i:s)i->moveBy(r-i->sceneBoundingRect().left(),0); }

void MainWindow::alignRight()
{ auto s=scene->selectedItems(); if(s.size()<2)return; qreal r=s.first()->sceneBoundingRect().right(); for(auto*i:s)i->moveBy(r-i->sceneBoundingRect().right(),0); }

void MainWindow::alignTop()
{ auto s=scene->selectedItems(); if(s.size()<2)return; qreal r=s.first()->sceneBoundingRect().top(); for(auto*i:s)i->moveBy(0,r-i->sceneBoundingRect().top()); }

void MainWindow::alignBottom()
{ auto s=scene->selectedItems(); if(s.size()<2)return; qreal r=s.first()->sceneBoundingRect().bottom(); for(auto*i:s)i->moveBy(0,r-i->sceneBoundingRect().bottom()); }

void MainWindow::alignHCenter()
{ auto s=scene->selectedItems(); if(s.size()<2)return; qreal r=s.first()->sceneBoundingRect().center().x(); for(auto*i:s)i->moveBy(r-i->sceneBoundingRect().center().x(),0); }

void MainWindow::alignVCenter()
{ auto s=scene->selectedItems(); if(s.size()<2)return; qreal r=s.first()->sceneBoundingRect().center().y(); for(auto*i:s)i->moveBy(0,r-i->sceneBoundingRect().center().y()); }

void MainWindow::distributeH()
{
    auto s=scene->selectedItems(); if(s.size()<3)return;
    std::sort(s.begin(),s.end(),[](QGraphicsItem*a,QGraphicsItem*b){ return a->sceneBoundingRect().left()<b->sceneBoundingRect().left(); });
    qreal L=s.first()->sceneBoundingRect().left(), R=s.last()->sceneBoundingRect().right(), tw=0;
    for(auto*i:s) tw+=i->sceneBoundingRect().width();
    qreal gap=(R-L-tw)/(s.size()-1), x=L;
    for(auto*i:s){ i->moveBy(x-i->sceneBoundingRect().left(),0); x+=i->sceneBoundingRect().width()+gap; }
}

void MainWindow::distributeV()
{
    auto s=scene->selectedItems(); if(s.size()<3)return;
    std::sort(s.begin(),s.end(),[](QGraphicsItem*a,QGraphicsItem*b){ return a->sceneBoundingRect().top()<b->sceneBoundingRect().top(); });
    qreal T=s.first()->sceneBoundingRect().top(), B=s.last()->sceneBoundingRect().bottom(), th=0;
    for(auto*i:s) th+=i->sceneBoundingRect().height();
    qreal gap=(B-T-th)/(s.size()-1), y=T;
    for(auto*i:s){ i->moveBy(0,y-i->sceneBoundingRect().top()); y+=i->sceneBoundingRect().height()+gap; }
}

void MainWindow::onGridSizeChanged(int v) { scene->setGridSize(v); }

void MainWindow::updateStatusBar()
{ m_zoomLabel->setText(QString("  %1%  ").arg(qRound(view->transform().m11()*100))); }

void MainWindow::saveSelectionAsSymbol()
{
    const auto sel=scene->selectedItems();
    if(sel.isEmpty()){ QMessageBox::warning(this,"No Selection","Select items first."); return; }
    bool ok; QString name=QInputDialog::getText(this,"Save Symbol","Name:",QLineEdit::Normal,"Symbol",&ok);
    if(!ok||name.trimmed().isEmpty()) return;
    QRectF bounds;
    for(auto*i:sel) bounds=bounds.united(i->mapToScene(i->boundingRect()).boundingRect());
    bounds=bounds.adjusted(-4,-4,4,4);
    QPixmap px(bounds.size().toSize()); px.fill(Qt::transparent);
    QPainter p(&px); p.setRenderHint(QPainter::Antialiasing);
    scene->render(&p,QRectF(),bounds); p.end();
    symbolLibrary->addSymbol(name,px);
}

void MainWindow::insertEquation()
{
    bool ok = false;
    const QString latex = QInputDialog::getMultiLineText(
        this,
        "Insert Equation",
        "Enter LaTeX-like expression (example: \\frac{a_1+b^2}{\\sqrt{c}}):",
        QString(),
        &ok);
    if (!ok || latex.trimmed().isEmpty()) return;

    const QString html = latexToHtml(latex);
    if (html.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Insert Equation", "Could not parse equation input.");
        return;
    }

    auto *ti = new QGraphicsTextItem;
    const QFont eqFont = standardEquationFont();
    const QString styledHtml = QString(
                                   "<div style=\"font-family:'%1'; font-size:%2pt; font-style:italic;\">%3</div>")
                                   .arg(eqFont.family())
                                   .arg(eqFont.pointSize())
                                   .arg(html);
    ti->document()->setDefaultFont(eqFont);
    ti->setHtml(styledHtml);
    ti->setFont(eqFont);
    ti->setDefaultTextColor(scene->currentTextColor());
    ti->setTextInteractionFlags(Qt::TextEditorInteraction);
    ti->setFlag(QGraphicsItem::ItemIsSelectable);
    ti->setFlag(QGraphicsItem::ItemIsMovable);
    ti->setData(0, latex);
    ti->setPos(view->mapToScene(view->viewport()->rect().center()));

    scene->clearSelection();
    scene->undoStack()->push(new AddItemCommand(scene, ti));
    ti->setSelected(true);
}

void MainWindow::onSymbolActivated(int index)
{
    if (!symbolLibrary->hasSymbolAt(index)) return;
    scene->setCurrentSymbol(symbolLibrary->symbolAt(index).thumbnail);
    setTool(DiagramScene::InsertSymbolMode);
}