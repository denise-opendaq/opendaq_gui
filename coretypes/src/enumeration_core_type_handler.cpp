#include "coretypes/enumeration_core_type_handler.h"
#include "context/AppContext.h"
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>
#include <opendaq/instance_ptr.h>

EnumerationCoreTypeHandler::EnumerationCoreTypeHandler(const daq::EnumerationTypePtr& enumType)
    : enumType(enumType)
{
    typeManager = AppContext::Instance()->daqInstance().getContext().getTypeManager();
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
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Error converting enumeration to string: {}", e.what());
        return QString("Error");
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Unknown error converting enumeration to string");
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
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Error getting enumeration selection values: {}", e.what());
        // Return empty list on error
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Unknown error getting enumeration selection values");
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

