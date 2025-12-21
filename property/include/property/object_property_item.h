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
    ObjectPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop);

    QString showValue() const override;
    bool isValueEditable() const override;
    bool hasSubtree() const override;
    void build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self) override;
    void commitEdit(QTreeWidgetItem*, int) override;

private:
    daq::PropertyObjectPtr nested;
    bool loaded = false;
};
