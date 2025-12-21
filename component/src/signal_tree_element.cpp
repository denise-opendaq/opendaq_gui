#include "component/signal_tree_element.h"
#include "DetachableTabWidget.h"
#include "widgets/signal_value_widget.h"

SignalTreeElement::SignalTreeElement(QTreeWidget* tree, const daq::SignalPtr& daqSignal, QObject* parent)
    : ComponentTreeElement(tree, daqSignal, parent)
{
    this->type = "Signal";
    this->iconName = "signal";
}

void SignalTreeElement::onSelected(QWidget* mainContent)
{
    Super::onSelected(mainContent);

    auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);

    // Add value widget which updates every second
    auto daqSignal = daqComponent.asPtr<daq::ISignal>();
    auto valueWidget = new SignalValueWidget(daqSignal);
    QString tabName = getName() + " Value";
    addTab(tabWidget, valueWidget, tabName);
}

QStringList SignalTreeElement::getAvailableTabNames() const
{
    QStringList tabs = Super::getAvailableTabNames();
    tabs << (getName() + " Value");
    return tabs;
}

void SignalTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    QString valueTabName = getName() + " Value";
    if (tabName == valueTabName) {
        auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
        if (tabWidget) {
            auto daqSignal = daqComponent.asPtr<daq::ISignal>();
            auto valueWidget = new SignalValueWidget(daqSignal);
            addTab(tabWidget, valueWidget, tabName);
        }
    } else {
        Super::openTab(tabName, mainContent);
    }
}

