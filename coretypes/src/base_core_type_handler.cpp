#include "coretypes/base_core_type_handler.h"
#include "widgets/property_object_view.h"
#include <QMenu>
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

    QString currentValueStr = valueToString(currentValue);

    QMenu menu(view);
    for (const QString& val : selectionValues)
    {
        QAction* action = menu.addAction(val);
        if (val == currentValueStr)
        {
            action->setCheckable(true);
            action->setChecked(true);
        }
    }

    // Position at the item's value column
    QRect itemRect = view->visualItemRect(item);
    int valueColumnX = view->columnViewportPosition(1);
    QPoint pos = view->viewport()->mapToGlobal(QPoint(valueColumnX, itemRect.y() + itemRect.height()));

    QAction* selected = menu.exec(pos);
    if (selected)
    {
        try
        {
            daq::BaseObjectPtr newValue = stringToValue(selected->text());
            setValue(newValue);
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(view,
                                 "Value Update Error",
                                 QString("Failed to update value: %1").arg(e.what()));
        }
        catch (...)
        {
            QMessageBox::warning(view,
                                 "Value Update Error",
                                 "Failed to update value: unknown error");
        }
    }

    return true;
}
