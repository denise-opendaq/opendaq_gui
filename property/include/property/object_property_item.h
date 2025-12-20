#pragma once

#include "base_property_item.h"

// Forward declaration
class PropertySubtreeBuilder;

// ============================================================================
// ObjectPropertyItem — Property item for nested PropertyObject
// ============================================================================

class ObjectPropertyItem final : public BasePropertyItem
{
public:
    ObjectPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
        : BasePropertyItem(owner, prop)
        , nested(prop.getValue())
    {}

    QString showValue() const override
    {
        return QStringLiteral("PropertyObject");
    }

    bool isValueEditable() const override { return false; }
    bool hasSubtree() const override { return true; }

    void build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self) override;

    void commitEdit(QTreeWidgetItem*, int) override
    {
        // not editable
    }

private:
    daq::PropertyObjectPtr nested;
    bool loaded = false;
};
