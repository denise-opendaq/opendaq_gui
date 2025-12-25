#pragma once

#include "base_property_item.h"
#include <QTreeWidgetItem>
#include <map>

// Forward declarations
class PropertySubtreeBuilder;

// ============================================================================
// DictPropertyItem — Property item for nested Dict
// ============================================================================

class DictPropertyItem final : public BasePropertyItem
{
public:
    DictPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
        : BasePropertyItem(owner, prop)
        , dict(prop.getValue())
    {}

    QString showValue() const override
    {
        if (!dict.assigned())
            return QStringLiteral("Dict (null)");

        return QStringLiteral("Dict (%1 items)").arg(dict.getCount());
    }

    bool isKeyEditable() const override { return isValueEditable(); }
    bool hasSubtree() const override { return true; }

    void build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self) override;
    void commitEdit(QTreeWidgetItem* item, int column) override;
    void handle_right_click(PropertyObjectView* view, QTreeWidgetItem* item, const QPoint& globalPos) override;
    void handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item) override;

private:
    daq::DictPtr<daq::IBaseObject, daq::IBaseObject> dict;
    bool loaded = false;
    std::map<QTreeWidgetItem*, daq::BaseObjectPtr> itemToKeyMap;
};
