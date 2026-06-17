#pragma once

#include <QWidget>

#include <opendaq/device_ptr.h>

class QLabel;
class QVBoxLayout;
class QShowEvent;

class DeviceLogsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceLogsWidget(const daq::DevicePtr& device, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();

    daq::DevicePtr device;

    QLabel*      countLbl   = nullptr;
    QLabel*      updatedLbl = nullptr;
    QVBoxLayout* listLayout = nullptr;
    QWidget*     listWidget = nullptr;
};
