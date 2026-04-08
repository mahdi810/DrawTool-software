#include "symbollibrary.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>

SymbolLibrary::SymbolLibrary(QWidget *parent)
    : QDockWidget("Symbol Library", parent)
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("Double-click to place:"));

    m_listWidget = new QListWidget(container);
    m_listWidget->setIconSize(QSize(72, 72));
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setSpacing(4);
    layout->addWidget(m_listWidget);

    container->setLayout(layout);
    setWidget(container);
    setMinimumWidth(170);

    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this]() {
        int idx = m_listWidget->currentRow();
        if (idx >= 0) emit symbolActivated(idx);
    });
}

void SymbolLibrary::addSymbol(const QString &name, const QPixmap &thumbnail)
{
    m_symbols.append({name, thumbnail});
    m_listWidget->addItem(new QListWidgetItem(QIcon(thumbnail), name));
}