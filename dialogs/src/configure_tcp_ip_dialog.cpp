#include "dialogs/configure_tcp_ip_dialog.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"
#include <QHBoxLayout>
#include <QMessageBox>

ConfigureTcpIpDialog::ConfigureTcpIpDialog(const daq::DeviceInfoPtr& deviceInfo, QWidget* parent)
    : QDialog(parent)
    , deviceInfo(deviceInfo)
{
    setWindowTitle("Configure TCP/IP");
    resize(GUIConstants::ADD_DEVICE_CONFIG_DIALOG_WIDTH, GUIConstants::ADD_DEVICE_CONFIG_DIALOG_HEIGHT);
    setMinimumSize(GUIConstants::ADD_DEVICE_CONFIG_DIALOG_MIN_WIDTH, GUIConstants::ADD_DEVICE_CONFIG_DIALOG_MIN_HEIGHT);

    setupUI();

    try
    {
        for (const auto& [iface, networkInterface] : deviceInfo.getNetworkInterfaces())
        {
            const std::string ifaceName = iface.toStdString();
            networkInterfaces[ifaceName] = networkInterface;
            interfaceComboBox->addItem(QString::fromStdString(ifaceName), QString::fromStdString(ifaceName));
        }
    }
    catch (const std::exception& e)
    {
        statusLabel->setText(QString("Failed to load network interfaces: %1").arg(e.what()));
        interfaceComboBox->setEnabled(false);
        submitButton->setEnabled(false);
        return;
    }

    if (interfaceComboBox->count() == 0)
    {
        statusLabel->setText("No network interfaces available for this device.");
        interfaceComboBox->setEnabled(false);
        submitButton->setEnabled(false);
        return;
    }

    loadInterfaceConfiguration(interfaceComboBox->currentData().toString().toStdString());
}

bool ConfigureTcpIpDialog::deviceHasNetworkInterfaces(const daq::DeviceInfoPtr& deviceInfo)
{
    if (!deviceInfo.assigned())
        return false;

    try
    {
        const auto interfaces = deviceInfo.getNetworkInterfaces();
        return interfaces.assigned() && interfaces.getCount() > 0;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void ConfigureTcpIpDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                   GUIConstants::DEFAULT_LAYOUT_MARGIN);
    mainLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    auto* interfaceRow = new QHBoxLayout();
    auto* interfaceLabel = new QLabel("Network interface:", this);
    interfaceComboBox = new QComboBox(this);
    interfaceComboBox->setStyleSheet(GUIStyles::roundedComboBoxStyle());
    connect(interfaceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConfigureTcpIpDialog::onInterfaceSelected);
    interfaceRow->addWidget(interfaceLabel);
    interfaceRow->addWidget(interfaceComboBox, 1);
    mainLayout->addLayout(interfaceRow);

    configViewLayout = new QVBoxLayout();
    configViewLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(configViewLayout, 1);

    statusLabel = new QLabel("", this);
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet(GUIStyles::CAPTION_LABEL);
    mainLayout->addWidget(statusLabel);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();

    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonRow->addWidget(cancelButton);

    submitButton = new QPushButton("Submit", this);
    submitButton->setDefault(true);
    connect(submitButton, &QPushButton::clicked, this, &ConfigureTcpIpDialog::onSubmitClicked);
    buttonRow->addWidget(submitButton);

    mainLayout->addLayout(buttonRow);
    setLayout(mainLayout);
}

void ConfigureTcpIpDialog::onInterfaceSelected(int index)
{
    if (index < 0 || index >= interfaceComboBox->count())
        return;

    loadInterfaceConfiguration(interfaceComboBox->itemData(index).toString().toStdString());
}

void ConfigureTcpIpDialog::loadInterfaceConfiguration(const daq::StringPtr& interfaceName)
{
    statusLabel->clear();

    const auto it = networkInterfaces.find(interfaceName.toStdString());
    if (it == networkInterfaces.end())
    {
        statusLabel->setText("Network interface not found.");
        submitButton->setEnabled(false);
        selectedInterface = nullptr;
        currentConfig = nullptr;
        updateConfigView();
        return;
    }

    selectedInterface = it->second;
    submitButton->setEnabled(true);

    try
    {
        currentConfig = selectedInterface.requestCurrentConfiguration();
    }
    catch (const std::exception&)
    {
        try
        {
            currentConfig = selectedInterface.createDefaultConfiguration();
        }
        catch (const std::exception& e)
        {
            statusLabel->setText(QString("Failed to load configuration: %1").arg(e.what()));
            submitButton->setEnabled(false);
            currentConfig = nullptr;
        }
    }

    updateConfigView();
}

void ConfigureTcpIpDialog::updateConfigView()
{
    if (configView)
    {
        configViewLayout->removeWidget(configView);
        configView->deleteLater();
        configView = nullptr;
    }

    if (!currentConfig.assigned())
        return;

    configView = new PropertyObjectView(currentConfig, this);
    configViewLayout->addWidget(configView, 1);
}

void ConfigureTcpIpDialog::onSubmitClicked()
{
    if (!selectedInterface.assigned() || !currentConfig.assigned())
        return;

    try
    {
        selectedInterface.submitConfiguration(currentConfig);
        accept();
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, "Configure TCP/IP", QString("Failed to submit configuration: %1").arg(e.what()));
    }
}
