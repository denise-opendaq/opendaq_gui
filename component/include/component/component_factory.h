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
BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QObject* parent = nullptr);
