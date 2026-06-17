#pragma once
#include "component_tree_element.h"
#include <opendaq/input_port_ptr.h>

class InputPortTreeElement : public ComponentTreeElement
{
    Q_OBJECT
    using Super = ComponentTreeElement;

public:
    InputPortTreeElement(QTreeWidget* tree, const daq::InputPortPtr& daqInputPort, QWidget* contentHost, QObject* parent = nullptr);

    void openTab() override;

    daq::InputPortPtr getInputPort() const;
};
