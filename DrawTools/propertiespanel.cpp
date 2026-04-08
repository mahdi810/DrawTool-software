#include "propertiespanel.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QColorDialog>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>

#include "diagramrectitem.h"

static QString colorBtnStyle(const QColor &c)
{
    return QString("background:%1; border:1px solid #aaa; border-radius:3px;")
    .arg(c.name(QColor::HexArgb));
}

// ──────────────────────────────────────────────────
//  Constructor
// ──────────────────────────────────────────────────
PropertiesPanel::PropertiesPanel(DiagramScene *scene, QWidget *parent)
    : QDockWidget("Properties", parent)
    , m_scene(scene)
{
    setMinimumWidth(215);
    setMaximumWidth(265);
    buildUI();

    connect(m_scene, &DiagramScene::selectionChanged,
            this, &PropertiesPanel::onSelectionChanged);
}

// ──────────────────────────────────────────────────
//  Build UI
// ──────────────────────────────────────────────────
void PropertiesPanel::buildUI()
{
    m_stack = new QStackedWidget;
    m_stack->addWidget(buildSelectPage());  // 0
    m_stack->addWidget(buildShapePage());   // 1
    m_stack->addWidget(buildArrowPage());   // 2
    m_stack->addWidget(buildTextPage());    // 3

    QWidget     *container = new QWidget;
    QVBoxLayout *lay       = new QVBoxLayout(container);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addWidget(m_stack);
    lay->addStretch();
    setWidget(container);
}

// ── Select page ───────────────────────────────────
QWidget *PropertiesPanel::buildSelectPage()
{
    QWidget     *w   = new QWidget;
    QVBoxLayout *lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 16, 8, 8);
    QLabel *lbl = new QLabel("Select an item or\nchoose a draw tool\nto see properties.");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color:#999; font-style:italic; font-size:11px;");
    lay->addWidget(lbl);
    return w;
}

// ── Shape page ────────────────────────────────────
QWidget *PropertiesPanel::buildShapePage()
{
    QWidget     *w    = new QWidget;
    QFormLayout *form = new QFormLayout(w);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    // Stroke
    form->addRow(new QLabel("<b>Stroke</b>"));

    m_penColorBtn = new QPushButton;
    m_penColorBtn->setFixedHeight(22);
    setButtonColor(m_penColorBtn, Qt::black);
    connect(m_penColorBtn, &QPushButton::clicked, this, &PropertiesPanel::choosePenColor);
    form->addRow("Color:", m_penColorBtn);

    m_penWidthSpin = new QDoubleSpinBox;
    m_penWidthSpin->setRange(0.5, 20.0);
    m_penWidthSpin->setSingleStep(0.5);
    m_penWidthSpin->setValue(1.5);
    m_penWidthSpin->setSuffix(" px");
    connect(m_penWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertiesPanel::onShapePropertyChanged);
    form->addRow("Width:", m_penWidthSpin);

    m_penStyleCombo = new QComboBox;
    m_penStyleCombo->addItem("Solid",    static_cast<int>(Qt::SolidLine));
    m_penStyleCombo->addItem("Dashed",   static_cast<int>(Qt::DashLine));
    m_penStyleCombo->addItem("Dotted",   static_cast<int>(Qt::DotLine));
    m_penStyleCombo->addItem("Dash-Dot", static_cast<int>(Qt::DashDotLine));
    connect(m_penStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::onShapePropertyChanged);
    form->addRow("Style:", m_penStyleCombo);

    // Divider
    QFrame *div = new QFrame;
    div->setFrameShape(QFrame::HLine);
    div->setFrameShadow(QFrame::Sunken);
    form->addRow(div);

    // Corner radius
    m_cornerRadiusRow = new QWidget;
    QFormLayout *crForm = new QFormLayout(m_cornerRadiusRow);
    crForm->setContentsMargins(0,0,0,0);
    crForm->setSpacing(6);

    m_cornerRadiusSpin = new QDoubleSpinBox;
    m_cornerRadiusSpin->setRange(0.0, 200.0);
    m_cornerRadiusSpin->setSingleStep(2.0);
    m_cornerRadiusSpin->setValue(0.0);
    m_cornerRadiusSpin->setSuffix(" px");
    connect(m_cornerRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertiesPanel::onShapePropertyChanged);
    crForm->addRow("Corner r:", m_cornerRadiusSpin);
    form->addRow(m_cornerRadiusRow);

    // Fill
    m_fillGroup = new QWidget;
    QFormLayout *fillForm = new QFormLayout(m_fillGroup);
    fillForm->setContentsMargins(0,0,0,0);
    fillForm->setSpacing(6);

    fillForm->addRow(new QLabel("<b>Fill</b>"));

    m_fillCheck = new QCheckBox("Enable fill");
    m_fillCheck->setChecked(true);
    connect(m_fillCheck, &QCheckBox::toggled, this, &PropertiesPanel::onShapePropertyChanged);
    fillForm->addRow(m_fillCheck);

    m_fillColorBtn = new QPushButton;
    m_fillColorBtn->setFixedHeight(22);
    setButtonColor(m_fillColorBtn, QColor(220, 235, 255));
    connect(m_fillColorBtn, &QPushButton::clicked, this, &PropertiesPanel::chooseFillColor);
    fillForm->addRow("Color:", m_fillColorBtn);

    m_fillAlphaSlider = new QSlider(Qt::Horizontal);
    m_fillAlphaSlider->setRange(0, 255);
    m_fillAlphaSlider->setValue(180);
    m_fillAlphaLabel = new QLabel("180");
    m_fillAlphaLabel->setFixedWidth(28);

    QHBoxLayout *alphaLay = new QHBoxLayout;
    alphaLay->setContentsMargins(0,0,0,0);
    alphaLay->addWidget(m_fillAlphaSlider);
    alphaLay->addWidget(m_fillAlphaLabel);
    QWidget *alphaW = new QWidget; alphaW->setLayout(alphaLay);
    connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v){
        m_fillAlphaLabel->setText(QString::number(v));
        onShapePropertyChanged();
    });
    fillForm->addRow("Opacity:", alphaW);
    form->addRow(m_fillGroup);

    return w;
}

