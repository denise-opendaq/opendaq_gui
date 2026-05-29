#include "widgets/input_port_folder_widget.h"
#include "widgets/input_port_folder_selector.h"

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

QStringList InputPortFolderWidget::availableTabs() const
{
    auto result = ComponentWidget::availableTabs();
    result << tr("Signal Selector");
    return result;
}

QWidget* InputPortFolderWidget::openTab(const QString& title, QWidget* parent)
{
    if (title == tr("Signal Selector"))
    {
        auto* selector = new InputPortFolderSelector(folder, componentTree);

        auto* container = new QWidget(parent);
        auto* containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(10, 10, 10, 10);
        containerLayout->addWidget(selector);

        return container;
    }

    return ComponentWidget::openTab(title, parent);
}
