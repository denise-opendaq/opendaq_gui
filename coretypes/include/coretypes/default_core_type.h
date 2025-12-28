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
        case daq::CoreType::ctList:
            return daq::List<daq::IBaseObject>().detach();
        case daq::CoreType::ctDict:
            return daq::Dict<daq::IBaseObject, daq::IBaseObject>().detach();
        default:
            return nullptr;
    }
}

