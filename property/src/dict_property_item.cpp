#include "property/dict_property_item.h"
#include "property/property_object_view.h"
#include <QTreeWidgetItem>

// ============================================================================
// Helper functions to convert BaseObject to/from string
// ============================================================================

static daq::BaseObjectPtr stringToBaseObject(const QString& str, const daq::BaseObjectPtr& reference)
{
    if (!reference.assigned())
    {
        // Try to infer type from string
        bool ok;
        int intVal = str.toInt(&ok);
        if (ok)
            return daq::Integer(intVal);

        double doubleVal = str.toDouble(&ok);
        if (ok)
            return daq::Float(doubleVal);

        if (str == "true" || str == "false")
            return daq::Boolean(str == "true");

        return daq::String(str.toStdString());
    }

    try
    {
        if (reference.asPtrOrNull<daq::IInteger>(true).assigned())
        {
            bool ok;
            int val = str.toInt(&ok);
            if (!ok)
                throw std::runtime_error("Invalid integer value");
            return daq::Integer(val);
        }
        else if (reference.asPtrOrNull<daq::IString>(true).assigned())
        {
            return daq::String(str.toStdString());
        }
        else if (reference.asPtrOrNull<daq::IBoolean>(true).assigned())
        {
            if (str != "true" && str != "false")
                throw std::runtime_error("Invalid boolean value (must be 'true' or 'false')");
            return daq::Boolean(str == "true");
        }
        else if (reference.asPtrOrNull<daq::IFloat>(true).assigned())
        {
            bool ok;
            double val = str.toDouble(&ok);
            if (!ok)
                throw std::runtime_error("Invalid float value");
            return daq::Float(val);
        }
    }
    catch (...)
    {
        throw;
    }

    return daq::String(str.toStdString());
}

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

    // Add each key-value pair as a child item
    for (const auto& [key, value] : dict)
    {
        auto* treeChild = new QTreeWidgetItem();
        treeChild->setText(0, QString::fromStdString(key.toString()));
        treeChild->setText(1, QString::fromStdString(value.toString()));

        // Make both columns editable
        treeChild->setFlags(treeChild->flags() | Qt::ItemIsEditable);

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

    if (column == 1)
    {
        auto value = dict.get(originalKey);
        value = newText.convertTo(value.getCoreType());
        dict.set(originalKey, value);
    }
    else if (column == 0)
    {
        // Edit key: remove old key, add new key with same value
        auto key = originalKey;
        auto value = dict.remove(key);
        key = newText.convertTo(key.getCoreType());

        dict.set(key, value);

        // Update the map with new key
        itemToKeyMap[item] = key;
    }

    // Save updated dict
    owner.setPropertyValue(getName(), dict);
}