#include "property/object_property_item.h"
#include "widgets/property_object_view.h"

// ============================================================================
// ObjectPropertyItem subtree implementation
// ============================================================================

void ObjectPropertyItem::build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self)
{
    if (loaded || !nested.assigned())
        return;

    loaded = true;

    // remove dummy children
    while (self->childCount() > 0)
        delete self->takeChild(0);

    // Register this nested PropertyObject with this ObjectPropertyItem
    builder.view.propertyObjectToLogic[nested] = this;

    builder.buildFromPropertyObject(self, nested);
}