// ── Arrow page ────────────────────────────────────
QWidget *PropertiesPanel::buildArrowPage()
{
    QWidget     *w    = new QWidget;
    QFormLayout *form = new QFormLayout(w);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    form->addRow(new QLabel("<b>Line</b>"));

    m_arrowPenColorBtn = new QPushButton;
    m_arrowPenColorBtn->setFixedHeight(22);
    setButtonColor(m_arrowPenColorBtn, Qt::black);
    connect(m_arrowPenColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(buttonColor(m_arrowPenColorBtn), this, "Line Color");
        if (c.isValid()) { setButtonColor(m_arrowPenColorBtn, c); onShapePropertyChanged(); }
    });
    form->addRow("Color:", m_arrowPenColorBtn);

    m_arrowWidthSpin = new QDoubleSpinBox;
    m_arrowWidthSpin->setRange(0.5, 20.0);
    m_arrowWidthSpin->setSingleStep(0.5);
    m_arrowWidthSpin->setValue(1.5);
    m_arrowWidthSpin->setSuffix(" px");
    connect(m_arrowWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertiesPanel::onShapePropertyChanged);
    form->addRow("Width:", m_arrowWidthSpin);

    m_arrowPenStyleCombo = new QComboBox;
    m_arrowPenStyleCombo->addItem("Solid",    static_cast<int>(Qt::SolidLine));
    m_arrowPenStyleCombo->addItem("Dashed",   static_cast<int>(Qt::DashLine));
    m_arrowPenStyleCombo->addItem("Dotted",   static_cast<int>(Qt::DotLine));
    m_arrowPenStyleCombo->addItem("Dash-Dot", static_cast<int>(Qt::DashDotLine));
    connect(m_arrowPenStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::onShapePropertyChanged);
    form->addRow("Style:", m_arrowPenStyleCombo);

    QFrame *div = new QFrame;
    div->setFrameShape(QFrame::HLine);
    div->setFrameShadow(QFrame::Sunken);
    form->addRow(div);

    form->addRow(new QLabel("<b>Arrowheads</b>"));

    m_startArrowCombo = new QComboBox;
    m_startArrowCombo->addItem("None",   ArrowItem::NoArrow);
    m_startArrowCombo->addItem("Open",   ArrowItem::OpenArrow);
    m_startArrowCombo->addItem("Filled", ArrowItem::FilledArrow);
    connect(m_startArrowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::onShapePropertyChanged);
    form->addRow("Start:", m_startArrowCombo);

    m_endArrowCombo = new QComboBox;
    m_endArrowCombo->addItem("None",   ArrowItem::NoArrow);
    m_endArrowCombo->addItem("Open",   ArrowItem::OpenArrow);
    m_endArrowCombo->addItem("Filled", ArrowItem::FilledArrow);
    m_endArrowCombo->setCurrentIndex(2); // default: filled at end
    connect(m_endArrowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::onShapePropertyChanged);
    form->addRow("End:", m_endArrowCombo);

    return w;
}

