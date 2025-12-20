#pragma once
#include "component_tree_element.h"
#include <opendaq/opendaq.h>

class SignalTreeElement : public ComponentTreeElement
{
    Q_OBJECT

public:
    SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, QObject* parent = nullptr)
        : ComponentTreeElement(tree, daqSignal, parent)
    {
        this->type = "Signal";
        this->iconName = "signal";
    }
};
