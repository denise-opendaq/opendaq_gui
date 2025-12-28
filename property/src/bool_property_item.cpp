#include "property/bool_property_item.h"
#include "coretypes/bool_core_type_handler.h"
#include "widgets/property_object_view.h"
#include <QTreeWidget>
#include <QMessageBox>

BoolPropertyItem::BoolPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : BasePropertyItem(owner, prop)
{
}

QString BoolPropertyItem::showValue() const
{
    BoolCoreTypeHandler handler;
    const auto value = owner.getPropertyValue(prop.getName());
    return handler.valueToString(value);
}

bool BoolPropertyItem::isValueEditable() const
{
    return false; // Not editable via text, use double-click to toggle
}

void BoolPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    try
    {
        BoolCoreTypeHandler handler;
        const auto currentValue = owner.getPropertyValue(prop.getName());

        // Use handler to toggle the value
        handler.handleDoubleClick(view, item, currentValue, [this, view](const daq::BaseObjectPtr& newValue) {
            owner.setPropertyValue(getName(), newValue);
            // Trigger UI update (will be handled by componentCoreEventCallback if owner is set)
            view->onPropertyValueChanged(owner);
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