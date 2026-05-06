#pragma once

#include <QWidget>
#include <QPointer>

#include "context/UpdateScheduler.h"
#include <opendaq/device_ptr.h>

class QLabel;
class QTableWidget;
class QToolButton;
class QFrame;
class QScrollArea;

class DeviceWidget : public QWidget, public IUpdatable
{
    Q_OBJECT

public:
    explicit DeviceWidget(const daq::DevicePtr& device, QWidget* parent = nullptr);
    ~DeviceWidget() override;

    void setDevice(const daq::DevicePtr& device);

    void onScheduledUpdate() override;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void setupUi();
    void applyStyle();
    void refresh();

    QWidget* createOverviewCard();
    QWidget* createDeviceInfoCard();
    QWidget* createLogFilesCard();

    void showOperationModeMenu();
    void openLogFile(const QString& id, const QString& title);

    daq::DevicePtr device;

    // Overview card
    QLabel* overviewNameLabel;
    QLabel* overviewTypeLabel;
    QLabel* overviewActiveLabel;
    QLabel* overviewOpModeValue;
    QLabel* overviewTicksSinceOriginValue;
    QLabel* overviewCurrentTimeValue;
    QWidget* overviewConnStatusSection;    // whole "Connection Status" section widget
    QWidget* overviewConnStatusContainer;  // dynamic rows, cleared on each refresh
    QFrame* overviewConnStatusSepLeft;
    QFrame* overviewConnStatusSepRight;

    // Device info card
    QLabel* deviceInfoSerialNumberValue;
    QLabel* deviceInfoLocationValue;
    QLabel* deviceInfoConnectionStringValue;
    QLabel* deviceInfoVendorValue;
    QLabel* deviceInfoModelValue;
    QLabel* deviceInfoFirmwareVersionValue;
    QLabel* deviceInfoHardwareVersionValue;
    QLabel* deviceInfoDriverVersionValue;
    QLabel* deviceInfoDomainValue;

    // Log files card
    QTableWidget* logFilesTable;
    QLabel* logFilesCountLabel;
    QLabel* logFilesTotalSizeLabel;

};
