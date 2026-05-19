#include "widgets/input_port_folder_widget.h"
#include "widgets/input_port_folder_selector.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

InputPortFolderWidget::InputPortFolderWidget(const daq::FolderPtr& folder,
                                             ComponentTreeWidget* componentTree,
                                             QWidget* parent,
                                             const QString& treeIcon)
    : ComponentWidget(folder, parent, Qt::Uninitialized, treeIcon)
    , folder(folder)
    , componentTree(componentTree)
{
    setupUI();
}

InputPortFolderWidget::~InputPortFolderWidget() = default;

void InputPortFolderWidget::populateTabs()
{
    ComponentWidget::populateTabs();

    auto* selector = new InputPortFolderSelector(folder, componentTree);

    auto* container = new QWidget();
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(10, 10, 10, 10);
    containerLayout->addWidget(selector);

    tabs->addTab(container, tr("Signal Selector"));
}
