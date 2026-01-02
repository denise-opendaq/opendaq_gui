#include "component/device_tree_element.h"
#include "widgets/property_object_view.h"
#include "dialogs/add_device_dialog.h"
#include "dialogs/add_function_block_dialog.h"
#include "dialogs/load_configuration_dialog.h"
#include "context/gui_constants.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

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

    // Get the first action to insert our items before parent items
    QAction* firstAction = menu->actions().isEmpty() ? nullptr : menu->actions().first();

    QAction* addDeviceAction = new QAction("Add Device", menu);
    connect(addDeviceAction, &QAction::triggered, this, &DeviceTreeElement::onAddDevice);
    menu->insertAction(firstAction, addDeviceAction);

    if (parentElement)
    {
        QAction* removeDeviceAction = new QAction("Remove Device", menu);
        connect(removeDeviceAction, &QAction::triggered, this, &DeviceTreeElement::onRemoveDevice);
        menu->insertAction(firstAction, removeDeviceAction);
    }

    QAction* addFunctionBlockAction = new QAction("Add Function Block", menu);
    connect(addFunctionBlockAction, &QAction::triggered, this, &DeviceTreeElement::onAddFunctionBlock);
    menu->insertAction(firstAction, addFunctionBlockAction);

    menu->insertSeparator(firstAction);

    QAction* showDeviceInfoAction = new QAction("Show Device Info", menu);
    connect(showDeviceInfoAction, &QAction::triggered, this, &DeviceTreeElement::onShowDeviceInfo);
    menu->insertAction(firstAction, showDeviceInfoAction);

    menu->insertSeparator(firstAction);

    QAction* saveConfigAction = new QAction("Save Configuration", menu);
    connect(saveConfigAction, &QAction::triggered, this, &DeviceTreeElement::onSaveConfiguration);
    menu->insertAction(firstAction, saveConfigAction);

    // Insert in reverse order: last item in code = first in menu
    QAction* loadConfigAction = new QAction("Load Configuration", menu);
    connect(loadConfigAction, &QAction::triggered, this, &DeviceTreeElement::onLoadConfiguration);
    menu->insertAction(firstAction, loadConfigAction);

    menu->insertSeparator(firstAction);

    return menu;
}

void DeviceTreeElement::onAddDevice()
{
    auto device = daqComponent.asPtr<daq::IDevice>(true);

    AddDeviceDialog dialog(device, nullptr);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString connectionString = dialog.getConnectionString();
        if (connectionString.isEmpty())
            return;

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

    auto parentDevice = parentDeviceElement->getDaqComponent().asPtrOrNull<daq::IDevice>(true);
    if (!parentDevice.assigned())
        return;

    parentDevice.removeDevice(daqComponent);
}

void DeviceTreeElement::onAddFunctionBlock()
{
    auto device = daqComponent.asPtr<daq::IDevice>(true);

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

void DeviceTreeElement::onSaveConfiguration()
{
    auto device = daqComponent.asPtr<daq::IDevice>(true);

    try
    {
        // Get configuration string from device
        std::string configStr = device.saveConfiguration();

        // Open file dialog to choose save location
        QString fileName = QFileDialog::getSaveFileName(
            tree->parentWidget(),
            "Save Configuration",
            name + "_config.json",
            "JSON Files (*.json);;All Files (*)"
        );

        if (fileName.isEmpty())
            return;

        // Write configuration to file
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::critical(nullptr, "Error",
                QString("Failed to open file for writing: %1").arg(fileName));
            return;
        }

        QTextStream out(&file);
        out << QString::fromStdString(configStr);
        file.close();

        QMessageBox::information(nullptr, "Success",
            QString("Configuration saved to: %1").arg(fileName));
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(nullptr, "Error",
            QString("Failed to save configuration: %1").arg(e.what()));
    }
}

void DeviceTreeElement::onLoadConfiguration()
{
    auto device = daqComponent.asPtr<daq::IDevice>(true);

    // Open dialog to configure load parameters and select file
    LoadConfigurationDialog dialog(tree->parentWidget());
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString configFilePath = dialog.getConfigurationFilePath();
    if (configFilePath.isEmpty())
        return;

    try
    {
        // Read configuration from file
        QFile file(configFilePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMessageBox::critical(nullptr, "Error",
                QString("Failed to open file for reading: %1").arg(configFilePath));
            return;
        }

        QTextStream in(&file);
        QString configContent = in.readAll();
        file.close();

        // Load configuration to device
        daq::UpdateParametersPtr updateParams = dialog.getUpdateParameters();
        device.loadConfiguration(configContent.toStdString(), updateParams);

        QMessageBox::information(nullptr, "Success",
            QString("Configuration loaded from: %1").arg(configFilePath));
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(nullptr, "Error",
            QString("Failed to load configuration: %1").arg(e.what()));
    }
}

