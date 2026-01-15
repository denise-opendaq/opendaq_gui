#pragma once
// Include Qt headers first to avoid conflicts with openDAQ operators
#include <QString>
#include <QTreeWidget>

// Include openDAQ after Qt headers
#include <opendaq/component_ptr.h>

class BaseTreeElement;
class LayoutManager;

// Factory function to create appropriate tree element based on component type
BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, LayoutManager* layoutManager, QObject* parent = nullptr);
