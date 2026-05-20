#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <unordered_map>
#include <opendaq/opendaq.h>

class PropertyObjectView;

class ConfigureTcpIpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigureTcpIpDialog(const daq::DeviceInfoPtr& deviceInfo, QWidget* parent = nullptr);

    static bool deviceHasNetworkInterfaces(const daq::DeviceInfoPtr& deviceInfo);

private Q_SLOTS:
    void onInterfaceSelected(int index);
    void onSubmitClicked();

private:
    void setupUI();
    void loadInterfaceConfiguration(const daq::StringPtr& interfaceName);
    void updateConfigView();

    daq::DeviceInfoPtr deviceInfo;
    std::unordered_map<std::string, daq::NetworkInterfacePtr> networkInterfaces;
    daq::NetworkInterfacePtr selectedInterface;
    daq::PropertyObjectPtr currentConfig;

    QComboBox* interfaceComboBox = nullptr;
    QVBoxLayout* configViewLayout = nullptr;
    PropertyObjectView* configView = nullptr;
    QPushButton* submitButton = nullptr;
    QLabel* statusLabel = nullptr;
};
