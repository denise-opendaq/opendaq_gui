#include "component/function_blocks_folder_tree_element.h"
#include "component/device_tree_element.h"
#include "component/function_block_tree_element.h"
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QMessageBox>

FunctionBlocksFolderTreeElement::FunctionBlocksFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent)
    : FolderTreeElement(tree, daqFolder, parent)
{
    this->type = "FunctionBlocksFolder";
    this->iconName = "folder";
}

QMenu* FunctionBlocksFolderTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

    menu->addSeparator();

    if (!dynamic_cast<DeviceTreeElement*>(parentElement) && !dynamic_cast<FunctionBlockTreeElement*>(parentElement))
        return menu;

    // Add Function Block action
    QAction* addFBAction = menu->addAction("Add Function Block");
    connect(addFBAction, &QAction::triggered, this, &FunctionBlocksFolderTreeElement::onAddFunctionBlock);

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

