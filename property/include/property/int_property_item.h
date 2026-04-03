#pragma once

#include "base_property_item.h"

// Forward declarations
class PropertyObjectView;
class QTreeWidgetItem;
class QPoint;

class IntPropertyItem final : public BasePropertyItem
{
public:
    IntPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop);
};
