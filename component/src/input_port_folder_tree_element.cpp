#include "component/input_port_folder_tree_element.h"
#include "widgets/input_port_folder_selector.h"
#include "component/component_tree_widget.h"
#include "DetachableTabWidget.h"
#include <QWidget>
#include <QVBoxLayout>

InputPortFolderTreeElement::InputPortFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, LayoutManager* layoutManager, QObject* parent)
    : FolderTreeElement(tree, daqFolder, layoutManager, parent)
{
    this->type = "InputPortFolder";
    this->iconName = "folder";
}

QStringList InputPortFolderTreeElement::getAvailableTabNames() const
{
    QStringList tabs = FolderTreeElement::getAvailableTabNames();
    tabs << "Signal Selector";
    return tabs;
}

void InputPortFolderTreeElement::openTab(const QString& tabName)
{
    if (!layoutManager)
        return;
        
    if (tabName == "Signal Selector")
    {
        auto folder = daqComponent.asPtr<daq::IFolder>(true);
        auto componentTree = qobject_cast<ComponentTreeWidget*>(tree);
        auto inputPortFolderSelector = new InputPortFolderSelector(folder, componentTree);

        // Add margins for standalone usage in tab
        auto container = new QWidget();
        auto containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(10, 10, 10, 10);
        containerLayout->addWidget(inputPortFolderSelector);

        addTab(container, tabName, LayoutZone::Right);
    }
    else
    {
        FolderTreeElement::openTab(tabName);
    }
}

