#include "property/int_property_item.h"
#include "property/property_object_view.h"
#include <QComboBox>
#include <QTreeWidget>
#include <QMessageBox>

void IntPropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    if (!hasSelection())
        return;

    QStringList selectionValues = getSelectionValues();
    if (selectionValues.isEmpty())
        return;

    int currentIndex = getCurrentSelectionIndex();

    // Create and show combobox as a popup
    auto* combo = new QComboBox(view);
    combo->addItems(selectionValues);
    combo->setCurrentIndex(currentIndex);

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
            setBySelectionValue(selectedValue);
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