#pragma once

#include <QWidget>

#include <opendaq/device_ptr.h>

class QLabel;
class QVBoxLayout;

class DeviceLogsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceLogsWidget(const daq::DevicePtr& device, QWidget* parent = nullptr);

private:
    void refresh();

    daq::DevicePtr device;

    QLabel*      countLbl   = nullptr;
    QLabel*      updatedLbl = nullptr;
    QVBoxLayout* listLayout = nullptr;
    QWidget*     listWidget = nullptr;
};
