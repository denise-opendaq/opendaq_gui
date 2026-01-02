#pragma once

#include "base_property_item.h"
#include <memory>

// ============================================================================
// Property factory — creates appropriate PropertyItem based on property type
// ============================================================================

std::unique_ptr<BasePropertyItem> createPropertyItem(const daq::PropertyObjectPtr& obj, const daq::PropertyPtr& prop);