#include "component/input_port_tree_element.h"
#include "widgets/input_port_widget.h"
#include "component/component_tree_widget.h"
#include <qt_widget_interface/qt_widget_interface.h>

InputPortTreeElement::InputPortTreeElement(QTreeWidget* tree, const daq::InputPortPtr& daqInputPort, LayoutManager* layoutManager, QObject* parent)
    : ComponentTreeElement(tree, daqInputPort, layoutManager, parent)
{
    this->type = "InputPort";
    this->iconName = "input_port";
}

QStringList InputPortTreeElement::getAvailableTabNames() const
{
    QStringList tabs;
    tabs << "Attributes";
    return tabs;
}

void InputPortTreeElement::openTab(const QString& tabName)
{
    if (!layoutManager)
        return;

    if (tabName == "Attributes")
    {
        auto componentTree = qobject_cast<ComponentTreeWidget*>(tree);
        auto* widget = new InputPortWidget(getInputPort(), componentTree, nullptr, iconName);
        addTab(widget, tabName);
    }
    else
    {
        Super::openTab(tabName);
    }
}

daq::InputPortPtr InputPortTreeElement::getInputPort() const
{
    return daqComponent.asPtr<daq::IInputPort>();
}

