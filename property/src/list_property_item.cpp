#include "property/list_property_item.h"
#include "widgets/property_object_view.h"
#include "coretypes/default_core_type.h"
#include "coretypes/core_type_factory.h"
#include <QTreeWidgetItem>
#include <QMenu>
#include <QMessageBox>

// ============================================================================
// ListPropertyItem implementation
// ============================================================================

void ListPropertyItem::build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self, bool force)
{
    if (!force && (loaded || !list.assigned()))
        return;

    loaded = true;

    // remove dummy children
    while (self->childCount() > 0)
        delete self->takeChild(0);

    itemToKeyMap.clear();

    auto listEditable = isValueEditable();

    // Add each element as a child item
    QPalette palette = builder.view.palette();
    for (size_t i = 0; i < list.getCount(); i++)
    {
        auto* treeChild = new QTreeWidgetItem();
        const QString key = QStringLiteral("[%1]").arg(i);
        treeChild->setText(0, key);

        // Use handler to display value
        auto value = list[i];
        auto valueHandler = CoreTypeFactory::createHandler(value, prop.getItemType());
        treeChild->setText(1, valueHandler->valueToString(list[i]));

        if (listEditable)
        {
            // Set editable flag only if value handler is editable
            // Selection-based types (bool, enum) will use double-click instead
            if (valueHandler->isEditable())
                treeChild->setFlags(treeChild->flags() | Qt::ItemIsEditable);
        }
        else
        {
            // Set inactive color for non-editable items
            treeChild->setForeground(0, palette.color(QPalette::Disabled, QPalette::Text));
            treeChild->setForeground(1, palette.color(QPalette::Disabled, QPalette::Text));
        }

        self->addChild(treeChild);

        // Map tree item to index
        itemToKeyMap[treeChild] = i;
    }
}

void ListPropertyItem::commitEdit(QTreeWidgetItem* item, int column)
{
    // Check if this is a child item (list element)
    auto it = itemToKeyMap.find(item);
    if (it == itemToKeyMap.end())
        return; // Not a list child item

    const size_t index = it->second;

    // Refresh list reference
    list = owner.getPropertyValue(getName());
    if (!list.assigned())
        return;

    if (column == 1)
    {
        // Edit value
        QString newText = item->text(column);
        auto currentValue = list[index];
        auto valueHandler = CoreTypeFactory::createHandler(currentValue, prop.getItemType());

        daq::BaseObjectPtr newValue = valueHandler->stringToValue(newText);
        list.setItemAt(index, newValue);

        // Save updated list
        owner.setPropertyValue(getName(), list);
    }
}

void ListPropertyItem::handle_right_click(PropertyObjectView* view, QTreeWidgetItem* item, const QPoint& globalPos)
{
    if (isReadOnly())
        return;

    QMenu menu(view);

    // Check if right-click is on a child item (list element) or on the list itself
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
        // Add new item with empty value at the end
        try
        {
            list.pushBack(createDefaultValue(prop.getItemType()));

            // Save updated list
            owner.setPropertyValue(getName(), list);

            // Reload only this subtree
            QTreeWidgetItem* parentItem = isChildItem ? item->parent() : item;
            view->onPropertyValueChanged(owner);
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
        const size_t indexToRemove = it->second;

        try
        {
            list.removeAt(indexToRemove);

            // Save updated list
            owner.setPropertyValue(getName(), list);

            // Reload only this subtree
            view->onPropertyValueChanged(owner);
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(view, "Error Removing Item",
                               QString("Failed to remove item: %1").arg(e.what()));
        }
    }
}

void ListPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    // Check if this is a child item (list element)
    auto it = itemToKeyMap.find(item);
    if (it == itemToKeyMap.end())
        return; // Not a list child item

    const size_t index = it->second;

    // Refresh list reference
    list = owner.getPropertyValue(getName());
    if (!list.assigned())
        return;

    // Get the column that was double-clicked
    int column = view->currentColumn();

    if (column == 1)
    {
        // Double-click on value
        auto currentValue = list[index];
        auto valueHandler = CoreTypeFactory::createHandler(currentValue, prop.getItemType());

        if (valueHandler->hasSelection())
        {
            valueHandler->handleDoubleClick(view, item, currentValue, [this, index, valueHandler](const daq::BaseObjectPtr& newValue) {
                list.setItemAt(index, newValue);
                owner.setPropertyValue(getName(), list);
            });
        }
    }
}