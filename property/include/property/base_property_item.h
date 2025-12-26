#pragma once

// Save Qt keywords and undefine them before including openDAQ
#ifdef signals
#pragma push_macro("signals")
#pragma push_macro("slots")
#pragma push_macro("emit")
#undef signals
#undef slots
#undef emit
#define QT_MACROS_PUSHED
#endif

#include <opendaq/opendaq.h>

#ifdef QT_MACROS_PUSHED
#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#undef QT_MACROS_PUSHED
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
    BasePropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop);
    virtual ~BasePropertyItem() = default;

    daq::StringPtr getName() const;

    // What to show in "Value" column
    virtual QString showValue() const;

    // Can user edit key (column 0)?
    virtual bool isKeyEditable() const;

    // Can user edit value (column 1)?
    virtual bool isValueEditable() const;

    // Is property truly read-only (cannot be edited at all, even via double-click)?
    virtual bool isReadOnly() const;

    // Subtree for nested PropertyObject
    virtual bool hasSubtree() const;

    virtual void build_subtree(PropertySubtreeBuilder&, QTreeWidgetItem*, bool force = false);

    // Events
    virtual void handle_double_click(PropertyObjectView*, QTreeWidgetItem*);

    virtual void handle_right_click(PropertyObjectView*, QTreeWidgetItem*, const QPoint&);

    // Commit edit from tree widget item
    // By default, reads text from column 1 and sets property value
    virtual void commitEdit(QTreeWidgetItem* item, int column);

protected:
    daq::PropertyObjectPtr owner;
    daq::PropertyPtr prop;
};
