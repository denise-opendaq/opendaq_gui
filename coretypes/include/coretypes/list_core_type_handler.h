#pragma once

#include "base_core_type_handler.h"

/**
 * Handler for List core type
 * Supports parsing comma-separated values from string
 */
class ListCoreTypeHandler : public BaseCoreTypeHandler
{
public:
    explicit ListCoreTypeHandler(daq::CoreType itemType);

    QString valueToString(const daq::BaseObjectPtr& value) const override;
    daq::BaseObjectPtr stringToValue(const QString& str) const override;
    bool hasSelection() const override;
    QStringList getSelectionValues() const override;
    bool isEditable() const override;
    daq::CoreType getCoreType() const override;

private:
    daq::CoreType itemType;
    daq::BaseObjectPtr parseItem(const QString& str) const;
};

