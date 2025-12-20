#pragma once

#include "base_property_item.h"

// Forward declarations
class PropertyObjectView;
class QTreeWidgetItem;
class QPoint;

class BoolPropertyItem final : public BasePropertyItem
{
public:
    BoolPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
        : BasePropertyItem(owner, prop)
    {}

    QString showValue() const override
    {
        const auto value = owner.getPropertyValue(prop.getName());
        return value ? QStringLiteral("True") : QStringLiteral("False");
    }

    bool isValueEditable() const override
    {
        return false; // Not editable via text, use double-click instead
    }

    void handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item) override;

    void commitEdit(QTreeWidgetItem*, int) override
    {
        // Not used, editing is done via double-click
    }

private:
    void toggleValue()
    {
        const auto currentValue = owner.getPropertyValue(prop.getName());
        owner.setPropertyValue(getName(), !static_cast<bool>(currentValue));
    }
};