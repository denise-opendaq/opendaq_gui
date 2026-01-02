#include "coretypes/core_type_factory.h"
#include "coretypes/bool_core_type_handler.h"
#include "coretypes/enumeration_core_type_handler.h"
#include "coretypes/default_core_type_handler.h"
#include "coretypes/list_core_type_handler.h"
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
                    const auto enumValue = value.asPtrOrNull<daq::IEnumeration>(true);
                    if (enumValue.assigned())
                    {
                        const auto enumType = enumValue.getEnumerationType();
                        return std::make_shared<EnumerationCoreTypeHandler>(enumType);
                    }
                }
                catch (const std::exception& e)
                {
                    const auto loggerComponent = AppContext::LoggerComponent();
                    LOG_D("Error creating enumeration handler, falling back to default: {}", e.what());
                    // Fall through to default handler
                }
                catch (...)
                {
                    const auto loggerComponent = AppContext::LoggerComponent();
                    LOG_D("Unknown error creating enumeration handler, falling back to default");
                    // Fall through to default handler
                }
            }
            return std::make_shared<DefaultCoreTypeHandler>(coreType);
        }

        case daq::CoreType::ctList:
        {
            // For list, try to extract item type from the value
            if (value.assigned())
            {
                try
                {
                    const auto list = value.asPtrOrNull<daq::IList>(true);
                    if (list.assigned() && list.getCount() > 0)
                    {
                        // Get item type from first element
                        auto firstItem = list.getItemAt(0);
                        if (firstItem.assigned())
                        {
                            daq::CoreType itemType = firstItem.getCoreType();
                            return std::make_shared<ListCoreTypeHandler>(itemType);
                        }
                    }
                }
                catch (...)
                {
                    // Fall through to default
                }
            }
            // Default to String item type if we can't determine
            return std::make_shared<ListCoreTypeHandler>(daq::CoreType::ctString);
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

