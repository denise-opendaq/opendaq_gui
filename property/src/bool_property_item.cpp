#include "property/bool_property_item.h"
#include "widgets/property_object_view.h"
#include <QTreeWidget>
#include <QMessageBox>

BoolPropertyItem::BoolPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : BasePropertyItem(owner, prop)
{
}

QString BoolPropertyItem::showValue() const
{
    const auto value = owner.getPropertyValue(prop.getName());
    return value ? QStringLiteral("True") : QStringLiteral("False");
}

bool BoolPropertyItem::isValueEditable() const
{
    return false; // Not editable via text, use double-click instead
}

void BoolPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    try
    {
        toggleValue();
        item->setText(1, showValue());
        Q_EMIT view->propertyChanged(QString::fromStdString(getName()), showValue());
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

void BoolPropertyItem::commitEdit(QTreeWidgetItem*, int)
{
    // Not used, editing is done via double-click
}

void BoolPropertyItem::toggleValue()
{
    const auto currentValue = owner.getPropertyValue(prop.getName());
    owner.setPropertyValue(getName(), !static_cast<bool>(currentValue));
}