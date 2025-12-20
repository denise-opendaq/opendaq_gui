#pragma once

#include "base_property_item.h"
#include "object_property_item.h"
#include "int_property_item.h"
#include "bool_property_item.h"
#include "dict_property_item.h"
#include "list_property_item.h"
#include "struct_property_item.h"

#include <memory>

// ============================================================================
// Property factory — creates appropriate PropertyItem based on property type
// ============================================================================

static std::unique_ptr<BasePropertyItem> createPropertyItem(const daq::PropertyObjectPtr& obj, const daq::PropertyPtr& prop)
{
    auto type = prop.getValueType();
    switch(prop.getValueType())
    {
        case daq::CoreType::ctObject:
            return std::make_unique<ObjectPropertyItem>(obj, prop);
        case daq::CoreType::ctDict:
            return std::make_unique<DictPropertyItem>(obj, prop);
        case daq::CoreType::ctList:
            return std::make_unique<ListPropertyItem>(obj, prop);
        case daq::CoreType::ctStruct:
            return std::make_unique<StructPropertyItem>(obj, prop);
        case daq::CoreType::ctInt:
            return std::make_unique<IntPropertyItem>(obj, prop);
        case daq::CoreType::ctBool:
            return std::make_unique<BoolPropertyItem>(obj, prop);
        default:
            return std::make_unique<BasePropertyItem>(obj, prop);
    }
}