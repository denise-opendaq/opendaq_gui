#pragma once

#include <atomic>

#include <QString>

#include "widgets/component_widget.h"
#include <opendaq/device_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class QComboBox;
class QTimer;

class DeviceWidget : public ComponentWidget
{
    Q_OBJECT

public:
    explicit DeviceWidget(const daq::DevicePtr& device,
                          QWidget* parent = nullptr,
                          const QString& treeIconName = {});

protected:
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args) override;

private:
    QWidget* buildHeaderCard() override;
    QStringList availableTabs() const override;
    QWidget* openTab(const QString& title, QWidget* parent) override;

    void updateConnectionStatus();
    void updateOpModeCombo();
    void updateCurrentTime();

    QString formatFrequency(double hz) const;
    QString computeCurrentTime() const;

    void onOpModeChanged(int index);

    daq::DevicePtr device;
    std::atomic<int> updatingFromDevice{0};

    QComboBox* opModeCombo    = nullptr;
    QLabel*    domainLabel    = nullptr;
    QLabel*    tickFreqLabel  = nullptr;
    QLabel*    currentTimeLbl = nullptr;
    QTimer*    timer          = nullptr;
};
