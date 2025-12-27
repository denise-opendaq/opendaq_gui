#include "property/coretypes/core_type_factory.h"
#include "property/coretypes/bool_core_type_handler.h"
#include "property/coretypes/enumeration_core_type_handler.h"
#include "property/coretypes/default_core_type_handler.h"
#include "context/AppContext.h"
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

std::shared_ptr<BaseCoreTypeHandler> CoreTypeFactory::createHandler(const daq::BaseObjectPtr& value, daq::CoreType coreType)
{
    switch (coreType)
    {
        case daq::CoreType::ctBool:
            return std::make_shared<BoolCoreTypeHandler>();

        case daq::CoreType::ctEnumeration:
        {
            // For enumeration, extract the type from the value
            if (value.assigned())
            {
                try
                {
                    const auto enumValue = value.asPtr<daq::IEnumeration>(true);
                    if (enumValue.assigned())
                    {
                        const auto enumType = enumValue.getEnumerationType();
                        if (enumType.assigned())
                            return std::make_shared<EnumerationCoreTypeHandler>(enumType);
                    }
                }
                catch (const std::exception& e)
                {
                    const auto loggerComponent = AppContext::getLoggerComponent();
                    LOG_D("Error creating enumeration handler, falling back to default: {}", e.what());
                    // Fall through to default handler
                }
                catch (...)
                {
                    const auto loggerComponent = AppContext::getLoggerComponent();
                    LOG_D("Unknown error creating enumeration handler, falling back to default");
                    // Fall through to default handler
                }
            }
            return std::make_shared<DefaultCoreTypeHandler>(coreType);
        }

        case daq::CoreType::ctInt:
        case daq::CoreType::ctFloat:
        case daq::CoreType::ctString:
        case daq::CoreType::ctRatio:
        case daq::CoreType::ctComplexNumber:
        default:
            return std::make_shared<DefaultCoreTypeHandler>(coreType);
    }
}
