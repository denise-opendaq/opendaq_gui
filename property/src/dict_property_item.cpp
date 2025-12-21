#include "property/dict_property_item.h"
#include "property/property_object_view.h"
#include "property/default_core_type.h"
#include <QTreeWidgetItem>
#include <QMenu>
#include <QMessageBox>

// ============================================================================
// DictPropertyItem implementation
// ============================================================================

void DictPropertyItem::build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self)
{
    if (loaded || !dict.assigned())
        return;

    loaded = true;

    // remove dummy children
    while (self->childCount() > 0)
        delete self->takeChild(0);

    itemToKeyMap.clear();
    auto isEditable = isValueEditable();

    // Add each key-value pair as a child item
    QPalette palette = builder.view.palette();
    for (const auto& [key, value] : dict)
    {
        auto* treeChild = new QTreeWidgetItem();
        treeChild->setText(0, QString::fromStdString(key.toString()));
        treeChild->setText(1, QString::fromStdString(value.toString()));

        if (isEditable)
        {
            treeChild->setFlags(treeChild->flags() | Qt::ItemIsEditable);
        }
        else
        {
            // Set inactive color for non-editable items
            treeChild->setForeground(0, palette.color(QPalette::Disabled, QPalette::Text));
            treeChild->setForeground(1, palette.color(QPalette::Disabled, QPalette::Text));
        }

        self->addChild(treeChild);

        // Map tree item to original key
        itemToKeyMap[treeChild] = key;
    }
}

void DictPropertyItem::commitEdit(QTreeWidgetItem* item, int column)
{
    // Check if this is a child item (dict key-value pair)
    auto it = itemToKeyMap.find(item);
    if (it == itemToKeyMap.end())
        return; // Not a dict child item

    const daq::BaseObjectPtr& originalKey = it->second;

    // Refresh dict reference
    dict = owner.getPropertyValue(getName());
    if (!dict.assigned())
        return;

    daq::StringPtr newText = item->text(column).toStdString();

    if (column == 0)
    {
        // Edit key: remove old key, add new key with same value
        auto key = originalKey;
        auto value = dict.remove(key);

        key = newText.convertTo(prop.getKeyType());
        dict.set(key, value);

        // Update the map with new key
        itemToKeyMap[item] = key;
    }
    else if (column == 1)
    {
        const auto value = newText.convertTo(prop.getItemType());
        dict.set(originalKey, value);
    }

    // Save updated dict
    owner.setPropertyValue(getName(), dict);
}

void DictPropertyItem::handle_right_click(PropertyObjectView* view, QTreeWidgetItem* item, const QPoint& globalPos)
{
    if (!isValueEditable())
        return;

    QMenu menu(view);

    // Check if right-click is on a child item (dict entry) or on the dict itself
    auto it = itemToKeyMap.find(item);
    bool isChildItem = (it != itemToKeyMap.end());

    QAction* addAction = menu.addAction("Add item");
    QAction* removeAction = nullptr;

    if (isChildItem)
    {
        removeAction = menu.addAction("Remove item");
    }

    QAction* selectedAction = menu.exec(globalPos);

    if (selectedAction == addAction)
    {
        // Add new item with default key "new_key" and empty value
        try
        {
            dict.set(createDefaultValue(prop.getKeyType()), createDefaultValue(prop.getItemType()));

            // Save updated dict
            owner.setPropertyValue(getName(), dict);

            // Reload only this subtree
            QTreeWidgetItem* parentItem = isChildItem ? item->parent() : item;
            loaded = false;
            PropertySubtreeBuilder builder(*view);
            build_subtree(builder, parentItem);
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(view, "Error Adding Item",
                               QString("Failed to add item: %1").arg(e.what()));
        }
    }
    else if (selectedAction == removeAction && isChildItem)
    {
        // Remove item
        const daq::BaseObjectPtr& keyToRemove = it->second;

        try
        {
            dict.remove(keyToRemove);

            // Save updated dict
            owner.setPropertyValue(getName(), dict);

            // Reload only this subtree
            loaded = false;
            PropertySubtreeBuilder builder(*view);
            build_subtree(builder, item->parent());
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(view, "Error Removing Item",
                               QString("Failed to remove item: %1").arg(e.what()));
        }
    }
}