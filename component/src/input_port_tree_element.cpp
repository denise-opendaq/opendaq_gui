#include "component/input_port_tree_element.h"
#include "widgets/input_port_widget.h"
#include "component/component_tree_widget.h"
#include "DetachableTabWidget.h"

InputPortTreeElement::InputPortTreeElement(QTreeWidget* tree, const daq::InputPortPtr& daqInputPort, QObject* parent)
    : ComponentTreeElement(tree, daqInputPort, parent)
{
    this->type = "InputPort";
    this->iconName = "input_port";
}

QStringList InputPortTreeElement::getAvailableTabNames() const
{
    QStringList tabs = Super::getAvailableTabNames();
    tabs << "Signal Selector";
    return tabs;
}

void InputPortTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    if (tabName == "Signal Selector") 
    {
        auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
        if (tabWidget) 
        {
            auto inputPort = getInputPort();
            auto componentTree = qobject_cast<ComponentTreeWidget*>(tree);
            auto inputPortWidget = new InputPortWidget(inputPort, componentTree);
            addTab(tabWidget, inputPortWidget, tabName);
        }
    } 
    else 
    {
        Super::openTab(tabName, mainContent);
    }
}

daq::InputPortPtr InputPortTreeElement::getInputPort() const
{
    return daqComponent.asPtr<daq::IInputPort>();
}

