#pragma once

#include "base_core_type_handler.h"

/**
 * Handler for Dict core type
 */
class DictCoreTypeHandler : public BaseCoreTypeHandler
{
public:
    explicit DictCoreTypeHandler(daq::CoreType keyType, daq::CoreType itemType);

    QString valueToString(const daq::BaseObjectPtr& value) const override;
    daq::BaseObjectPtr stringToValue(const QString& str) const override;
    bool hasSelection() const override;
    QStringList getSelectionValues() const override;
    bool isEditable() const override;
    bool hasSubtree(const daq::BaseObjectPtr& value) const override;
    void buildSubtree(QTreeWidgetItem* parent, const daq::BaseObjectPtr& value) const override;
    daq::CoreType getCoreType() const override;

private:
    daq::CoreType keyType;
    daq::CoreType itemType;
};
