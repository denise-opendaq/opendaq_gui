#include "component/input_port_tree_element.h"

InputPortTreeElement::InputPortTreeElement(QTreeWidget* tree, const daq::InputPortPtr& daqInputPort, QObject* parent)
    : ComponentTreeElement(tree, daqInputPort, parent)
{
    this->type = "InputPort";
    this->iconName = "input_port";
}

