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
