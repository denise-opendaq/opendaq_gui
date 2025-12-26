#include "dialogs/add_device_config_dialog.h"
#include "coreobjects/property_factory.h"
#include "opendaq/server_capability_ptr.h"
#include "widgets/property_object_view.h"
#include <QSplitter>
#include <QListWidgetItem>
#include <QCheckBox>
#include <QGroupBox>

AddDeviceConfigDialog::AddDeviceConfigDialog(const daq::DevicePtr& parentDevice, const QString& connectionString, QWidget* parent)
    : QDialog(parent)
    , parentDevice(parentDevice)
    , config(nullptr)
    , deviceInfo(getDeviceInfo(connectionString.toStdString()))
    , defaultConnectionString(connectionString)
    , configurationProtocolComboBox(nullptr)
    , streamingProtocolsComboBox(nullptr)
    , configTabs(nullptr)
    , statusLabel(nullptr)
    , addButton(nullptr)
{
    setWindowTitle("Add with config");
    resize(1200, 600);
    setMinimumSize(1000, 500);

    setupUI();
    initSelectionWidgets();
    updateConfigTabs();
}

daq::DeviceInfoPtr AddDeviceConfigDialog::getDeviceInfo(const daq::StringPtr& connectionString)
{
    for (const auto& deviceInfo : parentDevice.getAvailableDevices())
    {
        if (deviceInfo.getConnectionString() == connectionString) 
            return deviceInfo;
    }
    return nullptr;
}

void AddDeviceConfigDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create horizontal splitter for left and right panels
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    
    // === LEFT PANEL ===
    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(10, 10, 10, 10);

    // Configuration protocols combobox
    auto* protocolLabel = new QLabel("Configuration protocols:", this);
    protocolLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(protocolLabel);
    
    configurationProtocolComboBox = new QComboBox(this);
    configurationProtocolComboBox->setEditable(false);
    connect(configurationProtocolComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AddDeviceConfigDialog::onProtocolSelected);
    leftLayout->addWidget(configurationProtocolComboBox);

    // Streaming protocols combobox
    leftLayout->addSpacing(10);
    auto* streamingLabel = new QLabel("Streaming protocols:", this);
    streamingLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(streamingLabel);
    
    streamingProtocolsComboBox = new QComboBox(this);
    streamingProtocolsComboBox->setEditable(false);
    streamingProtocolsComboBox->setMaxVisibleItems(10);
    connect(streamingProtocolsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddDeviceConfigDialog::onStreamingProtocolSelected);
    leftLayout->addWidget(streamingProtocolsComboBox);

    leftLayout->addStretch();
    leftPanel->setLayout(leftLayout);
    splitter->addWidget(leftPanel);

    // === RIGHT PANEL ===
    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // Config tabs
    configTabs = new QTabWidget(this);
    rightLayout->addWidget(configTabs);

    // Bottom frame with status and button
    auto* bottomFrame = new QWidget(this);
    auto* bottomLayout = new QHBoxLayout(bottomFrame);
    
    statusLabel = new QLabel("", this);
    statusLabel->setWordWrap(true);
    bottomLayout->addWidget(statusLabel, 1);
    
    addButton = new QPushButton("Add device", this);
    addButton->setDefault(true);
    connect(addButton, &QPushButton::clicked, this, &AddDeviceConfigDialog::onAddClicked);
    bottomLayout->addWidget(addButton);
    
    rightLayout->addWidget(bottomFrame);
    rightPanel->setLayout(rightLayout);
    splitter->addWidget(rightPanel);

    // Set splitter proportions (left: 1, right: 3)
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({250, 950});

    mainLayout->addWidget(splitter);
    setLayout(mainLayout);
}

daq::StringPtr getPrefixFromConnectionString(const daq::StringPtr& connectionString)
{
    try
    {
        std::string str = connectionString;
        return str.substr(0, str.find("://"));
    }
    catch(...)
    {
    }

    return "";
}

