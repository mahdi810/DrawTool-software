#include "mainwindow.h"
#include "diagramview.h"
#include "symbollibrary.h"
#include "propertiespanel.h"
#include "commands.h"
#include "filemanager.h"
#include "resizableequationitem.h"

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
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QTextDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QDir>
#include <QImage>
#include <algorithm>

static constexpr int kEquationRenderDpi = 1200;

static void appendHtmlEscaped(QString &out, QChar ch)
{
    switch (ch.unicode()) {
    case '&': out += "&amp;"; break;
    case '<': out += "&lt;"; break;
    case '>': out += "&gt;"; break;
    case '"': out += "&quot;"; break;
    case '\'': out += "&#39;"; break;
    default: out += ch; break;
    }
}

static QString latexCommandToHtmlEntity(const QString &cmd)
{
    static const std::pair<const char *, const char *> commandMap[] = {
        {"alpha", "&alpha;"}, {"beta", "&beta;"}, {"gamma", "&gamma;"},
        {"delta", "&delta;"}, {"epsilon", "&epsilon;"}, {"theta", "&theta;"},
        {"lambda", "&lambda;"}, {"mu", "&mu;"}, {"pi", "&pi;"},
        {"sigma", "&sigma;"}, {"phi", "&phi;"}, {"omega", "&omega;"},
        {"Gamma", "&Gamma;"}, {"Delta", "&Delta;"}, {"Theta", "&Theta;"},
        {"Lambda", "&Lambda;"}, {"Pi", "&Pi;"}, {"Sigma", "&Sigma;"},
        {"Phi", "&Phi;"}, {"Omega", "&Omega;"},
        {"times", "&times;"}, {"cdot", "&middot;"}, {"pm", "&plusmn;"},
        {"neq", "&ne;"}, {"leq", "&le;"}, {"geq", "&ge;"},
        {"approx", "&asymp;"}, {"infty", "&infin;"}, {"rightarrow", "&rarr;"},
        {"leftarrow", "&larr;"}, {"leftrightarrow", "&harr;"},
        {"sum", "&sum;"}, {"prod", "&prod;"}, {"int", "&int;"},
        {"partial", "&part;"}, {"nabla", "&nabla;"}
    };
    for (const auto &entry : commandMap) {
        if (cmd == QString::fromLatin1(entry.first))
            return QString::fromLatin1(entry.second);
    }
    return QString();
}

static QString parseLatexCore(const QString &src, int &i, bool stopOnBrace);

static QString parseLatexGroupOrAtom(const QString &src, int &i)
{
    if (i >= src.size()) return QString();

    if (src[i] == '{') {
        ++i;
        QString inner = parseLatexCore(src, i, true);
        if (i < src.size() && src[i] == '}') ++i;
        return inner;
    }

    if (src[i] == '\\') {
        ++i;
        const int start = i;
        while (i < src.size() && src[i].isLetter()) ++i;

        if (i > start) {
            const QString cmd = src.mid(start, i - start);

            if (cmd == "frac") {
                const QString num = parseLatexGroupOrAtom(src, i);
                const QString den = parseLatexGroupOrAtom(src, i);
                return QString("<span style=\"white-space:nowrap;\"><sup>%1</sup>&#8260;<sub>%2</sub></span>")
                    .arg(num, den);
            }
            if (cmd == "sqrt") {
                const QString rad = parseLatexGroupOrAtom(src, i);
                return QString("&#8730;(%1)").arg(rad);
            }
            if (cmd == "text") {
                const QString txt = parseLatexGroupOrAtom(src, i);
                return QString("<span style=\"font-style:normal;\">%1</span>").arg(txt);
            }
            if (cmd == "left" || cmd == "right") {
                return QString();
            }
            if (cmd == "," || cmd == ":") return "&thinsp;";
            if (cmd == ";") return "&nbsp;";
            if (cmd == "quad") return "&nbsp;&nbsp;";
            if (cmd == "qquad") return "&nbsp;&nbsp;&nbsp;&nbsp;";

            const QString entity = latexCommandToHtmlEntity(cmd);
            if (!entity.isEmpty()) return entity;

            QString fallback = "\\";
            for (QChar c : cmd) appendHtmlEscaped(fallback, c);
            return fallback;
        }

        // Escaped single character like \{, \}, \%, etc.
        if (i < src.size()) {
            QString out;
            appendHtmlEscaped(out, src[i]);
            ++i;
            return out;
        }
        return "\\";
    }

    QString out;
    if (src[i] == '\n') out = "<br/>";
    else appendHtmlEscaped(out, src[i]);
    ++i;
    return out;
}

