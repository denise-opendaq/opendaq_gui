#pragma once

#include "base_property_item.h"
#include "object_property_item.h"
#include "int_property_item.h"
#include "bool_property_item.h"
#include "dict_property_item.h"
#include "list_property_item.h"
#include "struct_property_item.h"
#include "enumeration_property_item.h"
#include "function_property_item.h"

#include <memory>

// ============================================================================
// Property factory — creates appropriate PropertyItem based on property type
// ============================================================================

std::unique_ptr<BasePropertyItem> createPropertyItem(const daq::PropertyObjectPtr& obj, const daq::PropertyPtr& prop);