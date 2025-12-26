#pragma once

#include "widgets/signal_value_widget.h"


#include <opendaq/opendaq.h>


class ComponentTreeWidget;
class InputPortSignalSelector;

class InputPortWidget : public SignalValueWidget
{
    Q_OBJECT

public:
    explicit InputPortWidget(const daq::InputPortPtr& inputPort, ComponentTreeWidget* componentTree, QWidget* parent = nullptr);
    ~InputPortWidget() override;

private Q_SLOTS:
    void updateSignal();

private:
    daq::InputPortPtr inputPort;
    ComponentTreeWidget* componentTree;
    InputPortSignalSelector* signalSelector;
};
