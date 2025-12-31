#pragma once

#include "base_property_item.h"

// Forward declarations
class PropertyObjectView;
class QTreeWidgetItem;
class QPoint;

class FunctionPropertyItem final : public BasePropertyItem
{
public:
    FunctionPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop);

    QString showValue() const override;
    bool isReadOnly() const override;
    bool isValueEditable() const override;
    void handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item) override;
};

