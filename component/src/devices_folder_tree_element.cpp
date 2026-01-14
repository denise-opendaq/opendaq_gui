#include "component/devices_folder_tree_element.h"
#include "dialogs/add_device_dialog.h"
#include "component/device_tree_element.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>

DevicesFolderTreeElement::DevicesFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, LayoutManager* layoutManager, QObject* parent)
    : FolderTreeElement(tree, daqFolder, layoutManager, parent)
{
    this->type = "DevicesFolder";
    this->iconName = "folder";
}

QMenu* DevicesFolderTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);
    QAction* firstAction = menu->actions().isEmpty() ? nullptr : menu->actions().first();

    // Add Device action
    QAction* addDeviceAction = new QAction("Add Device", menu);
    connect(addDeviceAction, &QAction::triggered, this, &DevicesFolderTreeElement::onAddDevice);
    menu->insertAction(firstAction, addDeviceAction);

    menu->insertSeparator(firstAction);

    return menu;
}

void DevicesFolderTreeElement::onAddDevice()
{
    if (!parentElement)
        return;

    auto parentDeviceElement = dynamic_cast<DeviceTreeElement*>(parentElement);
    if (!parentDeviceElement)
        return;

    parentDeviceElement->onAddDevice();
}

