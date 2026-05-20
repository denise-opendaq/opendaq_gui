#include "component/input_port_tree_element.h"
#include "widgets/input_port_widget.h"
#include "component/component_tree_widget.h"
#include <qt_widget_interface/qt_widget_interface.h>

InputPortTreeElement::InputPortTreeElement(QTreeWidget* tree, const daq::InputPortPtr& daqInputPort, QWidget* contentHost, QObject* parent)
    : ComponentTreeElement(tree, daqInputPort, contentHost, parent)
{
    this->type = "InputPort";
    this->iconName = "input_port";
}

void InputPortTreeElement::openTab()
{
    if (!contentHost)
        return;

    auto componentTree = ComponentTreeWidget::fromTreeWidget(tree);
    auto* widget = new InputPortWidget(getInputPort(), componentTree, nullptr, iconName);
    setContent(widget);
}

daq::InputPortPtr InputPortTreeElement::getInputPort() const
{
    return daqComponent.asPtr<daq::IInputPort>();
}

