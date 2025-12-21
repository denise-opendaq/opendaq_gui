#include "component/device_tree_element.h"
#include "DetachableTabWidget.h"
#include "widgets/property_object_view.h"
#include "dialogs/add_device_dialog.h"
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QMessageBox>

DeviceTreeElement::DeviceTreeElement(QTreeWidget* tree, const daq::DevicePtr& daqDevice, QObject* parent)
    : Super(tree, daqDevice, parent)
{
    this->type = "Device";
    this->iconName = "device";
    this->name = getStandardFolderName(this->name);
}

bool DeviceTreeElement::visible() const
{
    return true;
}

void DeviceTreeElement::onSelected(QWidget* mainContent)
{
    // Open all available tabs by calling openTab for each
    QStringList availableTabs = getAvailableTabNames();
    for (const QString& tabName : availableTabs) {
        openTab(tabName, mainContent);
    }
}

QStringList DeviceTreeElement::getAvailableTabNames() const
{
    QStringList tabs = Super::getAvailableTabNames();
    tabs << (getName() + " device info");
    return tabs;
}

void DeviceTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    QString deviceInfoTabName = getName() + " device info";
    if (tabName == deviceInfoTabName) {
        auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
        if (tabWidget) {
            auto propertyView = new PropertyObjectView(daqComponent.asPtr<daq::IDevice>(true).getInfo());
            addTab(tabWidget, propertyView, tabName);
        }
    } else {
        Super::openTab(tabName, mainContent);
    }
}

QMenu* DeviceTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = new QMenu(parent);

    QAction* addDeviceAction = menu->addAction("Add Device");
    connect(addDeviceAction, &QAction::triggered, this, &DeviceTreeElement::onAddDevice);

    if (parentElement)
    {
        QAction* removeDeviceAction = menu->addAction("Remove Device");
        connect(removeDeviceAction, &QAction::triggered, this, &DeviceTreeElement::onRemoveDevice);
    }
    menu->addSeparator();

    QAction* addFunctionBlockAction = menu->addAction("Add Function Block");
    connect(addFunctionBlockAction, &QAction::triggered, this, &DeviceTreeElement::onAddFunctionBlock);

    return menu;
}

void DeviceTreeElement::onAddDevice()
{
    auto device = daqComponent.asPtr<daq::IDevice>();

    AddDeviceDialog dialog(device, nullptr);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString connectionString = dialog.getConnectionString();
        if (connectionString.isEmpty())
        {
            return;
        }

        try
        {
            // Get config if available (will be nullptr if not set)
            daq::PropertyObjectPtr config = dialog.getConfig();
            
            // Add device using IDevice interface
            daq::DevicePtr newDevice = device.addDevice(connectionString.toStdString(), config);

            // openDAQ automatically adds the device to the "Dev" folder in the structure
            // We just need to refresh the Devices folder to pick up the new device
            BaseTreeElement* devicesFolder = getChild("Dev");
            
            if (devicesFolder)
            {
                // Refresh the folder to sync with openDAQ structure
                // This will add the new device without duplicates
                auto folderElement = dynamic_cast<FolderTreeElement*>(devicesFolder);
                if (folderElement)
                {
                    folderElement->refresh();
                }
            }
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(nullptr, "Error",
                QString("Failed to add device '%1': %2").arg(connectionString, e.what()));
        }
    }
}

void DeviceTreeElement::onRemoveDevice()
{
    // Remove this device from parent
    if (!parentElement)
        return;

    auto parentDeviceElement = dynamic_cast<ComponentTreeElement*>(parentElement->getParent());
    if (!parentDeviceElement)
        return;

    auto parentDevice = parentDeviceElement->getDaqComponent().asPtrOrNull<daq::IDevice>();
    if (!parentDevice.assigned())
        return;

    parentDevice.removeDevice(daqComponent);
    parentElement->removeChild(this);
}

void DeviceTreeElement::onAddFunctionBlock()
{
    // TODO: Implement adding a new function block
    // This would typically show a dialog to select function block type
    // and then create it via openDAQ API
    qDebug() << "Add function block requested for:" << name;

    // Example placeholder:
    // QString functionBlockType = showFunctionBlockTypeDialog();
    // if (!functionBlockType.isEmpty()) {
    //     try {
    //         auto device = daqComponent.asPtr<daq::IDevice>();
    //         auto fb = device.addFunctionBlock(functionBlockType.toStdString());
    //         auto childElement = createTreeElement(tree, fb, this);
    //         if (childElement) {
    //             addChild(childElement);
    //         }
    //     } catch (const std::exception& e) {
    //         QMessageBox::warning(nullptr, "Error",
    //             QString("Failed to add function block: %1").arg(e.what()));
    //     }
    // }
}

