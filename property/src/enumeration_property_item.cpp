#include "property/enumeration_property_item.h"
#include "widgets/property_object_view.h"
#include <QComboBox>
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
        const auto enumValue = owner.getPropertyValue(prop.getName()).asPtr<daq::IEnumeration>();
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

    return BasePropertyItem::showValue();
}

bool EnumerationPropertyItem::isValueEditable() const
{
    return false;  // Enumeration values are edited via double-click with combo box
}

void EnumerationPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    QStringList enumValues = getEnumerationValues();
    if (enumValues.isEmpty())
        return;

    // Create and show combobox as a popup
    auto* combo = new QComboBox(view);
    combo->addItems(enumValues);

    // Position combobox at the item's value column
    QRect itemRect = view->visualItemRect(item);
    int valueColumnX = view->columnViewportPosition(1);
    int valueColumnWidth = view->columnWidth(1);
    combo->setGeometry(valueColumnX, itemRect.y(), valueColumnWidth, itemRect.height());

    // Connect to handle selection
    QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), view, [this, combo, item, view](int index) {
        try
        {
            QString selectedValue = combo->itemText(index);
            setByEnumerationValue(selectedValue);
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
        combo->deleteLater();
    });

    // Delete combobox when focus is lost
    QObject::connect(combo, &QComboBox::destroyed, combo, &QComboBox::deleteLater);

    combo->show();
    combo->setFocus();
    combo->showPopup();
}

QStringList EnumerationPropertyItem::getEnumerationValues() const
{
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
    try
    {
        const auto enumValue = owner.getPropertyValue(prop.getName()).asPtr<daq::IEnumeration>(true);
        if (enumValue.assigned())
        {
            const auto enumType = enumValue.getEnumerationType();
            const auto enumName = enumType.getName();
            const auto valueStr = daq::String(value.toStdString());

            const auto typeManager = AppContext::instance()->daqInstance().getContext().getTypeManager();

            // Create new enumeration with the selected value
            const auto newEnumValue = daq::Enumeration(enumName, valueStr, typeManager);
            owner.setPropertyValue(getName(), newEnumValue);
        }
    }
    catch (...)
    {
        // Silently fail - error will be shown by the caller
        throw;
    }
}