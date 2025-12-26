#include "property/int_property_item.h"
#include "widgets/property_object_view.h"
#include <QComboBox>
#include <QTreeWidget>
#include <QMessageBox>

IntPropertyItem::IntPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : BasePropertyItem(owner, prop)
{
}

QString IntPropertyItem::showValue() const
{
    const auto selection = prop.getSelectionValues();
    if (selection.assigned())
    {
        if (const auto list = selection.asPtrOrNull<daq::IList>(true); list.assigned())
        {
            const auto index = owner.getPropertyValue(prop.getName());
            if (static_cast<size_t>(index) < list.getCount())
                return QString::fromStdString(list.getItemAt(index));
            else
                return QString("Wrong value");
        }
        else if (const auto dict = selection.asPtrOrNull<daq::IDict>(true); dict.assigned())
        {
            const auto key = owner.getPropertyValue(prop.getName());
            if (dict.hasKey(key))
                return QString::fromStdString(dict[key]);
            else
                return QString("Wrong value");
        }
    }

    return BasePropertyItem::showValue();
}

bool IntPropertyItem::isValueEditable() const
{
    return !hasSelection();
}

bool IntPropertyItem::hasSelection() const
{
    const auto selection = prop.getSelectionValues();
    return selection.assigned();
}

void IntPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    QStringList selectionValues = getSelectionValues();
    if (selectionValues.isEmpty())
        return;

    // Create and show combobox as a popup
    auto* combo = new QComboBox(view);
    combo->addItems(selectionValues);

    // Set current value as selected
    QString currentValue = showValue();
    int currentIndex = selectionValues.indexOf(currentValue);
    if (currentIndex >= 0)
        combo->setCurrentIndex(currentIndex);

    // Position combobox at the item's value column, below the current item
    QRect itemRect = view->visualItemRect(item);
    int valueColumnX = view->columnViewportPosition(1);
    int valueColumnWidth = view->columnWidth(1);
    combo->setGeometry(valueColumnX, itemRect.y(), valueColumnWidth, itemRect.height());

    // Connect to handle selection
    QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), view, [this, combo, view](int index) {
        try
        {
            QString selectedValue = combo->itemText(index);
            setBySelectionValue(selectedValue);
            // Trigger UI update (will be handled by componentCoreEventCallback if owner is set)
            view->onPropertyValueChanged(owner);
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

QStringList IntPropertyItem::getSelectionValues() const
{
    QStringList result;
    const auto selection = prop.getSelectionValues();

    if (selection.assigned())
    {
        if (const auto list = selection.asPtrOrNull<daq::IList>(true); list.assigned())
        {
            for (size_t i = 0; i < list.getCount(); ++i)
                result.append(QString::fromStdString(list.getItemAt(i)));
        }
        else if (const auto dict = selection.asPtrOrNull<daq::IDict>(true); dict.assigned())
        {
            for (const auto& [key, value] : dict)
                result.append(QString::fromStdString(value));
        }
    }

    return result;
}

void IntPropertyItem::setBySelectionValue(const QString& value)
{
    const auto selection = prop.getSelectionValues();
    if (!selection.assigned())
        return;

    const std::string valueStr = value.toStdString();

    if (const auto list = selection.asPtrOrNull<daq::IList>(true); list.assigned())
    {
        // For list, find the index of the value
        for (size_t i = 0; i < list.getCount(); ++i)
        {
            if (list[i] == valueStr)
            {
                owner.setPropertyValue(getName(), static_cast<int>(i));
                return;
            }
        }
    }
    else if (const auto dict = selection.asPtrOrNull<daq::IDict>(true); dict.assigned())
    {
        // For dict, find the key by value
        for (const auto& [key, val] : dict)
        {
            if (val == valueStr)
            {
                owner.setPropertyValue(getName(), key);
                return;
            }
        }
    }
}