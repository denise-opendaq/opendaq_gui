#pragma once
#include "component_tree_element.h"
#include <opendaq/opendaq.h>

class InputPortTreeElement : public ComponentTreeElement
{
    Q_OBJECT

public:
    InputPortTreeElement(QTreeWidget* tree, const daq::InputPortPtr& daqInputPort, QObject* parent = nullptr)
        : ComponentTreeElement(tree, daqInputPort, parent)
    {
        this->type = "InputPort";
        this->iconName = "input_port";
    }
};
