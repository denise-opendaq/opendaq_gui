#include "property/enumeration_property_item.h"
#include "coretypes/enumeration_core_type_handler.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include <QTreeWidget>
#include <QMessageBox>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

EnumerationPropertyItem::EnumerationPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : BasePropertyItem(owner, prop)
{
}

QString EnumerationPropertyItem::showValue() const
{
    try
    {
        const auto enumValue = owner.getPropertyValue(prop.getName());
        if (enumValue.assigned())
        {
            const auto enumType = enumValue.asPtr<daq::IEnumeration>().getEnumerationType();
            EnumerationCoreTypeHandler handler(enumType);
            return handler.valueToString(enumValue);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_D("Error converting enumeration to string: {}", e.what());
        return QString("Error");
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_D("Unknown error converting enumeration to string");
        return QString("Error");
    }

    return BasePropertyItem::showValue();
}

bool EnumerationPropertyItem::isValueEditable() const
{
    return false;  // Enumeration values are edited via double-click with combo box
}

void EnumerationPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    try
    {
        const auto currentValue = owner.getPropertyValue(prop.getName());
        if (!currentValue.assigned())
            return;

        const auto enumType = currentValue.asPtr<daq::IEnumeration>().getEnumerationType();
        auto handler = std::make_shared<EnumerationCoreTypeHandler>(enumType);

        // Use handler to show combobox and handle selection, capture handler to keep it alive
        handler->handleDoubleClick(view, item, currentValue, [this, view, handler](const daq::BaseObjectPtr& newValue) 
        {
            owner.setPropertyValue(getName(), newValue);
            view->onPropertyValueChanged(owner, true);
        });
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(view,
                             "Property Update Error",
                             QString("Failed to update property: %1").arg(e.what()));
    }
    catch (...)
    {
        QMessageBox::warning(view,
                             "Property Update Error",
                             "Failed to update property: unknown error");
    }
}

QStringList EnumerationPropertyItem::getEnumerationValues() const
{
    // This method can be removed if not used elsewhere
    QStringList result;

    try
    {
        const auto enumValue = owner.getPropertyValue(prop.getName()).asPtr<daq::IEnumeration>(true);
        if (enumValue.assigned())
        {
            const auto enumType = enumValue.getEnumerationType();
            for (const auto & enumName : enumType.getEnumeratorNames())
                result.append(QString::fromStdString(enumName));
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_D("Error getting enumeration values: {}", e.what());
        // Return empty list on error
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_D("Unknown error getting enumeration values");
        // Return empty list on error
    }

    return result;
}

void EnumerationPropertyItem::setByEnumerationValue(const QString& value)
{
    // This method can be removed if not used elsewhere
    try
    {
        const auto enumValue = owner.getPropertyValue(prop.getName()).asPtr<daq::IEnumeration>(true);
        if (enumValue.assigned())
        {
            const auto enumType = enumValue.getEnumerationType();
            EnumerationCoreTypeHandler handler(enumType);
            const auto newEnumValue = handler.stringToValue(value);
            owner.setPropertyValue(getName(), newEnumValue);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_D("Error setting enumeration value: {}", e.what());
        // Re-throw - error will be shown by the caller
        throw;
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_D("Unknown error setting enumeration value");
        // Re-throw - error will be shown by the caller
        throw;
    }
}