#pragma once

#include "coretypes/stringobject.h"
#include <coretypes/baseobject.h>
#include <coretypes/common.h>

struct QWidget;

DECLARE_OPENDAQ_INTERFACE(IQTWidget, daq::IBaseObject)
{
    virtual daq::ErrCode INTERFACE_FUNC getWidget(QWidget** widget) = 0;
    virtual daq::ErrCode INTERFACE_FUNC getName(daq::IString** name) = 0;
};

