#pragma once

#include "base_property_item.h"

// Forward declarations
class PropertyObjectView;
class QTreeWidgetItem;
class QPoint;

class IntPropertyItem final : public BasePropertyItem
{
public:
    IntPropertyItem(const daq::PropertyObjectPtr& owner, const daq::PropertyPtr& prop);

    QString showValue() const override;
    bool isValueEditable() const override;
    void handle_double_click(PropertyObjectView* view, QTreeWidgetItem* item) override;

    bool hasSelection() const;
    QStringList getSelectionValues() const;
    int getCurrentSelectionIndex() const;
    void setBySelectionValue(const QString& value);
};
