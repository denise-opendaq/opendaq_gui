#pragma once
#include "component_tree_element.h"
#include "signal_value_widget.h"
#include <opendaq/opendaq.h>

class SignalTreeElement : public ComponentTreeElement
{
    Q_OBJECT
    using Super = ComponentTreeElement;

public:
    SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, QObject* parent = nullptr)
        : ComponentTreeElement(tree, daqSignal, parent)
    {
        this->type = "Signal";
        this->iconName = "signal";
    }

    void onSelected(QWidget* mainContent) override
    {
        Super::onSelected(mainContent);

        auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);

        // Add value widget which updates every second
        auto daqSignal = daqComponent.asPtr<daq::ISignal>();
        auto valueWidget = new SignalValueWidget(daqSignal);
        QString tabName = getName() + " Value";
        addTab(tabWidget, valueWidget, tabName);
    }
};