void AddDeviceConfigDialog::initSelectionWidgets()
{ 
    config = parentDevice.createDefaultAddDeviceConfig();
    daq::PropertyObjectPtr configurationTypes;
    daq::PropertyObjectPtr streamingTypes;

    if (config.hasProperty("Device"))
    {
        configurationTypes = config.getPropertyValue("Device");
    }
    else
    {
        configurationTypes = daq::PropertyObject();
        for (const auto & [id, type] : parentDevice.getAvailableDeviceTypes())
            configurationTypes.addProperty(daq::ObjectProperty(id, type.createDefaultConfig()));
        config.addProperty(daq::ObjectProperty("Device", configurationTypes));
    }

    if (config.hasProperty("Streaming"))
    {
        streamingTypes = config.getPropertyValue("Streaming");
    }
    else
    {
        streamingTypes = daq::PropertyObject();
        config.addProperty(daq::ObjectProperty("Streaming", streamingTypes));
    }

    if (!config.hasProperty("General"))
    {
        config.addProperty(daq::ObjectProperty("General", daq::PropertyObject()));
    }

    try
    {
        const auto caps = deviceInfo.getServerCapabilities();
        for (const auto& cap : caps)
        {
            daq::StringPtr protocolId = cap.getProtocolId();
            daq::StringPtr protocolName = cap.getProtocolName();
            QString protocolIdStr = QString::fromStdString(protocolId.toStdString());
            QString protocolNameStr = QString::fromStdString(protocolName.toStdString());
            
            switch (cap.getProtocolType())
            {
                case daq::ProtocolType::ConfigurationAndStreaming:
                case daq::ProtocolType::Configuration:
                {
                    // Use protocolId for property names in config (as that's what openDAQ uses)
                    if (configurationTypes.hasProperty(protocolId))
                        configurationProtocolComboBox->addItem(protocolNameStr, protocolIdStr);
                    break;
                }
                case daq::ProtocolType::Streaming:
                {
                    // Use protocolId for property names in config (as that's what openDAQ uses)
                    if (streamingTypes.hasProperty(protocolId))
                        streamingProtocolsComboBox->addItem(protocolNameStr, protocolIdStr);
                    break;
                }
                default:
                    break;
            }
        }

        if (caps.getCount() == 0)
        {
            const auto prefix = getPrefixFromConnectionString(deviceInfo.getConnectionString());
            if (prefix.getLength())
            {
                for (const auto& [id, type] : parentDevice.getAvailableDeviceTypes())
                {
                    if (type.getConnectionStringPrefix() == prefix)
                    {
                        auto deviceId = QString::fromStdString(id);
                        configurationProtocolComboBox->addItem(deviceId, deviceId);
                        break;
                    }
                }
            }
        }

        if (streamingProtocolsComboBox->count() > 0)
        {
            // Add streaming only option
            configurationProtocolComboBox->addItem("-- Streaming only --", "");

            // Set default selection for streaming protocol
            selectedStreamingProtocol = streamingProtocolsComboBox->currentText();
            selectedStreamingProtocolId = streamingProtocolsComboBox->currentData().toString();
        }
        
        // Set default selection for configuration protocol
        if (configurationProtocolComboBox->count() > 0)
        {
            selectedConfigurationProtocol = configurationProtocolComboBox->currentText();
            selectedConfigurationProtocolId = configurationProtocolComboBox->currentData().toString();
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error updating selection widgets:" << e.what();
    }
}

void AddDeviceConfigDialog::updateConfigTabs()
{
    configTabs->clear();

    try
    {
        {
            daq::PropertyObjectPtr generalProps = config.getPropertyValue("General");
            if (generalProps.getAllProperties().getCount())
            {
                auto generalTab = new PropertyObjectView(generalProps, this);
                configTabs->addTab(generalTab, "General");
            }
        }

        // Device tab (only if protocol selected)
        if (!selectedConfigurationProtocolId.isEmpty())
        {
            daq::PropertyObjectPtr deviceProps = config.getPropertyValue("Device");
            if (deviceProps.hasProperty(selectedConfigurationProtocolId.toStdString()))
            {
                daq::PropertyObjectPtr configurationProtocolConfig = deviceProps.getPropertyValue(selectedConfigurationProtocolId.toStdString());
                auto deviceTab = new PropertyObjectView(configurationProtocolConfig, this);
                configTabs->addTab(deviceTab, selectedConfigurationProtocol);
            }
        }

        if (!selectedStreamingProtocolId.isEmpty())
        {
            daq::PropertyObjectPtr deviceProps = config.getPropertyValue("Streaming");
            if (deviceProps.hasProperty(selectedStreamingProtocolId.toStdString()))
            {
                daq::PropertyObjectPtr streamingProtocolConfig = deviceProps.getPropertyValue(selectedStreamingProtocolId.toStdString());
                auto streamingTab = new PropertyObjectView(streamingProtocolConfig, this);
                configTabs->addTab(streamingTab, selectedStreamingProtocolId);
            }
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error updating config tabs:" << e.what();
    }
}

void AddDeviceConfigDialog::updateConnectionString()
{
    if (!selectedConfigurationProtocolId.isEmpty())
    {
        defaultConnectionString = QString::fromStdString(getConnectionStringFromServerCapability(selectedConfigurationProtocolId));
    }
    else if (!selectedStreamingProtocolId.isEmpty())
    {
        defaultConnectionString = QString::fromStdString(getConnectionStringFromServerCapability(selectedStreamingProtocolId));
    }
}

void AddDeviceConfigDialog::onProtocolSelected(int index)
{
    if (index >= 0 && index < configurationProtocolComboBox->count())
    {
        selectedConfigurationProtocol = configurationProtocolComboBox->itemText(index);
        selectedConfigurationProtocolId = configurationProtocolComboBox->itemData(index).toString();
        updateConfigTabs();
        updateConnectionString();
    }
}

void AddDeviceConfigDialog::onStreamingProtocolSelected(int index)
{
    selectedStreamingProtocol.clear();
    selectedStreamingProtocolId.clear();
    
    if (index >= 0 && index < streamingProtocolsComboBox->count())
    {
        selectedStreamingProtocol = streamingProtocolsComboBox->itemText(index);
        selectedStreamingProtocolId = streamingProtocolsComboBox->itemData(index).toString();
    }
    
    // Update PrioritizedStreamingProtocols in General section
    if (config.assigned() && config.hasProperty("General"))
    {
        try
        {
            daq::PropertyObjectPtr generalSection = config.getPropertyValue("General");
            generalSection.setPropertyValue("PrioritizedStreamingProtocols", daq::List<daq::IString>(selectedStreamingProtocolId.toStdString()));
        }
        catch (const std::exception& e)
        {
            qWarning() << "Error updating PrioritizedStreamingProtocols:" << e.what();
        }
    }
    
    updateConfigTabs();
    updateConnectionString();
}

void AddDeviceConfigDialog::onAddClicked()
{ 
    accept();
}

daq::StringPtr AddDeviceConfigDialog::getConnectionStringFromServerCapability(const QString& protocolId) 
{
    daq::StringPtr connectionString;
    
    try
    {
        daq::ServerCapabilityPtr cap;
        if (deviceInfo.hasServerCapability(protocolId.toStdString()))
            cap = deviceInfo.getServerCapability(protocolId.toStdString());
        else
            return deviceInfo.getConnectionString();
        
        const daq::PropertyObjectPtr generalSection = config.getPropertyValue("General");
        daq::StringPtr primaryAddressType = "";
        if (generalSection.hasProperty("PrimaryAddressType"))
            primaryAddressType = generalSection.getPropertyValue("PrimaryAddressType");

        for (const auto & addressInfo : cap.getAddressInfo())
        {
            if (!primaryAddressType.getLength() || addressInfo.getType() == primaryAddressType)
            {
                switch (addressInfo.getReachabilityStatus())
                {
                    case daq::AddressReachabilityStatus::Unknown:
                        connectionString = addressInfo.getConnectionString();
                        break;
                    case daq::AddressReachabilityStatus::Reachable:
                        return addressInfo.getConnectionString();
                    case daq::AddressReachabilityStatus::Unreachable:
                        if (!connectionString.assigned())
                            connectionString = addressInfo.getConnectionString();
                        break;
                }
            }
        }
            
        if (!connectionString.assigned())
        {
            qWarning() << "No connection string found for protocol:" << protocolId;
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error getting address types from server capability:" << e.what();
    }

    return connectionString;
}

QString AddDeviceConfigDialog::getConnectionString() const
{
    return defaultConnectionString;
}