// ── Text page ─────────────────────────────────────
QWidget *PropertiesPanel::buildTextPage()
{
    QWidget     *w    = new QWidget;
    QFormLayout *form = new QFormLayout(w);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    form->addRow(new QLabel("<b>Text</b>"));

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setCurrentFont(QFont("Segoe UI"));
    m_fontCombo->setMaximumWidth(190);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged,
            this, &PropertiesPanel::onTextPropertyChanged);
    form->addRow("Font:", m_fontCombo);

    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(6, 144);
    m_fontSizeSpin->setValue(11);
    m_fontSizeSpin->setSuffix(" pt");
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PropertiesPanel::onTextPropertyChanged);
    form->addRow("Size:", m_fontSizeSpin);

    m_boldBtn      = new QToolButton; m_boldBtn->setText("B");
    m_italicBtn    = new QToolButton; m_italicBtn->setText("I");
    m_underlineBtn = new QToolButton; m_underlineBtn->setText("U");

    for (QToolButton *b : {m_boldBtn, m_italicBtn, m_underlineBtn}) {
        b->setCheckable(true);
        b->setFixedSize(28, 28);
    }
    m_boldBtn->setStyleSheet("font-weight:bold;");
    m_italicBtn->setStyleSheet("font-style:italic;");
    m_underlineBtn->setStyleSheet("text-decoration:underline;");

    connect(m_boldBtn,      &QToolButton::toggled, this, &PropertiesPanel::onTextPropertyChanged);
    connect(m_italicBtn,    &QToolButton::toggled, this, &PropertiesPanel::onTextPropertyChanged);
    connect(m_underlineBtn, &QToolButton::toggled, this, &PropertiesPanel::onTextPropertyChanged);

    QHBoxLayout *styleLay = new QHBoxLayout;
    styleLay->setContentsMargins(0,0,0,0);
    styleLay->addWidget(m_boldBtn);
    styleLay->addWidget(m_italicBtn);
    styleLay->addWidget(m_underlineBtn);
    styleLay->addStretch();
    QWidget *styleW = new QWidget; styleW->setLayout(styleLay);
    form->addRow("Style:", styleW);

    m_textColorBtn = new QPushButton;
    m_textColorBtn->setFixedHeight(22);
    setButtonColor(m_textColorBtn, Qt::black);
    connect(m_textColorBtn, &QPushButton::clicked, this, &PropertiesPanel::chooseTextColor);
    form->addRow("Color:", m_textColorBtn);

    return w;
}

// ── Color helpers ─────────────────────────────────
void PropertiesPanel::setButtonColor(QPushButton *btn, const QColor &c)
{
    btn->setStyleSheet(colorBtnStyle(c));
    btn->setProperty("storedColor", c);
}

QColor PropertiesPanel::buttonColor(QPushButton *btn) const
{
    return btn->property("storedColor").value<QColor>();
}

void PropertiesPanel::choosePenColor()
{
    QColor c = QColorDialog::getColor(buttonColor(m_penColorBtn), this, "Stroke Color");
    if (c.isValid()) { setButtonColor(m_penColorBtn, c); onShapePropertyChanged(); }
}

void PropertiesPanel::chooseFillColor()
{
    QColor c = QColorDialog::getColor(buttonColor(m_fillColorBtn), this,
                                      "Fill Color", QColorDialog::ShowAlphaChannel);
    if (c.isValid()) {
        setButtonColor(m_fillColorBtn, c);
        m_block = true;
        m_fillAlphaSlider->setValue(c.alpha());
        m_fillAlphaLabel->setText(QString::number(c.alpha()));
        m_block = false;
        onShapePropertyChanged();
    }
}

