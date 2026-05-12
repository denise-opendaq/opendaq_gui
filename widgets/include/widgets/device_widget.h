#pragma once

#include <atomic>

#include "widgets/component_widget.h"
#include <opendaq/device_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class QComboBox;
class QTimer;

class DeviceWidget : public ComponentWidget
{
    Q_OBJECT

public:
    explicit DeviceWidget(const daq::DevicePtr& device, QWidget* parent = nullptr);
    ~DeviceWidget() override;

private:
    QWidget* buildHeaderCard() override;
    void populateTabs() override;

    void updateConnectionStatus();
    void updateOpModeCombo();
    void updateCurrentTime();

    QString formatFrequency(double hz) const;
    QString computeCurrentTime() const;

    void onOpModeChanged(int index);
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

    daq::DevicePtr device;
    std::atomic<int> updatingFromDevice{0};

    QComboBox* opModeCombo    = nullptr;
    QLabel*    domainLabel    = nullptr;
    QLabel*    tickFreqLabel  = nullptr;
    QLabel*    currentTimeLbl = nullptr;
    QTimer*    timer          = nullptr;
};
