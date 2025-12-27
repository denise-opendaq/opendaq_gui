#include "component/devices_folder_tree_element.h"
#include "dialogs/add_device_dialog.h"
#include "component/device_tree_element.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>

DevicesFolderTreeElement::DevicesFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent)
    : FolderTreeElement(tree, daqFolder, parent)
{
    this->type = "DevicesFolder";
    this->iconName = "folder";
}

QMenu* DevicesFolderTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

    menu->addSeparator();

    // Add Device action
    QAction* addDeviceAction = menu->addAction("Add Device");
    connect(addDeviceAction, &QAction::triggered, this, &DevicesFolderTreeElement::onAddDevice);

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

