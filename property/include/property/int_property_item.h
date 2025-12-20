#pragma once

#include "base_property_item.h"

// Forward declarations
class PropertyObjectView;
class QTreeWidgetItem;
class QPoint;

class IntPropertyItem final : public BasePropertyItem
{
public:
    IntPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop)
        : BasePropertyItem(owner, prop)
    {}

    QString showValue() const override
    {
        // 
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

    bool isValueEditable() const override
    {
        return !hasSelection();
    }

    bool hasSelection() const
    {
        const auto selection = prop.getSelectionValues();
        return selection.assigned();
    }

    void handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item) override;

    QStringList getSelectionValues() const
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

    int getCurrentSelectionIndex() const
    {
        const auto selection = prop.getSelectionValues();
        if (!selection.assigned())
            return -1;

        const auto currentValue = owner.getPropertyValue(prop.getName());

        if (const auto list = selection.asPtrOrNull<daq::IList>(true); list.assigned())
        {
            return static_cast<int>(currentValue);
        }
        else if (const auto dict = selection.asPtrOrNull<daq::IDict>(true); dict.assigned())
        {
            int index = 0;
            for (const auto& [key, value] : dict)
            {
                if (key == currentValue)
                    return index;
                ++index;
            }
        }

        return -1;
    }

    void setBySelectionValue(const QString& value)
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
                if (list.getItemAt(i) == valueStr)
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
};
