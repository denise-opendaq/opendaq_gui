#pragma once

#include <QWidget>
#include <opendaq/signal_ptr.h>
#include <coreobjects/core_event_args_ptr.h>
#include <coreobjects/property_object_ptr.h>

class PropertyObjectView;

class SignalDescriptorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignalDescriptorWidget(const daq::SignalPtr& signal, QWidget* parent = nullptr);
    ~SignalDescriptorWidget() override;

private:
    void update();
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

    daq::SignalPtr         signal;
    daq::PropertyObjectPtr descriptorObj;
    PropertyObjectView*    view = nullptr;
};
