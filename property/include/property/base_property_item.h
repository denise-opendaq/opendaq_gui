#pragma once

#include <QtWidgets>

#include <coreobjects/property_object_ptr.h>

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

    daq::PropertyPtr getProperty() const;
    daq::PropertyObjectPtr getOwner() const;

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

    virtual bool isExpanded() const;
    virtual void setExpanded(bool expanded);

    // Check if property is visible
    virtual bool isVisible() const;

    // Refresh property value (update internal state if needed)
    virtual void refresh(PropertySubtreeBuilder&);

    virtual void build_subtree(PropertySubtreeBuilder&, QTreeWidgetItem*, bool force = false);

    // Events
    virtual void handle_double_click(PropertyObjectView*, QTreeWidgetItem*);

    virtual void handle_right_click(PropertyObjectView*, QTreeWidgetItem*, const QPoint&);

    // Commit edit from tree widget item
    // By default, reads text from column 1 and sets property value
    virtual void commitEdit(QTreeWidgetItem* item, int column);

    // Widget management
    QTreeWidgetItem* getWidgetItem() const { return widgetItem.get(); }
    void setWidgetItem(QTreeWidgetItem* item);

protected:
    daq::PropertyObjectPtr owner;
    daq::PropertyPtr prop;
    bool expanded = false;
    std::unique_ptr<QTreeWidgetItem> widgetItem;
};