static QString parseLatexCore(const QString &src, int &i, bool stopOnBrace)
{
    QString out;

    while (i < src.size()) {
        if (stopOnBrace && src[i] == '}')
            break;

        if (src[i] == '^' || src[i] == '_') {
            const bool sup = (src[i] == '^');
            ++i;
            QString atom = parseLatexGroupOrAtom(src, i);
            if (atom.isEmpty()) atom = " ";
            out += sup ? QString("<sup>%1</sup>").arg(atom)
                       : QString("<sub>%1</sub>").arg(atom);
            continue;
        }

        out += parseLatexGroupOrAtom(src, i);
    }

    return out;
}

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
    int i = 0;
    return parseLatexCore(latex, i, false);
}

static bool runProcessChecked(const QString &program,
                              const QStringList &args,
                              const QString &workingDir,
                              QString &combinedOutput,
                              int timeoutMs = 45000)
{
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    proc.setWorkingDirectory(workingDir);
    proc.start();
    if (!proc.waitForStarted(5000)) {
        combinedOutput += QString("Could not start %1\n").arg(program);
        return false;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(2000);
        combinedOutput += QString("%1 timed out\n").arg(program);
        return false;
    }

    combinedOutput += QString::fromUtf8(proc.readAllStandardOutput());
    combinedOutput += QString::fromUtf8(proc.readAllStandardError());
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

static QString findToolExecutable(const QString &toolName)
{
    QString exe = QStandardPaths::findExecutable(toolName);
    if (!exe.isEmpty()) return exe;

#ifdef Q_OS_WIN
    QString candidate = toolName;
    if (!candidate.endsWith(".exe", Qt::CaseInsensitive))
        candidate += ".exe";

    QStringList roots;
    roots << qEnvironmentVariable("LOCALAPPDATA")
          << qEnvironmentVariable("PROGRAMFILES")
          << qEnvironmentVariable("PROGRAMFILES(X86)");

    const QStringList suffixes = {
        "Programs/MiKTeX/miktex/bin/x64",
        "MiKTeX/miktex/bin/x64",
        "TeXLive/bin/win32"
    };

    for (const QString &root : roots) {
        if (root.isEmpty()) continue;
        for (const QString &suffix : suffixes) {
            const QString full = QDir::fromNativeSeparators(root + "/" + suffix + "/" + candidate);
            if (QFile::exists(full)) return full;
        }
    }
#endif

    return QString();
}

static QPixmap trimTransparentBorders(const QPixmap &pix)
{
    if (pix.isNull()) return pix;
    QImage img = pix.toImage().convertToFormat(QImage::Format_ARGB32);

    int minX = img.width(), minY = img.height();
    int maxX = -1, maxY = -1;

    for (int y = 0; y < img.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) > 0) {
                minX = qMin(minX, x);
                minY = qMin(minY, y);
                maxX = qMax(maxX, x);
                maxY = qMax(maxY, y);
            }
        }
    }

    if (maxX < minX || maxY < minY) return pix;
    const int pad = 3;
    QRect r(QPoint(qMax(0, minX - pad), qMax(0, minY - pad)),
            QPoint(qMin(img.width() - 1, maxX + pad), qMin(img.height() - 1, maxY + pad)));
    return QPixmap::fromImage(img.copy(r));
}

static QPixmap optimizeEquationPixmapForDisplay(const QPixmap &pix, qreal sourceDpi)
{
    if (pix.isNull()) return pix;
    Q_UNUSED(sourceDpi)
    // Keep full source resolution for quality.
    return pix;
}

