#include "component/signal_tree_element.h"
#include "DetachableTabWidget.h"
#include "widgets/signal_value_widget.h"

SignalTreeElement::SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, LayoutManager* layoutManager, QObject* parent)
    : ComponentTreeElement(tree, daqSignal, layoutManager, parent)
{
    this->type = "Signal";
    this->iconName = "signal";
}

QStringList SignalTreeElement::getAvailableTabNames() const
{
    QStringList tabs = Super::getAvailableTabNames();
    tabs << "Value";
    return tabs;
}

void SignalTreeElement::openTab(const QString& tabName)
{
    if (!layoutManager)
        return;
        
    if (tabName == "Value")
    {
        auto valueWidget = new SignalValueWidget(daqComponent);
        addTab(valueWidget, tabName, LayoutZone::Right);
    }
    else
    {
        Super::openTab(tabName);
    }
}

