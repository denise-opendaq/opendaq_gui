#pragma once

// Save Qt keywords and undefine them before including openDAQ
#ifdef signals
#pragma push_macro("signals")
#pragma push_macro("slots")
#pragma push_macro("emit")
#undef signals
#undef slots
#undef emit
#endif

#include <opendaq/opendaq.h>

#ifdef __cplusplus
#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#endif

#include <QtWidgets>

// Forward declarations
class PropertyObjectView;
class PropertySubtreeBuilder;

// ============================================================================
// BasePropertyItem — логика одного свойства (render/edit/subtree/events)
// ============================================================================

class BasePropertyItem
{
public:
    BasePropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
        : owner(owner)
        , prop(prop)
    {}

    virtual ~BasePropertyItem() = default;

    daq::StringPtr getName() const
    {
        return prop.getName();
    }

    // What to show in "Value" column
    virtual QString showValue() const
    {
        const auto value = owner.getPropertyValue(prop.getName());
        return QString::fromStdString(value);
    }

    // Can user edit key (column 0)?
    virtual bool isKeyEditable() const
    {
        return false; // Keys are not editable by default
    }

    // Can user edit value (column 1)?
    virtual bool isValueEditable() const
    {
        return !prop.getReadOnly();
    }

    // Legacy method - checks if column 1 is editable
    virtual bool isEditable() const
    {
        return isValueEditable();
    }

    // Subtree for nested PropertyObject
    virtual bool hasSubtree() const
    {
        return false;
    }

    virtual void build_subtree(PropertySubtreeBuilder&, QTreeWidgetItem*)
    {
    }

    // Events
    virtual void handle_double_click(PropertyObjectView*, QTreeWidgetItem*)
    {
    }

    virtual void handle_right_click(PropertyObjectView*, QTreeWidgetItem*, const QPoint&)
    {
    }

    // Commit edit from tree widget item
    // By default, reads text from column 1 and sets property value
    virtual void commitEdit(QTreeWidgetItem* item, int column)
    {
        if (column == 1)
        {
            const QString newText = item->text(1);
            owner.setPropertyValue(getName(), newText.toStdString());
        }
    }

protected:
    daq::PropertyObjectPtr owner;
    daq::PropertyPtr prop;
};
