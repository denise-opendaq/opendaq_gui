#pragma once

#include "base_core_type_handler.h"

class EnumerationCoreTypeHandler : public BaseCoreTypeHandler
{
public:
    explicit EnumerationCoreTypeHandler(const daq::EnumerationTypePtr& enumType);

    QString valueToString(const daq::BaseObjectPtr& value) const override;
    daq::BaseObjectPtr stringToValue(const QString& str) const override;
    bool hasSelection() const override;
    QStringList getSelectionValues() const override;
    bool isEditable() const override;
    daq::CoreType getCoreType() const override;

private:
    daq::EnumerationTypePtr enumType;
    daq::TypeManagerPtr typeManager;
};
