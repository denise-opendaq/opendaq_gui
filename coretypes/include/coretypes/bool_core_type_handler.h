#pragma once

#include "base_core_type_handler.h"

class BoolCoreTypeHandler : public BaseCoreTypeHandler
{
public:
    QString valueToString(const daq::BaseObjectPtr& value) const override;
    daq::BaseObjectPtr stringToValue(const QString& str) const override;
    bool hasSelection() const override;
    QStringList getSelectionValues() const override;
    bool isEditable() const override;
    daq::CoreType getCoreType() const override;

    // Override to toggle value instead of showing combobox
    bool handleDoubleClick(PropertyObjectView* view,
                          QTreeWidgetItem* item,
                          const daq::BaseObjectPtr& currentValue,
                          std::function<void(const daq::BaseObjectPtr&)> setValue) override;
};