static qreal equationInitialDisplayScale(qreal sourceDpi)
{
    Q_UNUSED(sourceDpi)
    // Keep inserted equation at native display scale for legibility.
    return 1.0;
}

static bool renderLatexWithEngine(const QString &latex, QPixmap &pixmap, QByteArray &svgData, QString &errorText)
{
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        errorText = "Could not create temporary directory for LaTeX compilation.";
        return false;
    }

    const QString texPath = tmp.filePath("equation.tex");
    QFile texFile(texPath);
    if (!texFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorText = "Could not write temporary LaTeX file.";
        return false;
    }

    const QString trimmedLatex = latex.trimmed();
    const bool isFullDocument = trimmedLatex.contains("\\documentclass") ||
                                trimmedLatex.contains("\\begin{document}");
    const bool hasOwnMathEnv = trimmedLatex.contains("\\begin{") ||
                               trimmedLatex.contains("\\[") ||
                               trimmedLatex.contains("\\(") ||
                               trimmedLatex.contains("$$") ||
                               (trimmedLatex.startsWith('$') && trimmedLatex.endsWith('$'));

    auto writeTex = [&](bool minimalTemplate) {
        if (texFile.isOpen()) texFile.close();
        if (!texFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }
        QTextStream ts(&texFile);
        ts.setEncoding(QStringConverter::Utf8);
        if (isFullDocument) {
            ts << trimmedLatex << "\n";
            texFile.close();
            return true;
        }
        if (minimalTemplate) {
            ts << "\\documentclass[12pt]{article}\n"
                  "\\pagestyle{empty}\n"
                  "\\begin{document}\n"
               << (hasOwnMathEnv ? trimmedLatex : QString("$%1$").arg(latex)) << "\n"
                  "\\end{document}\n";
        } else {
            ts << "\\documentclass[12pt]{article}\n"
                  "\\usepackage{amsmath,amssymb}\n"
                  "\\pagestyle{empty}\n"
                  "\\begin{document}\n"
               << (hasOwnMathEnv
                       ? trimmedLatex + "\n"
                       : QString("\\[\n%1\n\\]\n").arg(latex))
                    <<
                  "\\end{document}\n";
        }
        texFile.close();
        return true;
    };

    if (!writeTex(false)) {
        errorText = "Could not write temporary LaTeX file.";
        return false;
    }

    const QStringList engines = {"xelatex", "pdflatex"};
    QString compilerLog;
    QString usedEngine;
    bool anyEngineFound = false;
    const QString pdfPath = tmp.filePath("equation.pdf");

    for (int pass = 0; pass < 2 && usedEngine.isEmpty(); ++pass) {
        const bool minimalTemplate = (pass == 1);
        if (minimalTemplate && !writeTex(true)) {
            errorText = "Could not rewrite temporary LaTeX file.";
            return false;
        }

        for (const QString &engine : engines) {
            const QString engineExe = findToolExecutable(engine);
            if (engineExe.isEmpty())
                continue;
            anyEngineFound = true;

            bool ok = false;
            for (int attempt = 0; attempt < 2; ++attempt) {
                QFile::remove(pdfPath);
                ok = runProcessChecked(
                    engineExe,
                    {"-interaction=nonstopmode", "-halt-on-error", "-file-line-error", "-enable-installer", "-no-shell-escape", "equation.tex"},
                    tmp.path(),
                    compilerLog);
                if (ok && QFile::exists(pdfPath)) break;
            }

            if (ok && QFile::exists(pdfPath)) {
                usedEngine = engine;
                break;
            }
        }
    }

    if (usedEngine.isEmpty()) {
        if (!anyEngineFound) {
            errorText = "TeX compiler not found.\n"
                        "Install MiKTeX or TeX Live and ensure xelatex/pdflatex is on PATH.\n"
                        "Then restart the app/Qt Creator so PATH changes are picked up.";
        } else {
            errorText = "TeX compiler found but equation compilation failed.\n"
                        "If using MiKTeX, enable: Settings > General > Install missing packages = Always.\n"
                        "(The app already requests -enable-installer.)\n\n"
                        "Compiler output:\n" + compilerLog;
        }
        return false;
    }

    QString convertLog;
    const QString pngBase = tmp.filePath("equation");
    const QString pngPath = tmp.filePath("equation.png");
    const QString svgPath = tmp.filePath("equation.svg");
    const QString croppedPdfPath = tmp.filePath("equation-crop.pdf");

    const QString pdfcropExe    = findToolExecutable("pdfcrop");
    const QString pdftocairoExe = findToolExecutable("pdftocairo");
    const QString pdftoppmExe   = findToolExecutable("pdftoppm");
    const QString magickExe     = findToolExecutable("magick");
    const QString dvisvgmExe    = findToolExecutable("dvisvgm");

    QString pdfForConversion = pdfPath;
    if (!pdfcropExe.isEmpty()) {
        runProcessChecked(pdfcropExe,
                          {"--margins", "2", "equation.pdf", "equation-crop.pdf"},
                          tmp.path(),
                          convertLog,
                          30000);
        if (QFile::exists(croppedPdfPath)) {
            pdfForConversion = croppedPdfPath;
        }
    }

    if (!dvisvgmExe.isEmpty()) {
        runProcessChecked(dvisvgmExe,
                          {"--pdf", "--page=1", "--bbox=min", "--exact-bbox", "--no-fonts", "-o", svgPath, pdfForConversion},
                          tmp.path(),
                          convertLog,
                          30000);
        if (QFile::exists(svgPath)) {
            QFile sf(svgPath);
            if (sf.open(QIODevice::ReadOnly)) {
                svgData = sf.readAll();
                sf.close();
            }
            if (!svgData.isEmpty()) return true;
        }
    }

    if (!pdftocairoExe.isEmpty()) {
        runProcessChecked(pdftocairoExe, {"-singlefile", "-png", "-transp", "-r", QString::number(kEquationRenderDpi), pdfForConversion, pngBase}, tmp.path(), convertLog, 30000);
    }
    if (!QFile::exists(pngPath) && !pdftoppmExe.isEmpty()) {
        runProcessChecked(pdftoppmExe, {"-singlefile", "-png", "-r", QString::number(kEquationRenderDpi), pdfForConversion, pngBase}, tmp.path(), convertLog, 30000);
    }
    if (!QFile::exists(pngPath) && !magickExe.isEmpty()) {
        runProcessChecked(magickExe, {"-density", QString::number(kEquationRenderDpi), "-background", "none", pdfForConversion + "[0]", pngPath}, tmp.path(), convertLog, 30000);
    }

    if (QFile::exists(pngPath)) {
        if (pixmap.load(pngPath) && !pixmap.isNull()) {
            pixmap = trimTransparentBorders(pixmap);
            pixmap = optimizeEquationPixmapForDisplay(pixmap, kEquationRenderDpi);
            return true;
        }
    }

    errorText = "Equation compiled with " + usedEngine +
                " but no PDF converter was found.\n"
                "Install one of: pdftocairo, pdftoppm (Poppler), ImageMagick, or dvisvgm.\n\n"
                "Converter output:\n" + convertLog;
    return false;
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
    applyPageSettingsToScene();
    view->centerOn(scene->pageRect().center());

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

