#pragma once
#include "base_tree_element.h"
#include "component_tree_element.h"
#include "folder_tree_element.h"
#include "device_tree_element.h"
#include "function_block_tree_element.h"
#include "input_port_tree_element.h"
#include "signal_tree_element.h"
#include <opendaq/opendaq.h>
#include <QString>
#include <QTreeWidget>

// Factory function to create appropriate tree element based on component type
inline BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QObject* parent = nullptr)
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

        // Check if it's a Folder
        if (component.supportsInterface<daq::IFolder>())
        {
            auto folder = component.asPtr<daq::IFolder>();
            return new FolderTreeElement(tree, folder, parent);
        }

        // Check if it's a Device
        if (component.supportsInterface<daq::IDevice>())
        {
            auto device = component.asPtr<daq::IDevice>();
            // You can create a specialized DeviceTreeElement here if needed
            return new ComponentTreeElement(tree, component, parent);
        }

        // Check if it's a Channel
        if (typeStr.contains("Channel", Qt::CaseInsensitive))
        {
            // For now, use ComponentTreeElement
            // You can create ChannelTreeElement later
            return new ComponentTreeElement(tree, component, parent);
        }

        // Check if it's a Signal
        if (component.supportsInterface<daq::ISignal>())
        {
            auto signal = component.asPtr<daq::ISignal>();
            return new SignalTreeElement(tree, signal, parent);
        }

        // Check if it's a FunctionBlock
        if (component.supportsInterface<daq::IFunctionBlock>())
        {
            auto functionBlock = component.asPtr<daq::IFunctionBlock>();
            return new FunctionBlockTreeElement(tree, functionBlock, parent);
        }

        // Check if it's an InputPort
        if (component.supportsInterface<daq::IInputPort>())
        {
            auto inputPort = component.asPtr<daq::IInputPort>();
            return new InputPortTreeElement(tree, inputPort, parent);
        }

        // Default: create generic ComponentTreeElement
        return new ComponentTreeElement(tree, component, parent);
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error creating tree element:" << e.what();
        return nullptr;
    }
}
