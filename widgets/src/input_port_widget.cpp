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

QStringList InputPortWidget::availableTabs() const
{
    auto result = ComponentWidget::availableTabs();
    result << tr("Signal Selector");
    return result;
}

QWidget* InputPortWidget::openTab(const QString& title, QWidget* parent)
{
    if (title == tr("Signal Selector"))
    {
        auto* page = new QWidget(parent);
        page->setAttribute(Qt::WA_StyledBackground, true);
        page->setStyleSheet(QStringLiteral("background: transparent;"));

        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        signalSelector = new InputPortSignalSelector(inputPort, componentTree, page);
        layout->addWidget(signalSelector);
        layout->addStretch();

        return page;
    }

    return ComponentWidget::openTab(title, parent);
}
