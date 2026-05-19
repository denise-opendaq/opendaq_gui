#include "component/signal_tree_element.h"
#include "widgets/signal_widget.h"

SignalTreeElement::SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, QWidget* contentHost, QObject* parent)
    : ComponentTreeElement(tree, daqSignal, contentHost, parent)
{
    this->type = "Signal";
    this->iconName = "signal";
}

void SignalTreeElement::openTab()
{
    auto sig = daqComponent.asPtr<daq::ISignal>(true);
    auto* widget = new SignalWidget(sig, nullptr, iconName);
    setContent(widget);
}

