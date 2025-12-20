#pragma once

#include "base_property_item.h"
#include <QTreeWidgetItem>
#include <map>

// Forward declarations
class PropertySubtreeBuilder;

// ============================================================================
// ListPropertyItem — Property item for nested List
// ============================================================================

class ListPropertyItem final : public BasePropertyItem
{
public:
    ListPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
        : BasePropertyItem(owner, prop)
        , list(prop.getValue())
    {}

    QString showValue() const override
    {
        if (!list.assigned())
            return QStringLiteral("List (null)");

        return QStringLiteral("List (%1 items)").arg(list.getCount());
    }

    bool hasSubtree() const override { return true; }

    void build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self) override;
    void commitEdit(QTreeWidgetItem* item, int column) override;
    void handle_right_click(PropertyObjectView* view, QTreeWidgetItem* item, const QPoint& globalPos) override;

private:
    daq::ListPtr<daq::IBaseObject> list;
    bool loaded = false;
    std::map<QTreeWidgetItem*, size_t> itemToKeyMap;
};
