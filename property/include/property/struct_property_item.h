#pragma once

#include "base_property_item.h"
#include <QTreeWidgetItem>
#include <map>


// ============================================================================
// StructPropertyItem — Property item for nested Dict
// ============================================================================

class StructPropertyItem final : public BasePropertyItem
{
public:
    StructPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
        : BasePropertyItem(owner, prop)
        , structObj(prop.getValue())
    {}

    QString showValue() const override
    {
        return QStringLiteral("Struct");
    }

    bool hasSubtree() const override { return true; }

    void refresh(PropertyObjectView& view) override;
    void build_subtree(PropertyObjectView& view, QTreeWidgetItem* self, bool force = false) override;
    void commitEdit(QTreeWidgetItem* item, int column) override;
    void handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item) override;

private:
    daq::StructPtr structObj;
    std::map<QTreeWidgetItem*, daq::StringPtr> itemToKeyMap;
};
