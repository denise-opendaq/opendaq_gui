#pragma once

#include "base_property_item.h"

// Forward declarations
class PropertyObjectView;
class QTreeWidgetItem;
class QPoint;

class BoolPropertyItem final : public BasePropertyItem
{
public:
    BoolPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop);

    QString showValue() const override;
    bool isValueEditable() const override;
    void handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item) override;
};