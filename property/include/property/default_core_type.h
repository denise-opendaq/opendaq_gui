#pragma once
#include <opendaq/opendaq.h>

static daq::BaseObjectPtr createDefaultValue(daq::CoreType type)
{
    switch (type)
    {
        case daq::CoreType::ctBool:
            return daq::Bool(false);
        case daq::CoreType::ctInt:
            return daq::Integer(0);
        case daq::CoreType::ctFloat:
            return daq::Float(0.0);
        case daq::CoreType::ctString:
            return daq::String("empty");
        default:
            return nullptr;
    }
}