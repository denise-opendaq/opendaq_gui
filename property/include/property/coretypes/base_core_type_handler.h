#pragma once


#include <opendaq/opendaq.h>


#include <QtWidgets>

// Forward declarations
class PropertyObjectView;

/**
 * Base class for handling core type display and editing
 * Handles both editable (text input) and selectable (combobox) types
 */
class BaseCoreTypeHandler
{
public:
    virtual ~BaseCoreTypeHandler() = default;

    // Convert value to display string
    virtual QString valueToString(const daq::BaseObjectPtr& value) const = 0;

    // Convert string to value
    virtual daq::BaseObjectPtr stringToValue(const QString& str) const = 0;

    // Check if this type supports selection (combobox)
    virtual bool hasSelection() const = 0;

    // Get list of selectable values (for combobox)
    virtual QStringList getSelectionValues() const = 0;

    // Check if this type is editable via text input
    virtual bool isEditable() const = 0;

    // Handle double-click for selection-based types
    // Returns true if handled, false if should use default behavior
    virtual bool handleDoubleClick(PropertyObjectView* view,
                                   QTreeWidgetItem* item,
                                   const daq::BaseObjectPtr& currentValue,
                                   std::function<void(const daq::BaseObjectPtr&)> setValue);

    // Get core type this handler supports
    virtual daq::CoreType getCoreType() const = 0;
};
