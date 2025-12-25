#include "property/property_factory.h"
#include "property/object_property_item.h"
#include "property/dict_property_item.h"
#include "property/list_property_item.h"
#include "property/struct_property_item.h"
#include "property/int_property_item.h"
#include "property/bool_property_item.h"
#include "property/enumeration_property_item.h"

std::unique_ptr<BasePropertyItem> createPropertyItem(const daq::PropertyObjectPtr& obj, const daq::PropertyPtr& prop)
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
        case daq::CoreType::ctEnumeration:
            return std::make_unique<EnumerationPropertyItem>(obj, prop);
        default:
            return std::make_unique<BasePropertyItem>(obj, prop);
    }
}

