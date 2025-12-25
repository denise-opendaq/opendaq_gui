#include "property/dict_property_item.h"
#include "widgets/property_object_view.h"
#include "property/default_core_type.h"
#include "property/coretypes/core_type_factory.h"
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
    auto dictEditable = isValueEditable();

    // Add each key-value pair as a child item
    QPalette palette = builder.view.palette();
    for (const auto& [key, value] : dict)
    {
        auto* treeChild = new QTreeWidgetItem();

        auto keyHandler = CoreTypeFactory::createHandler(key, prop.getKeyType());
        auto valueHandler = CoreTypeFactory::createHandler(value, prop.getItemType());

        // Use handlers to display key and value
        treeChild->setText(0, keyHandler->valueToString(key));
        treeChild->setText(1, valueHandler->valueToString(value));

        if (dictEditable)
        {
            // Set editable flag if at least one of key or value is editable
            // Note: Qt doesn't support per-column flags, so we check in commitEdit
            if (keyHandler->isEditable() || valueHandler->isEditable())
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

    QString newText = item->text(column);

    if (column == 0)
    {
        // Edit key: check if key is editable
        auto keyHandler = CoreTypeFactory::createHandler(originalKey, prop.getKeyType());
        if (!keyHandler || !keyHandler->isEditable())
            return; // Key is not editable (e.g., bool, enum)

        // Remove old key, add new key with same value
        auto value = dict.remove(originalKey);
        daq::BaseObjectPtr newKey = keyHandler->stringToValue(newText);
        dict.set(newKey, value);

        // Update the map with new key
        itemToKeyMap[item] = newKey;
    }
    else if (column == 1)
    {
        // Edit value: check if value is editable
        auto currentValue = dict.get(originalKey);
        auto valueHandler = CoreTypeFactory::createHandler(currentValue, prop.getItemType());
        if (!valueHandler || !valueHandler->isEditable())
            return; // Value is not editable (e.g., bool, enum)

        daq::BaseObjectPtr newValue = valueHandler->stringToValue(newText);
        dict.set(originalKey, newValue);
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

void DictPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    // Check if this is a child item (dict element)
    auto it = itemToKeyMap.find(item);
    if (it == itemToKeyMap.end())
        return; // Not a dict child item

    if (!isValueEditable())
        return;

    const daq::BaseObjectPtr& key = it->second;

    // Refresh dict reference
    dict = owner.getPropertyValue(getName());
    if (!dict.assigned())
        return;

    // Get the column that was double-clicked
    int column = view->currentColumn();

    if (column == 0)
    {
        // Double-click on key
        auto keyHandler = CoreTypeFactory::createHandler(key, prop.getKeyType());
        if (keyHandler && keyHandler->hasSelection())
        {
            auto value = dict.get(key);
            dict.remove(key);

            keyHandler->handleDoubleClick(view, item, key, [this, value, item, keyHandler](const daq::BaseObjectPtr& newKey) {
                dict.set(newKey, value);
                owner.setPropertyValue(getName(), dict);

                // Update the map with new key
                itemToKeyMap[item] = newKey;
            });
        }
    }
    else if (column == 1)
    {
        // Double-click on value
        auto currentValue = dict.get(key);
        auto valueHandler = CoreTypeFactory::createHandler(currentValue, prop.getItemType());

        if (valueHandler && valueHandler->hasSelection())
        {
            valueHandler->handleDoubleClick(view, item, currentValue, [this, key, valueHandler](const daq::BaseObjectPtr& newValue) {
                dict.set(key, newValue);
                owner.setPropertyValue(getName(), dict);
            });
        }
    }
}