#include "component/signal_tree_element.h"
#include "DetachableTabWidget.h"
#include "widgets/signal_value_widget.h"

SignalTreeElement::SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, QObject* parent)
    : ComponentTreeElement(tree, daqSignal, parent)
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

void SignalTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    if (tabName == "Value") 
    {
        auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
        if (tabWidget) 
        {
            auto valueWidget = new SignalValueWidget(daqComponent);
            addTab(tabWidget, valueWidget, tabName);
        }
    } 
    else 
    {
        Super::openTab(tabName, mainContent);
    }
}

