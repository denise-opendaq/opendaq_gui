#include "property/struct_property_item.h"
#include "property/property_object_view.h"
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

    auto isEditable = isValueEditable();

    // Add each key-value pair as a child item
    for (const auto& [key, value] : structObj.getAsDictionary())
    {
        auto* treeChild = new QTreeWidgetItem();
        treeChild->setText(0, QString::fromStdString(key.toString()));
        treeChild->setText(1, QString::fromStdString(value.toString()));

        if (isEditable)
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
        const daq::StringPtr& originalKey = it->second;
        daq::StringPtr newText = item->text(column).toStdString();

        auto structBuilder = daq::StructBuilder(owner.getPropertyValue(getName()));
        auto value = structBuilder.get(originalKey);
        value = newText.convertTo(value.getCoreType());
        structObj = structBuilder.set(originalKey, value).build();

        // Save updated dict
        owner.setPropertyValue(getName(), structObj);
    }
    

}