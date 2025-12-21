#include "property/list_property_item.h"
#include "widgets/property_object_view.h"
#include "property/default_core_type.h"
#include <QTreeWidgetItem>
#include <QMenu>
#include <QMessageBox>

// ============================================================================
// ListPropertyItem implementation
// ============================================================================

void ListPropertyItem::build_subtree(PropertySubtreeBuilder& builder, QTreeWidgetItem* self)
{
    if (loaded || !list.assigned())
        return;

    loaded = true;

    // remove dummy children
    while (self->childCount() > 0)
        delete self->takeChild(0);

    itemToKeyMap.clear();

    auto isEditable = isValueEditable();

    // Add each key-value pair as a child item
    QPalette palette = builder.view.palette();
    for (size_t i = 0; i < list.getCount(); i++)
    {
        auto* treeChild = new QTreeWidgetItem();
        const QString key = QStringLiteral("[%1]").arg(i);
        treeChild->setText(0, key);
        treeChild->setText(1, QString::fromStdString(list[i].toString()));

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
        itemToKeyMap[treeChild] = i;
    }
}

void ListPropertyItem::commitEdit(QTreeWidgetItem* item, int column)
{
    // Check if this is a child item (dict key-value pair)
    auto it = itemToKeyMap.find(item);
    if (it == itemToKeyMap.end())
        return; // Not a dict child item

    const size_t index = it->second;

    // Refresh dict reference
    list = owner.getPropertyValue(getName());
    if (!list.assigned())
        return;

    if (column == 1)
    {
        daq::StringPtr newText = item->text(column).toStdString();
        const auto value = newText.convertTo(prop.getItemType());
        list.setItemAt(index, value);

        // Save updated list
        owner.setPropertyValue(getName(), list);
    }
}

void ListPropertyItem::handle_right_click(PropertyObjectView* view, QTreeWidgetItem* item, const QPoint& globalPos)
{
    if (!isValueEditable())
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
        const size_t indexToRemove = it->second;

        try
        {
            list.removeAt(indexToRemove);

            // Save updated list
            owner.setPropertyValue(getName(), list);

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