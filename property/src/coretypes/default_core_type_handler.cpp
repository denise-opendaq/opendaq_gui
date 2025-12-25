#include "property/coretypes/default_core_type_handler.h"
#include "property/default_core_type.h"

DefaultCoreTypeHandler::DefaultCoreTypeHandler(daq::CoreType coreType)
    : coreType(coreType)
{
}

QString DefaultCoreTypeHandler::valueToString(const daq::BaseObjectPtr& value) const
{
    if (!value.assigned())
        return QString("");

    try
    {
        return QString::fromStdString(value.toString());
    }
    catch (...)
    {
        return QString("Error");
    }
}

daq::BaseObjectPtr DefaultCoreTypeHandler::stringToValue(const QString& str) const
{
    try
    {
        daq::StringPtr strValue = str.toStdString();
        return strValue.convertTo(coreType);
    }
    catch (...)
    {
        // If conversion fails, try to create default value
        return createDefaultValue(coreType);
    }
}

bool DefaultCoreTypeHandler::hasSelection() const
{
    return false;  // Default types don't have selection
}

QStringList DefaultCoreTypeHandler::getSelectionValues() const
{
    return QStringList();  // No selection values
}

bool DefaultCoreTypeHandler::isEditable() const
{
    return true;  // Editable via text input
}

daq::CoreType DefaultCoreTypeHandler::getCoreType() const
{
    return coreType;
}
