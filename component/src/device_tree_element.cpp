#include "component/device_tree_element.h"
#include "widgets/property_object_view.h"
#include "dialogs/add_device_dialog.h"
#include "dialogs/add_function_block_dialog.h"
#include "context/gui_constants.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

DeviceTreeElement::DeviceTreeElement(QTreeWidget* tree, const daq::DevicePtr& daqDevice, QObject* parent)
    : Super(tree, daqDevice, parent)
{
    this->type = "Device";
    this->iconName = "device";
}

bool DeviceTreeElement::visible() const
{
    return true;
}


void DeviceTreeElement::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    Super::onCoreEvent(sender, args);

    auto eventId = static_cast<daq::CoreEventId>(args.getEventId());
    if (eventId == daq::CoreEventId::ConnectionStatusChanged)
    {
        const daq::EnumerationPtr statusEnum = args.getParameters()["StatusValue"];
        const daq::StringPtr statusValue = statusEnum.getValue();
        if (statusValue == "Connected")
            setName(QString::fromStdString(daqComponent.getName()));
        else
            setName(QString::fromStdString(daqComponent.getName() + " [" + statusValue +"]"));
    }
}

void DeviceTreeElement::onSelected(QWidget* mainContent)
{
    // Open all available tabs by calling openTab for each
    QStringList availableTabs = getAvailableTabNames();
    for (const QString& tabName : availableTabs) {
        openTab(tabName, mainContent);
    }
}

QMenu* DeviceTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = Super::onCreateRightClickMenu(parent);

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

    menu->addSeparator();

    QAction* showDeviceInfoAction = menu->addAction("Show Device Info");
    connect(showDeviceInfoAction, &QAction::triggered, this, &DeviceTreeElement::onShowDeviceInfo);

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
}

void DeviceTreeElement::onAddFunctionBlock()
{
    auto device = daqComponent.asPtr<daq::IDevice>();

    AddFunctionBlockDialog dialog(device, nullptr);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString functionBlockType = dialog.getFunctionBlockType();
        if (functionBlockType.isEmpty())
        {
            return;
        }

        try
        {
            // Get config if available (will be nullptr if not set)
            daq::PropertyObjectPtr config = dialog.getConfig();

            // Add function block using IDevice interface
            daq::FunctionBlockPtr newFunctionBlock = device.addFunctionBlock(functionBlockType.toStdString(), config);
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(nullptr, "Error",
                QString("Failed to add function block '%1': %2").arg(functionBlockType, e.what()));
        }
    }
}

void DeviceTreeElement::onShowDeviceInfo()
{
    auto device = daqComponent.asPtr<daq::IDevice>(true);
    auto info = device.getInfo();

    // Create dialog
    QDialog infoDialog(tree->parentWidget());
    infoDialog.setWindowTitle(QString("%1 - Device Info").arg(getName()));
    infoDialog.resize(GUIConstants::DEVICE_INFO_DIALOG_WIDTH, GUIConstants::DEVICE_INFO_DIALOG_HEIGHT);

    auto* layout = new QVBoxLayout(&infoDialog);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create PropertyObjectView with deviceInfo
    auto* propertyView = new PropertyObjectView(info, &infoDialog, daqComponent);
    layout->addWidget(propertyView);

    // Add close button
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton("Close", &infoDialog);
    connect(closeButton, &QPushButton::clicked, &infoDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    infoDialog.exec();
}

