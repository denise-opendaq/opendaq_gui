#include "component/component_factory.h"
#include "component/base_tree_element.h"
#include <opendaq/opendaq.h>
#include "component/device_tree_element.h"
#include "component/function_block_tree_element.h"
#include "component/server_tree_element.h"
#include "component/folder_tree_element.h"
#include "component/devices_folder_tree_element.h"
#include "component/function_blocks_folder_tree_element.h"
#include "component/servers_folder_tree_element.h"
#include "component/input_port_folder_tree_element.h"
#include "component/signal_tree_element.h"
#include "component/input_port_tree_element.h"
#include "component/component_tree_element.h"
#include "context/AppContext.h"
#include <opendaq/custom_log.h>

BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QTabWidget* contentTabs, QObject* parent)
{
    if (!component.assigned())
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Cannot create tree element: component is null");
        return nullptr;
    }

    try
    {
        if (auto dev = component.asPtrOrNull<daq::IDevice>(); dev.assigned())
            return new DeviceTreeElement(tree, dev, contentTabs, parent);

        if (auto fb = component.asPtrOrNull<daq::IFunctionBlock>(); fb.assigned())
            return new FunctionBlockTreeElement(tree, fb, contentTabs, parent);

        if (auto server = component.asPtrOrNull<daq::IServer>(); server.assigned())
            return new ServerTreeElement(tree, server, contentTabs, parent);

        if (auto folder = component.asPtrOrNull<daq::IFolder>(); folder.assigned())
        {
            auto localId = folder.getLocalId();
            if (localId == "Dev")
                return new DevicesFolderTreeElement(tree, folder, contentTabs, parent);
            if (localId == "FB")
                return new FunctionBlocksFolderTreeElement(tree, folder, contentTabs, parent);
            if (localId == "Srv")
                return new ServersFolderTreeElement(tree, folder, contentTabs, parent);
            if (localId == "IP")
                return new InputPortFolderTreeElement(tree, folder, contentTabs, parent);

            return new FolderTreeElement(tree, folder, contentTabs, parent);
        }

        if (auto sig = component.asPtrOrNull<daq::ISignal>(); sig.assigned())
            return new SignalTreeElement(tree, sig, contentTabs, parent);

        if (auto ip = component.asPtrOrNull<daq::IInputPort>(); ip.assigned())
            return new InputPortTreeElement(tree, ip, contentTabs, parent);

        return new ComponentTreeElement(tree, component, contentTabs, parent);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error creating tree element: {}", e.what());
        return nullptr;
    }
}
