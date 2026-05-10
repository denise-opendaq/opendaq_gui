#include "property/base_property_item.h"
#include "widgets/property_object_view.h"
#include <QMenu>
#include <QMessageBox>
#include <QTreeWidget>

static QString CoreTypeToString(daq::CoreType coretype)
{
    switch (coretype)
    {
        case daq::CoreType::ctBool: return QStringLiteral("Bool");
        case daq::CoreType::ctInt: return QStringLiteral("Int");
        case daq::CoreType::ctFloat: return QStringLiteral("Float");
        case daq::CoreType::ctString: return QStringLiteral("String");
        case daq::CoreType::ctList: return QStringLiteral("List");
        case daq::CoreType::ctDict: return QStringLiteral("Dict");
        case daq::CoreType::ctObject: return QStringLiteral("Object");
        case daq::CoreType::ctStruct: return QStringLiteral("Struct");
        case daq::CoreType::ctEnumeration: return QStringLiteral("Enumeration");
        case daq::CoreType::ctFunc: return QStringLiteral("Function");
        case daq::CoreType::ctProc: return QStringLiteral("Procedure");
        default: return QString();
    }
}

BasePropertyItem::BasePropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : owner(owner)
    , prop(prop)
{
}

daq::PropertyPtr BasePropertyItem::getProperty() const
{
    return prop;
}

daq::PropertyObjectPtr BasePropertyItem::getOwner() const
{
    return owner;
}

daq::StringPtr BasePropertyItem::getName() const
{
    return prop.getName();
}

QString BasePropertyItem::showValue() const
{
    try
    {
        if (hasSelectionValues())
            return QString::fromStdString(owner.getPropertySelectionValue(getName()));

        const auto value = owner.getPropertyValue(prop.getName());
        return QString::fromStdString(value);
    }
    catch (...)
    {
        const auto value = owner.getPropertyValue(prop.getName());
        return QString::fromStdString(value);
    }
}

QString BasePropertyItem::showDisplayValue() const
{
    const QString valueStr = showValue();

    try
    {
        const auto unit = prop.getUnit();
        if (!unit.assigned())
            return valueStr;

        const auto symbol = unit.getSymbol();
        if (!symbol.assigned() || !symbol.getLength())
            return valueStr;

        const QString symbolStr = QString::fromStdString(symbol);

        return valueStr + QStringLiteral(" ") + symbolStr;
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error showing display value for property" << QString::fromStdString(getName()) << ":" << QString::fromStdString(e.what());
    }
    catch (...)
    {
        qWarning() << "Unknown error showing display value for property" << QString::fromStdString(getName());
    }
    return valueStr;
}

bool BasePropertyItem::isReadOnly() const
{
    if (prop.getReadOnly())
        return true;

    if (auto freezable = prop.asPtrOrNull<daq::IFreezable>(true); freezable.assigned() && freezable.isFrozen())
        return true;

    return false;
}

bool BasePropertyItem::isKeyEditable() const
{
    return false; // Keys are not editable by default
}

bool BasePropertyItem::isValueEditable() const
{
    return !isReadOnly() && !hasSelectionValues();
}

bool BasePropertyItem::hasSubtree() const
{
    return false;
}

bool BasePropertyItem::isExpanded() const
{
    return expanded;
}

void BasePropertyItem::setExpanded(bool expanded)
{
    this->expanded = expanded;
}

bool BasePropertyItem::isVisible() const
{
    return prop.getVisible();
}

const QStringList& BasePropertyItem::getAvailableMetadata()
{
    static QStringList list = 
    {
        QStringLiteral("Value type"),
        QStringLiteral("Key type"),
        QStringLiteral("Item type"),
        QStringLiteral("Description"),
        QStringLiteral("Unit"),
        QStringLiteral("Min value"),
        QStringLiteral("Max value"),
        QStringLiteral("Default value"),
        QStringLiteral("Suggested values"),
        QStringLiteral("Visible"),
        QStringLiteral("Selection values"),
        QStringLiteral("Referenced"),
        QStringLiteral("Validator"),
        QStringLiteral("Coercer"),
        QStringLiteral("Callable info"),
        QStringLiteral("Struct type"),
    };
    return list;
}