void MainWindow::applyPageSettingsToScene()
{
    QPageSize pageSize(m_pageSizeId);
    QSizeF mm = pageSize.size(QPageSize::Millimeter);
    if (m_pageOrientation == QPageLayout::Landscape) {
        mm = QSizeF(mm.height(), mm.width());
    }

    constexpr qreal pxPerMm = 96.0 / 25.4;
    const qreal w = mm.width() * pxPerMm;
    const qreal h = mm.height() * pxPerMm;
    scene->setPageRect(QRectF(-w / 2.0, -h / 2.0, w, h));
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
    pageSetupAction = new QAction("Page Set&up...", this);
    exportPngAction = new QAction("Export &PNG...", this);
    exportPdfAction = new QAction("Export &PDF...", this);
    exportSvgAction = new QAction("Export &SVG...", this);
    exitAction      = new QAction("E&xit",          this); exitAction->setShortcut(QKeySequence::Quit);

    connect(newAction,       &QAction::triggered, this, &MainWindow::newDiagram);
    connect(openAction,      &QAction::triggered, this, &MainWindow::openDiagram);
    connect(saveAction,      &QAction::triggered, this, &MainWindow::saveDiagram);
    connect(saveAsAction,    &QAction::triggered, this, &MainWindow::saveDiagramAs);
    connect(pageSetupAction, &QAction::triggered, this, &MainWindow::pageSetup);
    connect(exportPngAction, &QAction::triggered, this, &MainWindow::exportPng);
    connect(exportPdfAction, &QAction::triggered, this, &MainWindow::exportPdf);
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
    toggleGridAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G));

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
    fileMenu->addAction(pageSetupAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exportPngAction); fileMenu->addAction(exportPdfAction); fileMenu->addAction(exportSvgAction);
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
        tb->addAction(toggleGridAction);

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

