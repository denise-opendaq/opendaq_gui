#pragma once

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QTabWidget>
#include <opendaq/opendaq.h>

class AddDeviceConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddDeviceConfigDialog(const daq::DevicePtr& parentDevice, const QString& connectionString, QWidget* parent = nullptr);
    ~AddDeviceConfigDialog() override = default;

    QString getConnectionString() const;
    daq::PropertyObjectPtr getConfig() const { return config; }

private Q_SLOTS:
    void onProtocolSelected(int index);
    void onStreamingCheckboxChanged();
    void onAddClicked();

private:
    daq::DeviceInfoPtr getDeviceInfo(const daq::StringPtr& connectionString);
    void setupUI();
    void createConfig();
    void initSelectionWidgets();
    void updateConnectionString();
    void updateConfigTabs();
    void validateConfig();
    daq::StringPtr getConnectionStringFromServerCapability(const QString& protocolName);

    daq::DevicePtr parentDevice;
    daq::PropertyObjectPtr config;
    QString defaultConnectionString;

    // Left panel widgets
    QComboBox* configurationProtocolComboBox;
    QWidget* streamingCheckboxContainer;
    QList<QCheckBox*> streamingCheckboxes;

    // Right panel widgets
    QTabWidget* configTabs;

    // Bottom widgets
    QLabel* statusLabel;
    QPushButton* addButton;

    QString selectedConfigurationProtocol;
    QString selectedConfigurationProtocolId;
    daq::DeviceInfoPtr deviceInfo;
};

