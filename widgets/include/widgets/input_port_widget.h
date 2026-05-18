#pragma once

#include <QString>

#include "widgets/component_widget.h"
#include <opendaq/input_port_ptr.h>

class ComponentTreeWidget;
class InputPortSignalSelector;

class InputPortWidget : public ComponentWidget
{
    Q_OBJECT

public:
    explicit InputPortWidget(const daq::InputPortPtr& inputPort,
                             ComponentTreeWidget* componentTree,
                             QWidget* parent = nullptr,
                             const QString& treeIconName = {});
    ~InputPortWidget() override;

private:
    void populateTabs() override;

    daq::InputPortPtr inputPort;
    ComponentTreeWidget* componentTree = nullptr;
    InputPortSignalSelector* signalSelector = nullptr;
};