void MainWindow::pageSetup()
{
    const QStringList sizeNames = {"A5", "A4", "A3", "Letter", "Legal"};
    QString currentSize = "A4";
    switch (m_pageSizeId) {
    case QPageSize::A5: currentSize = "A5"; break;
    case QPageSize::A3: currentSize = "A3"; break;
    case QPageSize::Letter: currentSize = "Letter"; break;
    case QPageSize::Legal: currentSize = "Legal"; break;
    default: currentSize = "A4"; break;
    }

    bool ok = false;
    const QString chosenSize = QInputDialog::getItem(this,
                                                     "Page Setup",
                                                     "Page size:",
                                                     sizeNames,
                                                     sizeNames.indexOf(currentSize),
                                                     false,
                                                     &ok);
    if (!ok || chosenSize.isEmpty()) return;

    const QStringList orientations = {"Portrait", "Landscape"};
    const QString currentOrientation = (m_pageOrientation == QPageLayout::Landscape)
                                       ? "Landscape" : "Portrait";
    const QString chosenOrientation = QInputDialog::getItem(this,
                                                            "Page Setup",
                                                            "Orientation:",
                                                            orientations,
                                                            orientations.indexOf(currentOrientation),
                                                            false,
                                                            &ok);
    if (!ok || chosenOrientation.isEmpty()) return;

    if (chosenSize == "A5") m_pageSizeId = QPageSize::A5;
    else if (chosenSize == "A3") m_pageSizeId = QPageSize::A3;
    else if (chosenSize == "Letter") m_pageSizeId = QPageSize::Letter;
    else if (chosenSize == "Legal") m_pageSizeId = QPageSize::Legal;
    else m_pageSizeId = QPageSize::A4;

    m_pageOrientation = (chosenOrientation == "Landscape")
                        ? QPageLayout::Landscape
                        : QPageLayout::Portrait;

    applyPageSettingsToScene();
    view->centerOn(scene->pageRect().center());

    statusBar()->showMessage(QString("Page set to %1 (%2)")
                                 .arg(chosenSize)
                                 .arg(chosenOrientation),
                             3000);
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

void MainWindow::exportPdf()
{
    QString path = QFileDialog::getSaveFileName(this, "Export PDF", {}, "PDF (*.pdf)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";

    const QRectF pageSource = scene->pageRect();
    if (!pageSource.isValid() || pageSource.isEmpty()) {
        QMessageBox::critical(this, "Export PDF", "Invalid page bounds.");
        return;
    }

    constexpr int sceneDpi = 96;
    constexpr int exportDpi = 300;

    QPdfWriter writer(path);
    writer.setResolution(exportDpi);
    writer.setPageSize(QPageSize(m_pageSizeId));
    writer.setPageOrientation(m_pageOrientation);
    writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

    const QRect pageRect = writer.pageLayout().fullRectPixels(writer.resolution());
    if (pageRect.width() <= 0 || pageRect.height() <= 0) {
        QMessageBox::critical(this, "Export PDF", "Invalid page layout.");
        return;
    }

    // Render page as seen on canvas into a high-resolution image first.
    const qreal scale = static_cast<qreal>(exportDpi) / static_cast<qreal>(sceneDpi);
    const QSize imageSize(qMax(1, qRound(pageSource.width() * scale)),
                          qMax(1, qRound(pageSource.height() * scale)));
    QImage pageImage(imageSize, QImage::Format_ARGB32_Premultiplied);
    pageImage.fill(Qt::white);

    // Hide selection handles/borders during export.
    const auto selected = scene->selectedItems();
    for (auto *it : selected) it->setSelected(false);

    {
        QPainter imgPainter(&pageImage);
        imgPainter.setRenderHint(QPainter::Antialiasing, true);
        imgPainter.setRenderHint(QPainter::TextAntialiasing, true);
        imgPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        scene->render(&imgPainter,
                      QRectF(0, 0, imageSize.width(), imageSize.height()),
                      pageSource,
                      Qt::KeepAspectRatio);
    }

    for (auto *it : selected) it->setSelected(true);

    QPainter painter(&writer);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRectF(pageRect), pageImage);
    painter.end();

    statusBar()->showMessage("Exported PDF: " + QFileInfo(path).fileName(), 3000);
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

    QPixmap rendered;
    QByteArray svgData;
    QString renderError;
    if (!renderLatexWithEngine(latex, rendered, svgData, renderError)) {
        const QString fallbackHtml = latexToHtml(latex);
        if (fallbackHtml.trimmed().isEmpty()) {
            QMessageBox::critical(this, "Insert Equation", renderError);
            return;
        }
        const auto btn = QMessageBox::warning(
            this,
            "LaTeX Compiler Not Available",
            renderError + "\n\nUsing simplified renderer as fallback.",
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Ok);
        if (btn == QMessageBox::Cancel) return;

        auto *ti = new QGraphicsTextItem;
        const QFont eqFont = standardEquationFont();
        const QString styledHtml = QString(
                                       "<div style=\"font-family:'%1'; font-size:%2pt; font-style:italic;\">%3</div>")
                                       .arg(eqFont.family())
                                       .arg(eqFont.pointSize())
                                       .arg(fallbackHtml);
        ti->document()->setDefaultFont(eqFont);
        ti->setHtml(styledHtml);
        ti->setFont(eqFont);
        ti->setDefaultTextColor(scene->currentTextColor());
        ti->setTextInteractionFlags(Qt::NoTextInteraction);
        ti->setFlag(QGraphicsItem::ItemIsSelectable);
        ti->setFlag(QGraphicsItem::ItemIsMovable);
        ti->setData(0, latex);
        ti->setPos(view->mapToScene(view->viewport()->rect().center()));

        scene->clearSelection();
        scene->undoStack()->push(new AddItemCommand(scene, ti));
        ti->setSelected(true);
        return;
    }

    QGraphicsItem *eqItem = nullptr;
    if (!svgData.isEmpty()) {
        auto *si = new ResizableEquationItem(svgData);
        si->setData(0, latex);
        eqItem = si;
    } else {
        auto *pi = new ResizableEquationItem(rendered);
        pi->setData(0, latex);
        eqItem = pi;
    }

    const QRectF br = eqItem->boundingRect();
    eqItem->setPos(view->mapToScene(view->viewport()->rect().center())
                   - QPointF(br.width() / 2.0, br.height() / 2.0));

    scene->clearSelection();
    scene->undoStack()->push(new AddItemCommand(scene, eqItem));
    eqItem->setSelected(true);
}

void MainWindow::onSymbolActivated(int index)
{
    if (!symbolLibrary->hasSymbolAt(index)) return;
    scene->setCurrentSymbol(symbolLibrary->symbolAt(index).thumbnail);
    setTool(DiagramScene::InsertSymbolMode);
}