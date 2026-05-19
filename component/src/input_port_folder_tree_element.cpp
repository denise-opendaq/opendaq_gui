#include "component/input_port_folder_tree_element.h"
#include "widgets/input_port_folder_widget.h"
#include "component/component_tree_widget.h"

InputPortFolderTreeElement::InputPortFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QWidget* contentHost, QObject* parent)
    : FolderTreeElement(tree, daqFolder, contentHost, parent)
{
    this->type = "InputPortFolder";
    this->iconName = "folder";
}

void InputPortFolderTreeElement::openTab()
{
    if (!contentHost)
        return;

    auto folder = daqComponent.asPtr<daq::IFolder>(true);
    auto* componentTree = qobject_cast<ComponentTreeWidget*>(tree);
    auto* widget = new InputPortFolderWidget(folder, componentTree, nullptr, iconName);
    setContent(widget);
}
