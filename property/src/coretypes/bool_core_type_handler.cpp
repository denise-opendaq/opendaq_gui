#include "property/coretypes/bool_core_type_handler.h"
#include "widgets/property_object_view.h"
#include <QTreeWidget>

QString BoolCoreTypeHandler::valueToString(const daq::BaseObjectPtr& value) const
{
    if (!value.assigned())
        return QString("False");

    bool boolValue = value;
    return boolValue ? QString("True") : QString("False");
}

daq::BaseObjectPtr BoolCoreTypeHandler::stringToValue(const QString& str) const
{
    bool value = (str == "True" || str == "true" || str == "1");
    return daq::Boolean(value);
}

bool BoolCoreTypeHandler::hasSelection() const
{
    return true;  // Bool has selection: True/False
}

QStringList BoolCoreTypeHandler::getSelectionValues() const
{
    return QStringList{"True", "False"};
}

bool BoolCoreTypeHandler::isEditable() const
{
    return false;  // Not editable via text, use double-click to toggle
}

daq::CoreType BoolCoreTypeHandler::getCoreType() const
{
    return daq::CoreType::ctBool;
}

bool BoolCoreTypeHandler::handleDoubleClick(PropertyObjectView* view,
                                            QTreeWidgetItem* item,
                                            const daq::BaseObjectPtr& currentValue,
                                            std::function<void(const daq::BaseObjectPtr&)> setValue)
{
    // Toggle boolean value
    bool currentBool = currentValue.assigned() ? static_cast<bool>(currentValue) : false;
    bool newBool = !currentBool;

    daq::BaseObjectPtr newValue = daq::Boolean(newBool);
    setValue(newValue);

    // Update display
    item->setText(1, valueToString(newValue));

    return true;  // Handled
}
