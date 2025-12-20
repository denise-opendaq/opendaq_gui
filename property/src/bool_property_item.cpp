#include "property/bool_property_item.h"
#include "property/property_object_view.h"
#include <QTreeWidget>
#include <QMessageBox>

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