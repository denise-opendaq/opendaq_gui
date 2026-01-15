#include "component/function_blocks_folder_tree_element.h"
#include "component/device_tree_element.h"
#include "component/function_block_tree_element.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>

FunctionBlocksFolderTreeElement::FunctionBlocksFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, LayoutManager* layoutManager, QObject* parent)
    : FolderTreeElement(tree, daqFolder, layoutManager, parent)
{
    this->type = "FunctionBlocksFolder";
    this->iconName = "folder";
}

QMenu* FunctionBlocksFolderTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

    if (!dynamic_cast<DeviceTreeElement*>(parentElement) && !dynamic_cast<FunctionBlockTreeElement*>(parentElement))
        return menu;

    // Get the first action to insert our items before parent items
    QAction* firstAction = menu->actions().isEmpty() ? nullptr : menu->actions().first();

    // Add Function Block action
    QAction* addFBAction = new QAction("Add Function Block", menu);
    connect(addFBAction, &QAction::triggered, this, &FunctionBlocksFolderTreeElement::onAddFunctionBlock);
    menu->insertAction(firstAction, addFBAction);

    menu->insertSeparator(firstAction);

    return menu;
}

void FunctionBlocksFolderTreeElement::onAddFunctionBlock()
{
    if (!parentElement)
        return;

    // Try to get parent as DeviceTreeElement
    auto parentDeviceElement = dynamic_cast<DeviceTreeElement*>(parentElement);
    if (parentDeviceElement)
    {
        parentDeviceElement->onAddFunctionBlock();
        return;
    }

    // Try to get parent as FunctionBlockTreeElement
    auto parentFBElement = dynamic_cast<FunctionBlockTreeElement*>(parentElement);
    if (parentFBElement)
    {
        parentFBElement->onAddFunctionBlock();
        return;
    }
}

