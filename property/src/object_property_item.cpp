#include "property/object_property_item.h"
#include "widgets/property_object_view.h"

// ============================================================================
// ObjectPropertyItem implementation
// ============================================================================

ObjectPropertyItem::ObjectPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : BasePropertyItem(owner, prop)
    , nested(prop.getValue())
{
}

QString ObjectPropertyItem::showValue() const
{
    return QStringLiteral("PropertyObject");
}

bool ObjectPropertyItem::isValueEditable() const
{
    return false;
}

bool ObjectPropertyItem::hasSubtree() const
{
    return true;
}

void ObjectPropertyItem::build_subtree(PropertyObjectView& view, QTreeWidgetItem* self, bool force)
{
    if (!force && (expanded || !nested.assigned()))
        return;

    expanded = true;

    view.propertyObjectToLogic.insert_or_assign(nested, this);
    view.buildFromPropertyObject(self, nested);
}

void ObjectPropertyItem::commitEdit(QTreeWidgetItem*, int)
{
    // not editable
}

void ObjectPropertyItem::refresh(PropertyObjectView& view)
{
    BasePropertyItem::refresh(view);
    if (widgetItem)
        build_subtree(view, widgetItem, true);
}

