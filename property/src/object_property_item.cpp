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

void ObjectPropertyItem::build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self, bool force)
{
    if (!force && (loaded || !nested.assigned()))
        return;

    loaded = true;

    // remove dummy children
    while (self->childCount() > 0)
        delete self->takeChild(0);

    // Register this nested PropertyObject with this ObjectPropertyItem
    builder.view.propertyObjectToLogic[nested] = this;

    builder.buildFromPropertyObject(self, nested);
}

void ObjectPropertyItem::commitEdit(QTreeWidgetItem*, int)
{
    // not editable
}
