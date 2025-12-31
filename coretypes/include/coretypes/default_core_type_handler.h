#pragma once

#include "base_core_type_handler.h"

/**
 * Default handler for all editable core types (Int, Float, String, etc.)
 * Uses toString() and convertTo() for conversion
 */
class DefaultCoreTypeHandler : public BaseCoreTypeHandler
{
public:
    explicit DefaultCoreTypeHandler(daq::CoreType coreType);

    QString valueToString(const daq::BaseObjectPtr& value) const override;
    daq::BaseObjectPtr stringToValue(const QString& str) const override;
    bool hasSelection() const override;
    QStringList getSelectionValues() const override;
    bool isEditable() const override;
    daq::CoreType getCoreType() const override;

private:
    daq::CoreType coreType;
};

