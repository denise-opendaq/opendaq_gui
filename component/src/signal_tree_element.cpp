#include "component/signal_tree_element.h"
#include "widgets/signal_widget.h"

SignalTreeElement::SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, LayoutManager* layoutManager, QObject* parent)
    : ComponentTreeElement(tree, daqSignal, layoutManager, parent)
{
    this->type = "Signal";
    this->iconName = "signal";
}

QStringList SignalTreeElement::getAvailableTabNames() const
{
    QStringList tabs;
    tabs << "Attributes";
    return tabs;
}

void SignalTreeElement::openTab(const QString& tabName)
{
    if (tabName == "Attributes")
    {
        auto sig = daqComponent.asPtr<daq::ISignal>(true);
        auto* widget = new SignalWidget(sig);
        addTab(widget, tabName);
    }
    else
    {
        Super::openTab(tabName);
    }
}