void PropertiesPanel::chooseTextColor()
{
    QColor c = QColorDialog::getColor(buttonColor(m_textColorBtn), this, "Text Color");
    if (c.isValid()) { setButtonColor(m_textColorBtn, c); onTextPropertyChanged(); }
}

// ── Apply shape properties ────────────────────────
void PropertiesPanel::onShapePropertyChanged()
{
    if (m_block) return;

    // Shape pen/brush
    QPen pen;
    pen.setColor    (buttonColor(m_penColorBtn));
    pen.setWidthF   (m_penWidthSpin->value());
    pen.setStyle    (static_cast<Qt::PenStyle>(m_penStyleCombo->currentData().toInt()));
    pen.setCapStyle (Qt::RoundCap);

    QBrush brush(Qt::NoBrush);
    if (m_fillCheck->isChecked()) {
        QColor fc = buttonColor(m_fillColorBtn);
        fc.setAlpha(m_fillAlphaSlider->value());
        brush = QBrush(fc);
    }
    m_scene->setCurrentPen(pen);
    m_scene->setCurrentBrush(brush);

    // Arrow pen
    QPen arrowPen;
    arrowPen.setColor   (buttonColor(m_arrowPenColorBtn));
    arrowPen.setWidthF  (m_arrowWidthSpin->value());
    arrowPen.setStyle   (static_cast<Qt::PenStyle>(m_arrowPenStyleCombo->currentData().toInt()));
    arrowPen.setCapStyle(Qt::RoundCap);

    auto startArrow = static_cast<ArrowItem::ArrowType>(m_startArrowCombo->currentData().toInt());
    auto endArrow   = static_cast<ArrowItem::ArrowType>(m_endArrowCombo->currentData().toInt());
    m_scene->setCurrentStartArrow(startArrow);
    m_scene->setCurrentEndArrow  (endArrow);

    // Apply to selected items live
    for (auto *item : m_scene->selectedItems()) {
        if (auto *ri = dynamic_cast<DiagramRectItem *>(item)) {
            ri->setPen(pen); ri->setBrush(brush);
            ri->setCornerRadius(m_cornerRadiusSpin->value());
        } else if (auto *ei = qgraphicsitem_cast<QGraphicsEllipseItem *>(item)) {
            ei->setPen(pen); ei->setBrush(brush);
        } else if (auto *ai = dynamic_cast<ArrowItem *>(item)) {
            ai->setPen(arrowPen);
            ai->setStartArrow(startArrow);
            ai->setEndArrow(endArrow);
        } else if (auto *li = qgraphicsitem_cast<QGraphicsLineItem *>(item)) {
            li->setPen(pen);
        }
    }
}

// ── Apply text properties ─────────────────────────
void PropertiesPanel::onTextPropertyChanged()
{
    if (m_block) return;

    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_fontSizeSpin->value());
    font.setBold     (m_boldBtn->isChecked());
    font.setItalic   (m_italicBtn->isChecked());
    font.setUnderline(m_underlineBtn->isChecked());
    QColor color = buttonColor(m_textColorBtn);

    m_scene->setCurrentFont(font);
    m_scene->setCurrentTextColor(color);

    for (auto *item : m_scene->selectedItems()) {
        if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(item)) {
            ti->setFont(font);
            ti->setDefaultTextColor(color);
        }
    }
}

// ── Mode changed ──────────────────────────────────
void PropertiesPanel::onModeChanged(DiagramScene::Mode mode)
{
    switch (mode) {
    case DiagramScene::InsertArrowMode:
        m_stack->setCurrentIndex(PAGE_ARROW);
        break;
    case DiagramScene::InsertTextMode:
        m_stack->setCurrentIndex(PAGE_TEXT);
        break;
    case DiagramScene::InsertRectMode:
        m_stack->setCurrentIndex(PAGE_SHAPE);
        m_fillGroup->setVisible(true);
        m_cornerRadiusRow->setVisible(true);
        break;
    case DiagramScene::InsertEllipseMode:
        m_stack->setCurrentIndex(PAGE_SHAPE);
        m_fillGroup->setVisible(true);
        m_cornerRadiusRow->setVisible(false);
        break;
    case DiagramScene::InsertLineMode:
        m_stack->setCurrentIndex(PAGE_SHAPE);
        m_fillGroup->setVisible(false);
        m_cornerRadiusRow->setVisible(false);
        break;
    default:
        onSelectionChanged();
        break;
    }
}

