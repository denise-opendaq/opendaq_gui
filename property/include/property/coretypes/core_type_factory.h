#pragma once

#include "base_core_type_handler.h"
#include <memory>

/**
 * Factory for creating appropriate core type handlers
 */
class CoreTypeFactory
{
public:
    /**
     * Create handler for a value with known core type
     * @param value - example value to extract enumeration type if needed
     * @param coreType - the core type of values to handle
     * For Enumeration types, value should be an IEnumeration object to extract the type
     */
    static std::shared_ptr<BaseCoreTypeHandler> createHandler(const daq::BaseObjectPtr& value, daq::CoreType coreType);
};
