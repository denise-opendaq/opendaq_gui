#pragma once
#include "component_tree_element.h"
#include "widgets/signal_value_widget.h"
#include <opendaq/opendaq.h>

class SignalTreeElement : public ComponentTreeElement
{
    Q_OBJECT
    using Super = ComponentTreeElement;

public:
    SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, QObject* parent = nullptr);

    void onSelected(QWidget* mainContent) override;
};
