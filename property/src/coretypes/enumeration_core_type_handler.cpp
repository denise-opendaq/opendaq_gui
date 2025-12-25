#include "property/coretypes/enumeration_core_type_handler.h"
#include "context/AppContext.h"

EnumerationCoreTypeHandler::EnumerationCoreTypeHandler(const daq::EnumerationTypePtr& enumType)
    : enumType(enumType)
{
    typeManager = AppContext::instance()->daqInstance().getContext().getTypeManager();
}

QString EnumerationCoreTypeHandler::valueToString(const daq::BaseObjectPtr& value) const
{
    try
    {
        const auto enumValue = value.asPtr<daq::IEnumeration>();
        if (enumValue.assigned())
        {
            const auto valueName = enumValue.getValue();
            return QString::fromStdString(valueName);
        }
    }
    catch (...)
    {
        return QString("Error");
    }

    return QString("Unknown");
}

daq::BaseObjectPtr EnumerationCoreTypeHandler::stringToValue(const QString& str) const
{
    try
    {
        const auto enumName = enumType.getName();
        const auto valueStr = daq::String(str.toStdString());
        return daq::Enumeration(enumName, valueStr, typeManager);
    }
    catch (...)
    {
        throw;
    }
}

bool EnumerationCoreTypeHandler::hasSelection() const
{
    return true;  // Enumeration always has selection
}

QStringList EnumerationCoreTypeHandler::getSelectionValues() const
{
    QStringList result;

    try
    {
        if (enumType.assigned())
        {
            for (const auto& enumName : enumType.getEnumeratorNames())
                result.append(QString::fromStdString(enumName));
        }
    }
    catch (...)
    {
        // Return empty list on error
    }

    return result;
}

bool EnumerationCoreTypeHandler::isEditable() const
{
    return false;  // Not editable via text, use combobox
}

daq::CoreType EnumerationCoreTypeHandler::getCoreType() const
{
    return daq::CoreType::ctEnumeration;
}
