#pragma once
#include "base_tree_element.h"
#include <opendaq/opendaq.h>
#include <QString>
#include <QTreeWidget>

// Factory function to create appropriate tree element based on component type
BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QObject* parent = nullptr);
