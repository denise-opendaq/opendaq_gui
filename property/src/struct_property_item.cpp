#include "property/struct_property_item.h"
#include "widgets/property_object_view.h"
#include "property/coretypes/core_type_factory.h"
#include <QTreeWidgetItem>

// ============================================================================
// StructPropertyItem implementation
// ============================================================================

void StructPropertyItem::build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self)
{
    if (loaded || !structObj.assigned())
        return;

    loaded = true;

    // remove dummy children
    while (self->childCount() > 0)
        delete self->takeChild(0);

    itemToKeyMap.clear();

    auto structEditable = isValueEditable();

    // Add each field as a child item
    for (const auto& [key, value] : structObj.getAsDictionary())
    {
        auto* treeChild = new QTreeWidgetItem();
        treeChild->setText(0, QString::fromStdString(key.toString()));

        // Use handler to display value
        auto valueHandler = CoreTypeFactory::createHandler(value, value.getCoreType());
        treeChild->setText(1, valueHandler->valueToString(value));

        // Set editable flag only if struct is editable AND field handler is editable
        if (structEditable && valueHandler->isEditable())
            treeChild->setFlags(treeChild->flags() | Qt::ItemIsEditable);

        self->addChild(treeChild);

        // Map tree item to original key
        itemToKeyMap[treeChild] = key;
    }
}

void StructPropertyItem::commitEdit(QTreeWidgetItem* item, int column)
{
    // Check if this is a child item (dict key-value pair)
    auto it = itemToKeyMap.find(item);
    if (it == itemToKeyMap.end())
        return; // Not a dict child item

    if (column == 1)
    {
        const daq::StringPtr& fieldKey = it->second;
        QString newText = item->text(column);

        auto structBuilder = daq::StructBuilder(owner.getPropertyValue(getName()));
        auto currentValue = structBuilder.get(fieldKey);

        // Use handler to convert value
        auto valueHandler = CoreTypeFactory::createHandler(currentValue, currentValue.getCoreType());
        daq::BaseObjectPtr newValue = valueHandler->stringToValue(newText);

        structObj = structBuilder.set(fieldKey, newValue).build();

        // Save updated struct
        owner.setPropertyValue(getName(), structObj);
    }
}

void StructPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    // Check if this is a child item (struct field)
    auto it = itemToKeyMap.find(item);
    if (it == itemToKeyMap.end())
        return; // Not a struct field item

    if (!isValueEditable())
        return;

    const daq::StringPtr& fieldKey = it->second;

    // Refresh struct reference
    structObj = owner.getPropertyValue(getName());
    if (!structObj.assigned())
        return;

    // Get the column that was double-clicked
    int column = view->currentColumn();

    if (column == 1)
    {
        // Double-click on value
        auto currentValue = structObj.get(fieldKey);
        auto valueHandler = CoreTypeFactory::createHandler(currentValue, currentValue.getCoreType());

        if (valueHandler && valueHandler->hasSelection())
        {
            // Capture handler as shared_ptr to keep it alive during async callback
            valueHandler->handleDoubleClick(view, item, currentValue, [this, fieldKey, valueHandler](const daq::BaseObjectPtr& newValue) {
                auto structBuilder = daq::StructBuilder(structObj);
                structObj = structBuilder.set(fieldKey, newValue).build();
                owner.setPropertyValue(getName(), structObj);
            });
        }
    }
}