// ── Selection changed ─────────────────────────────
void PropertiesPanel::onSelectionChanged()
{
    auto sel = m_scene->selectedItems();
    if (sel.isEmpty()) { m_stack->setCurrentIndex(PAGE_SELECT); return; }

    QGraphicsItem *item = sel.first();

    if (qgraphicsitem_cast<QGraphicsTextItem *>(item)) {
        m_stack->setCurrentIndex(PAGE_TEXT);
        loadFromItem(item);
    } else if (dynamic_cast<ArrowItem *>(item)) {
        m_stack->setCurrentIndex(PAGE_ARROW);
        loadFromItem(item);
    } else if (dynamic_cast<DiagramRectItem *>(item)) {
        m_stack->setCurrentIndex(PAGE_SHAPE);
        m_fillGroup->setVisible(true);
        m_cornerRadiusRow->setVisible(true);
        loadFromItem(item);
    } else if (qgraphicsitem_cast<QGraphicsEllipseItem *>(item)) {
        m_stack->setCurrentIndex(PAGE_SHAPE);
        m_fillGroup->setVisible(true);
        m_cornerRadiusRow->setVisible(false);
        loadFromItem(item);
    } else if (qgraphicsitem_cast<QGraphicsLineItem *>(item)) {
        m_stack->setCurrentIndex(PAGE_SHAPE);
        m_fillGroup->setVisible(false);
        m_cornerRadiusRow->setVisible(false);
        loadFromItem(item);
    } else {
        m_stack->setCurrentIndex(PAGE_SELECT);
    }
}

void PropertiesPanel::loadFromItem(QGraphicsItem *item)
{
    m_block = true;

    auto loadPen = [&](const QPen &pen, bool isArrow = false) {
        if (isArrow) {
            setButtonColor(m_arrowPenColorBtn, pen.color());
            m_arrowWidthSpin->setValue(pen.widthF());
            int idx = m_arrowPenStyleCombo->findData(static_cast<int>(pen.style()));
            if (idx >= 0) m_arrowPenStyleCombo->setCurrentIndex(idx);
        } else {
            setButtonColor(m_penColorBtn, pen.color());
            m_penWidthSpin->setValue(pen.widthF());
            int idx = m_penStyleCombo->findData(static_cast<int>(pen.style()));
            if (idx >= 0) m_penStyleCombo->setCurrentIndex(idx);
        }
    };

    auto loadBrush = [&](const QBrush &brush) {
        bool has = (brush.style() != Qt::NoBrush);
        m_fillCheck->setChecked(has);
        if (has) {
            QColor fc = brush.color();
            setButtonColor(m_fillColorBtn, fc);
            m_fillAlphaSlider->setValue(fc.alpha());
            m_fillAlphaLabel->setText(QString::number(fc.alpha()));
        }
    };

    if (auto *ri = dynamic_cast<DiagramRectItem *>(item)) {
        loadPen(ri->pen()); loadBrush(ri->brush());
        m_cornerRadiusSpin->setValue(ri->cornerRadius());
    } else if (auto *ei = qgraphicsitem_cast<QGraphicsEllipseItem *>(item)) {
        loadPen(ei->pen()); loadBrush(ei->brush());
    } else if (auto *ai = dynamic_cast<ArrowItem *>(item)) {
        loadPen(ai->pen(), true);
        int si = m_startArrowCombo->findData(static_cast<int>(ai->startArrow()));
        int ei2 = m_endArrowCombo->findData(static_cast<int>(ai->endArrow()));
        if (si >= 0) m_startArrowCombo->setCurrentIndex(si);
        if (ei2 >= 0) m_endArrowCombo->setCurrentIndex(ei2);
    } else if (auto *li = qgraphicsitem_cast<QGraphicsLineItem *>(item)) {
        loadPen(li->pen());
    } else if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(item)) {
        QFont f = ti->font();
        m_fontCombo->setCurrentFont(f);
        m_fontSizeSpin->setValue(f.pointSize() > 0 ? f.pointSize() : 11);
        m_boldBtn->setChecked(f.bold());
        m_italicBtn->setChecked(f.italic());
        m_underlineBtn->setChecked(f.underline());
        setButtonColor(m_textColorBtn, ti->defaultTextColor());
    }

    m_block = false;
}