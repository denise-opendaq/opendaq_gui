#include "dialogs/add_device_config_dialog.h"
#include "coreobjects/property_factory.h"
#include "opendaq/server_capability_ptr.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"
#include <QSplitter>
#include <QListWidgetItem>
#include <QCheckBox>
#include <QGroupBox>
#include <opendaq/custom_log.h>
#include "context/AppContext.h"
#include <opendaq/logger_component_ptr.h>

AddDeviceConfigDialog::AddDeviceConfigDialog(const daq::DevicePtr& parentDevice, const QString& connectionString, QWidget* parent)
    : QDialog(parent)
    , parentDevice(parentDevice)
    , config(nullptr)
    , deviceInfo(getDeviceInfo(connectionString.toStdString()))
    , defaultConnectionString(connectionString)
    , configurationProtocolComboBox(nullptr)
    , streamingCheckboxContainer(nullptr)
    , configTabs(nullptr)
    , statusLabel(nullptr)
    , addButton(nullptr)
{
    setWindowTitle("Add with config");
    resize(GUIConstants::ADD_DEVICE_CONFIG_DIALOG_WIDTH, GUIConstants::ADD_DEVICE_CONFIG_DIALOG_HEIGHT);
    setMinimumSize(GUIConstants::ADD_DEVICE_CONFIG_DIALOG_MIN_WIDTH, GUIConstants::ADD_DEVICE_CONFIG_DIALOG_MIN_HEIGHT);

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
    
    // === LEFT PANEL (styled like component tree view) ===
    auto* leftPanel = new QWidget(this);
    leftPanel->setObjectName("leftPanel");
    leftPanel->setAttribute(Qt::WA_StyledBackground, true);
    leftPanel->setStyleSheet(GUIStyles::sidebarPanelStyle("leftPanel"));
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN);
    leftLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    const QString checkboxStyle =
        "QCheckBox {"
        "  font-size: 13px;"
        "  color: palette(text);"
        "  spacing: 8px;"
        "  padding: 4px 0;"
        "}";

    auto* protocolLabel = new QLabel(tr("Configuration protocols").toUpper(), leftPanel);
    protocolLabel->setStyleSheet(GUIStyles::SMALL_CAPS_LABEL);
    leftLayout->addWidget(protocolLabel);

    configurationProtocolComboBox = new QComboBox(leftPanel);
    configurationProtocolComboBox->setEditable(false);
    configurationProtocolComboBox->setStyleSheet(GUIStyles::roundedComboBoxStyle());
    connect(configurationProtocolComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddDeviceConfigDialog::onProtocolSelected);
    leftLayout->addWidget(configurationProtocolComboBox);

    leftLayout->addSpacing(4);
    auto* streamingLabel = new QLabel(tr("Streaming protocols").toUpper(), leftPanel);
    streamingLabel->setStyleSheet(GUIStyles::SMALL_CAPS_LABEL);
    leftLayout->addWidget(streamingLabel);

    streamingCheckboxContainer = new QWidget(leftPanel);
    streamingCheckboxContainer->setStyleSheet(checkboxStyle);
    auto* checkboxLayout = new QVBoxLayout(streamingCheckboxContainer);
    checkboxLayout->setContentsMargins(0, 0, 0, 0);
    checkboxLayout->setSpacing(4);
    leftLayout->addWidget(streamingCheckboxContainer);

    leftLayout->addStretch();
    splitter->addWidget(leftPanel);

    // === RIGHT PANEL ===
    auto* rightPanel = new QWidget(this);
    rightPanel->setAttribute(Qt::WA_StyledBackground, true);
    rightPanel->setStyleSheet("QWidget { background-color: palette(base); }");
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN);
    rightLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    configTabs = new QTabWidget(rightPanel);
    configTabs->setDocumentMode(true);
    configTabs->tabBar()->setDrawBase(false);
    configTabs->setStyleSheet(GUIStyles::underlineTabBarStyle());
    rightLayout->addWidget(configTabs, 1);

    auto* bottomFrame = new QWidget(rightPanel);
    bottomFrame->setAttribute(Qt::WA_StyledBackground, true);
    bottomFrame->setStyleSheet("QWidget { background-color: palette(base); }");
    auto* bottomLayout = new QHBoxLayout(bottomFrame);
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    statusLabel = new QLabel("", bottomFrame);
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet(GUIStyles::CAPTION_LABEL);
    bottomLayout->addWidget(statusLabel, 1);

    addButton = new QPushButton(tr("Add device"), bottomFrame);
    addButton->setDefault(true);
    connect(addButton, &QPushButton::clicked, this, &AddDeviceConfigDialog::onAddClicked);
    bottomLayout->addWidget(addButton);

    rightLayout->addWidget(bottomFrame);
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
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error parsing connection string: {}", e.what());
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Unknown error parsing connection string");
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
        config.addProperty(daq::ObjectProperty("General", daq::PropertyObject()));

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
                    if (configurationTypes.hasProperty(protocolId))
                        configurationProtocolComboBox->addItem(protocolNameStr, protocolIdStr);
                    break;
                }
                case daq::ProtocolType::Streaming:
                {
                    if (streamingTypes.hasProperty(protocolId))
                    {
                        auto* cb = new QCheckBox(protocolNameStr, streamingCheckboxContainer);
                        cb->setProperty("protocolId", protocolIdStr);
                        streamingCheckboxContainer->layout()->addWidget(cb);
                        streamingCheckboxes.append(cb);
                    }
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

        if (!streamingCheckboxes.isEmpty())
        {
            configurationProtocolComboBox->addItem("-- Streaming only --", "");

            // Pre-check protocols from PrioritizedStreamingProtocols
            QSet<QString> prioritized;
            if (config.assigned() && config.hasProperty("General"))
            {
                try
                {
                    daq::PropertyObjectPtr generalSection = config.getPropertyValue("General");
                    if (generalSection.hasProperty("PrioritizedStreamingProtocols"))
                    {
                        daq::ListPtr<daq::IString> list = generalSection.getPropertyValue("PrioritizedStreamingProtocols");
                        if (list.assigned())
                            for (const auto& id : list)
                                prioritized.insert(QString::fromStdString(id.toStdString()));
                    }
                }
                catch (const std::exception&) { }
            }

            // Check matching checkboxes; if none prioritized, check the first one
            bool anyChecked = false;
            for (auto* cb : std::as_const(streamingCheckboxes))
            {
                if (prioritized.contains(cb->property("protocolId").toString()))
                {
                    cb->setChecked(true);
                    anyChecked = true;
                }
            }
            if (!anyChecked && !streamingCheckboxes.isEmpty())
                streamingCheckboxes.first()->setChecked(true);

            // Connect after initial state is set
            for (auto* cb : std::as_const(streamingCheckboxes))
                connect(cb, &QCheckBox::checkStateChanged, this, &AddDeviceConfigDialog::onStreamingCheckboxChanged);

            onStreamingCheckboxChanged();
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
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error updating selection widgets: {}", e.what());
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

        {
            daq::PropertyObjectPtr streamingProps = config.getPropertyValue("Streaming");
            for (const auto* cb : std::as_const(streamingCheckboxes))
            {
                if (!cb->isChecked())
                    continue;
                QString protocolId = cb->property("protocolId").toString();
                if (streamingProps.hasProperty(protocolId.toStdString()))
                {
                    auto* streamingTab = new PropertyObjectView(streamingProps.getPropertyValue(protocolId.toStdString()), this);
                    configTabs->addTab(streamingTab, cb->text());
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error updating config tabs: {}", e.what());
    }
}

void AddDeviceConfigDialog::updateConnectionString()
{
    if (!selectedConfigurationProtocolId.isEmpty())
    {
        defaultConnectionString = QString::fromStdString(getConnectionStringFromServerCapability(selectedConfigurationProtocolId));
        return;
    }
    for (const auto* cb : std::as_const(streamingCheckboxes))
    {
        if (cb->isChecked())
        {
            defaultConnectionString = QString::fromStdString(getConnectionStringFromServerCapability(cb->property("protocolId").toString()));
            return;
        }
    }
}

void AddDeviceConfigDialog::validateConfig()
{
    const bool streamingOnly = selectedConfigurationProtocolId.isEmpty() && !streamingCheckboxes.isEmpty();
    const bool anyStreamingChecked = std::any_of(streamingCheckboxes.cbegin(), streamingCheckboxes.cend(),
                                                  [](const QCheckBox* cb) { return cb->isChecked(); });

    if (streamingOnly && !anyStreamingChecked)
    {
        addButton->setEnabled(false);
        statusLabel->setText("Select at least one streaming protocol.");
    }
    else
    {
        addButton->setEnabled(true);
        statusLabel->clear();
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
        validateConfig();
    }
}

void AddDeviceConfigDialog::onStreamingCheckboxChanged()
{
    if (!config.assigned() || !config.hasProperty("General"))
        return;

    try
    {
        daq::PropertyObjectPtr generalSection = config.getPropertyValue("General");
        auto allowedList = daq::List<daq::IString>();
        for (const auto* cb : std::as_const(streamingCheckboxes))
            if (cb->isChecked())
                allowedList.pushBack(cb->property("protocolId").toString().toStdString());
        generalSection.setPropertyValue("AllowedStreamingProtocols", allowedList);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error updating AllowedStreamingProtocols: {}", e.what());
    }

    updateConfigTabs();
    updateConnectionString();
    validateConfig();
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
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("No connection string found for protocol: {}", protocolId.toStdString());
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error getting address types from server capability: {}", e.what());
    }

    return connectionString;
}

QString AddDeviceConfigDialog::getConnectionString() const
{
    return defaultConnectionString;
}