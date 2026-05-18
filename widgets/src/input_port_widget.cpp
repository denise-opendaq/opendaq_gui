#include "widgets/input_port_widget.h"
#include "widgets/input_port_signal_selector.h"
#include <QVBoxLayout>

InputPortWidget::InputPortWidget(const daq::InputPortPtr& port,
                                 ComponentTreeWidget* tree,
                                 QWidget* parent,
                                 const QString& treeIcon)
    : ComponentWidget(port, parent, Qt::Uninitialized, treeIcon)
    , inputPort(port)
    , componentTree(tree)
{
    setupUI();
}

InputPortWidget::~InputPortWidget() = default;

void InputPortWidget::populateTabs()
{
    ComponentWidget::populateTabs();

    auto* page = new QWidget();
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    signalSelector = new InputPortSignalSelector(inputPort, componentTree, page);
    layout->addWidget(signalSelector);
    layout->addStretch();

    tabs->addTab(page, tr("Signal Selector"));
}
