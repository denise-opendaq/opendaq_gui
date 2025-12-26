#include "property/base_property_item.h"

BasePropertyItem::BasePropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
    : owner(owner)
    , prop(prop)
{
}

daq::StringPtr BasePropertyItem::getName() const
{
    return prop.getName();
}

QString BasePropertyItem::showValue() const
{
    const auto value = owner.getPropertyValue(prop.getName());
    return QString::fromStdString(value);
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
    return !isReadOnly();
}

bool BasePropertyItem::hasSubtree() const
{
    return false;
}

void BasePropertyItem::build_subtree(PropertySubtreeBuilder&, QTreeWidgetItem*, bool)
{
}

void BasePropertyItem::handle_double_click(PropertyObjectView*, QTreeWidgetItem*)
{
}

void BasePropertyItem::handle_right_click(PropertyObjectView*, QTreeWidgetItem*, const QPoint&)
{
}

void BasePropertyItem::commitEdit(QTreeWidgetItem* item, int column)
{
    if (column == 1)
    {
        const daq::StringPtr newText = item->text(1).toStdString();
        owner.setPropertyValue(getName(), newText.convertTo(prop.getValueType()));
    }
}

