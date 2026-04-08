#ifndef PROPERTIESPANEL_H
#define PROPERTIESPANEL_H

#include <QDockWidget>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QColor>
#include "diagramscene.h"
#include "arrowitem.h"

class QStackedWidget;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QFontComboBox;
class QToolButton;
class QSlider;
class QLabel;
class QCheckBox;
class QGraphicsItem;

class PropertiesPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(DiagramScene *scene, QWidget *parent = nullptr);

public slots:
    void onModeChanged(DiagramScene::Mode mode);
    void onSelectionChanged();

private slots:
    void choosePenColor();
    void chooseFillColor();
    void chooseTextColor();
    void onShapePropertyChanged();
    void onTextPropertyChanged();

private:
    void    buildUI();
    QWidget *buildSelectPage();
    QWidget *buildShapePage();
    QWidget *buildArrowPage();
    QWidget *buildTextPage();

    void loadFromItem(QGraphicsItem *item);
    void setButtonColor(QPushButton *btn, const QColor &c);
    QColor buttonColor(QPushButton *btn) const;

    DiagramScene   *m_scene;
    QStackedWidget *m_stack;

    static constexpr int PAGE_SELECT = 0;
    static constexpr int PAGE_SHAPE  = 1;
    static constexpr int PAGE_ARROW  = 2;
    static constexpr int PAGE_TEXT   = 3;

    // ── Shape page ────────────────────────────────
    QPushButton    *m_penColorBtn;
    QDoubleSpinBox *m_penWidthSpin;
    QComboBox      *m_penStyleCombo;
    QCheckBox      *m_fillCheck;
    QPushButton    *m_fillColorBtn;
    QSlider        *m_fillAlphaSlider;
    QLabel         *m_fillAlphaLabel;
    QWidget        *m_fillGroup;
    QDoubleSpinBox *m_cornerRadiusSpin;
    QWidget        *m_cornerRadiusRow;

    // ── Arrow page ────────────────────────────────
    QPushButton    *m_arrowPenColorBtn;
    QDoubleSpinBox *m_arrowWidthSpin;
    QComboBox      *m_arrowPenStyleCombo;
    QComboBox      *m_startArrowCombo;
    QComboBox      *m_endArrowCombo;

    // ── Text page ─────────────────────────────────
    QFontComboBox  *m_fontCombo;
    QSpinBox       *m_fontSizeSpin;
    QToolButton    *m_boldBtn;
    QToolButton    *m_italicBtn;
    QToolButton    *m_underlineBtn;
    QPushButton    *m_textColorBtn;

    bool m_block = false;
};

#endif // PROPERTIESPANEL_H