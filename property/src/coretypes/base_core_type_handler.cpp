#include "property/coretypes/base_core_type_handler.h"
#include "widgets/property_object_view.h"
#include <QComboBox>
#include <QTreeWidget>
#include <QMessageBox>

bool BaseCoreTypeHandler::handleDoubleClick(PropertyObjectView* view,
                                            QTreeWidgetItem* item,
                                            const daq::BaseObjectPtr& currentValue,
                                            std::function<void(const daq::BaseObjectPtr&)> setValue)
{
    if (!hasSelection())
        return false;

    QStringList selectionValues = getSelectionValues();
    if (selectionValues.isEmpty())
        return false;

    // Create and show combobox as a popup
    auto* combo = new QComboBox(view);
    combo->addItems(selectionValues);

    // Set current value as selected
    QString currentValueStr = valueToString(currentValue);
    int currentIndex = selectionValues.indexOf(currentValueStr);
    if (currentIndex >= 0)
        combo->setCurrentIndex(currentIndex);

    // Position combobox at the item's value column, below the current item
    QRect itemRect = view->visualItemRect(item);
    int valueColumnX = view->columnViewportPosition(1);
    int valueColumnWidth = view->columnWidth(1);
    combo->setGeometry(valueColumnX, itemRect.y() + itemRect.height(), valueColumnWidth, itemRect.height());

    // Connect to handle selection
    QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), view, [this, combo, setValue](int index) {
        try
        {
            QString selectedValue = combo->itemText(index);
            daq::BaseObjectPtr newValue = stringToValue(selectedValue);
            setValue(newValue);
            // Note: Display will be updated automatically via componentCoreEventCallback
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(combo->parentWidget(),
                                 "Value Update Error",
                                 QString("Failed to update value: %1").arg(e.what()));
        }
        catch (...)
        {
            QMessageBox::warning(combo->parentWidget(),
                                 "Value Update Error",
                                 "Failed to update value: unknown error");
        }
        combo->deleteLater();
    });

    // Delete combobox when focus is lost
    QObject::connect(combo, &QComboBox::destroyed, combo, &QComboBox::deleteLater);

    combo->show();
    combo->setFocus();
    combo->showPopup();

    return true;
}
