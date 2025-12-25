#include "property/enumeration_property_item.h"
#include "property/coretypes/enumeration_core_type_handler.h"
#include "widgets/property_object_view.h"
#include <QTreeWidget>
#include <QMessageBox>

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
    catch (...)
    {
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
        if (!prop.getReadOnly())
            return;

        const auto currentValue = owner.getPropertyValue(prop.getName());
        if (!currentValue.assigned())
            return;

        const auto enumType = currentValue.asPtr<daq::IEnumeration>().getEnumerationType();
        auto handler = std::make_shared<EnumerationCoreTypeHandler>(enumType);

        // Use handler to show combobox and handle selection, capture handler to keep it alive
        handler->handleDoubleClick(view, item, currentValue, [this, item, view, handler](const daq::BaseObjectPtr& newValue) {
            owner.setPropertyValue(getName(), newValue);
            Q_EMIT view->propertyChanged(QString::fromStdString(getName()), showValue());
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
    catch (...)
    {
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
    catch (...)
    {
        // Silently fail - error will be shown by the caller
        throw;
    }
}