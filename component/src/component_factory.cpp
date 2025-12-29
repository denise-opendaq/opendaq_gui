#include "component/component_factory.h"
#include "component/device_tree_element.h"
#include "component/function_block_tree_element.h"
#include "component/server_tree_element.h"
#include "component/folder_tree_element.h"
#include "component/devices_folder_tree_element.h"
#include "component/function_blocks_folder_tree_element.h"
#include "component/servers_folder_tree_element.h"
#include "component/signal_tree_element.h"
#include "component/input_port_tree_element.h"
#include "component/component_tree_element.h"
#include "context/AppContext.h"
#include <opendaq/custom_log.h>

BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QObject* parent)
{
    if (!component.assigned())
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Cannot create tree element: component is null");
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

        // Check if it's a Server
        if (auto server = component.asPtrOrNull<daq::IServer>(); server.assigned())
            return new ServerTreeElement(tree, server, parent);

        // Check if it's a Folder
        if (auto folder = component.asPtrOrNull<daq::IFolder>(); folder.assigned())
        {
            auto localId = folder.getLocalId();
            if (localId == "Dev")
            {
                return new DevicesFolderTreeElement(tree, folder, parent);
            }
            if (localId == "FB")
            {
                return new FunctionBlocksFolderTreeElement(tree, folder, parent);
            }
            if (localId == "Srv")
            {
                return new ServersFolderTreeElement(tree, folder, parent);
            }
            return new FolderTreeElement(tree, folder, parent);
        }

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
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error creating tree element: {}", e.what());
        return nullptr;
    }
}

