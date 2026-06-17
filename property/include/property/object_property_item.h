#pragma once

#include "base_property_item.h"

// Forward declaration
class PropertyObjectView;

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
    void build_subtree(PropertyObjectView& view, QTreeWidgetItem* self, bool force = false) override;
    void commitEdit(QTreeWidgetItem*, int) override;

    void refresh(PropertyObjectView& view) override;

private:
    daq::PropertyObjectPtr nested;
};
