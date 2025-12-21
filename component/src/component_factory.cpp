#include "component/component_factory.h"
#include "component/device_tree_element.h"
#include "component/function_block_tree_element.h"
#include "component/folder_tree_element.h"
#include "component/signal_tree_element.h"
#include "component/input_port_tree_element.h"
#include "component/component_tree_element.h"

BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QObject* parent)
{
    if (!component.assigned())
    {
        qWarning() << "Cannot create tree element: component is null";
        return nullptr;
    }

    try
    {
        // Get component type
        auto componentType = component.getClassName().toStdString();
        QString typeStr = QString::fromStdString(componentType);

        // Check if it's a Device
        if (auto dev = component.asPtrOrNull<daq::IDevice>(); dev.assigned())
            return new DeviceTreeElement(tree, dev, parent);

        // Check if it's a Channel
        // if (auto ch = component.asPtrOrNull<daq::IChannel>(); ch.assigned())
        //     return new ChannelBlockTreeElement(tree, ch, parent);

        // Check if it's a FunctionBlock
        if (auto fb = component.asPtrOrNull<daq::IFunctionBlock>(); fb.assigned())
            return new FunctionBlockTreeElement(tree, fb, parent);

        // Check if it's a Folder
        if (auto folder = component.asPtrOrNull<daq::IFolder>(); folder.assigned())
            return new FolderTreeElement(tree, folder, parent);

        // Check if it's a Signal
        if (auto sig = component.asPtrOrNull<daq::ISignal>(); sig.assigned())
            return new SignalTreeElement(tree, sig, parent);

        // Check if it's an InputPort
        if (auto ip = component.asPtrOrNull<daq::IInputPort>(); ip.assigned())
            return new InputPortTreeElement(tree, ip, parent);

        // Default: create generic ComponentTreeElement
        return new ComponentTreeElement(tree, component, parent);
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error creating tree element:" << e.what();
        return nullptr;
    }
}