QString BasePropertyItem::getMetadataValue(const QString& key) const
{
    try
    {
        if (key == QStringLiteral("Value type"))
        {
            return CoreTypeToString(prop.getValueType());
        }
        if (key == QStringLiteral("Key type"))
        {
            return CoreTypeToString(prop.getKeyType());
        }
        if (key == QStringLiteral("Item type"))
        {
            return CoreTypeToString(prop.getItemType());
        }
        if (key == QStringLiteral("Description"))
        {
            const auto description = prop.getDescription();
            return description.assigned() ? QString::fromStdString(description) : QString();
        }
        if (key == QStringLiteral("Unit"))
        {
            const auto unit = prop.getUnit();
            return unit.assigned() ? QString::fromStdString(unit) : QString();
        }
        if (key == QStringLiteral("Min value"))
        {
            const auto min = prop.getMinValue();
            return min.assigned() ? QString::fromStdString(min) : QString();
        }
        if (key == QStringLiteral("Max value"))
        {
            const auto max = prop.getMaxValue();
            return max.assigned() ? QString::fromStdString(max) : QString();
        }
        if (key == QStringLiteral("Default value"))
        {
            const auto defaultValue = prop.getDefaultValue();
            return defaultValue.assigned() ? QString::fromStdString(defaultValue) : QString();
        }
        if (key == QStringLiteral("Suggested values"))
        {
            const auto suggestedValues = prop.getSuggestedValues();
            return suggestedValues.assigned() ? QString::fromStdString(suggestedValues) : QString();
        }
        if (key == QStringLiteral("Visible"))
        {
            return prop.getVisible() ? QStringLiteral("Yes") : QStringLiteral("No");
        }
        if (key == QStringLiteral("Selection values"))
        {
            const auto selectionValues = prop.getSelectionValues();
            if (const auto dict = selectionValues.asPtrOrNull<daq::IDict>(true); dict.assigned())
                return QString::fromStdString(dict.getValueList());
            return selectionValues.assigned() ? QString::fromStdString(selectionValues) : QString();
        }
        if (key == QStringLiteral("Referenced"))
        {
            return prop.getIsReferenced() ? QStringLiteral("Yes") : QStringLiteral("No");
        }
        if (key == QStringLiteral("Validator"))
        {
            const auto validator = prop.getValidator();
            return validator.assigned() ? QString::fromStdString(validator) : QString();
        }
        if (key == QStringLiteral("Coercer"))
        {
            const auto coercer = prop.getCoercer();
            return coercer.assigned() ? QString::fromStdString(coercer) : QString();
        }
        if (key == QStringLiteral("Callable info"))
        {
            const auto callableInfo = prop.getCallableInfo();
            return callableInfo.assigned() ? QString::fromStdString(callableInfo) : QString();
        }
        if (key == QStringLiteral("Struct type") && prop.getValueType() == daq::CoreType::ctStruct)
        {
            const auto structType = prop.getStructType();
            return structType.assigned() ? QString::fromStdString(structType) : QString();
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error getting metadata value for property" << QString::fromStdString(getName()) << "with key" << key << ":" << QString::fromStdString(e.what());
    }
    catch (...)
    {
        qWarning() << "Unknown error getting metadata value for property" << QString::fromStdString(getName()) << "with key" << key;
    }
    return QString();
}

void BasePropertyItem::setWidgetItem(QTreeWidgetItem* item)
{
    if (widgetItem && widgetItem != item)
    {
        // Remove from parent and delete old widget
        if (auto* parent = widgetItem->parent())
            parent->removeChild(widgetItem);
        else
        {
            // If it's a top-level item, we need to find the tree and remove it
            // But we don't have access to the tree here, so just delete it
            delete widgetItem;
        }
    }
    widgetItem = item;
}

void BasePropertyItem::refresh(PropertySubtreeBuilder& builder)
{
    if (!widgetItem)
        return;
    const QString nameStr = QString::fromStdString(getName());
    const QString rawValueStr = showValue();
    const QString displayValueStr = showDisplayValue();
    widgetItem->setText(0, nameStr);
    // Keep "raw" and "display" values separate so units never leak into edits/saves
    widgetItem->setData(1, Qt::UserRole, rawValueStr);
    widgetItem->setText(1, displayValueStr);
}

void BasePropertyItem::build_subtree(PropertySubtreeBuilder&, QTreeWidgetItem*, bool)
{
}

void BasePropertyItem::handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item)
{
    if (!view || !item)
        return;

    QStringList selectionValues = getSelectionValues();
    if (selectionValues.isEmpty())
        return;

    QString currentValue = showValue();

    QMenu menu(view);
    for (const QString& val : selectionValues)
    {
        QAction* action = menu.addAction(val);
        if (val == currentValue)
        {
            action->setCheckable(true);
            action->setChecked(true);
        }
    }

    QRect itemRect = view->visualItemRect(item);
    int valueColumnX = view->columnViewportPosition(1);
    QPoint pos = view->viewport()->mapToGlobal(QPoint(valueColumnX, itemRect.y() + itemRect.height()));

    QAction* selected = menu.exec(pos);
    if (selected)
    {
        try
        {
            setBySelectionValue(selected->text());
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
    }
}

void BasePropertyItem::handle_right_click(PropertyObjectView*, QTreeWidgetItem*, const QPoint&)
{
}

void BasePropertyItem::commitEdit(QTreeWidgetItem* item, int column)
{
    if (column == 1)
    {
        // Prefer raw value stored in UserRole (set by delegate), fallback to visible text
        const QString raw = item->data(1, Qt::UserRole).toString();
        const QString editedText = raw.isEmpty() ? item->text(1) : raw;
        const daq::StringPtr newText = editedText.toStdString();
        owner.setPropertyValue(getName(), newText.convertTo(prop.getValueType()));

        // Restore display value with unit immediately
        const QString rawValueStr = showValue();
        const QString displayValueStr = showDisplayValue();
        item->setData(1, Qt::UserRole, rawValueStr);
        item->setText(1, displayValueStr);
        item->setToolTip(1, displayValueStr);
    }
}

bool BasePropertyItem::hasSelectionValues() const
{
    return prop.getSelectionValues().assigned();
}

QStringList BasePropertyItem::getSelectionValues() const
{
    QStringList result;
    const auto selection = prop.getSelectionValues();

    if (!selection.assigned())
        return result;

    if (const auto list = selection.asPtrOrNull<daq::IList>(true); list.assigned())
    {
        for (const auto& item : list)
            result.append(QString::fromStdString(item));
    }
    else if (const auto dict = selection.asPtrOrNull<daq::IDict>(true); dict.assigned())
    {
        for (const auto& [key, value] : dict)
            result.append(QString::fromStdString(value));
    }

    return result;
}

void BasePropertyItem::setBySelectionValue(const QString& value)
{
    if (hasSelectionValues())
        owner.setPropertySelectionValue(getName(), value.toStdString());
}

