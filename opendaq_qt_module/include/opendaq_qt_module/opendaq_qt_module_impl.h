#pragma once
#include <opendaq_qt_module/common.h>
#include <opendaq/module_impl.h>

BEGIN_NAMESPACE_OPENDAQ_QT_MODULE

class OpendaqQtModule final : public daq::Module
{
public:
    explicit OpendaqQtModule(daq::ContextPtr ctx);

    daq::DictPtr<daq::IString, daq::IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    daq::FunctionBlockPtr onCreateFunctionBlock(const daq::StringPtr& id,
                                                 const daq::ComponentPtr& parent,
                                                 const daq::StringPtr& localId,
                                                 const daq::PropertyObjectPtr& config) override;
};

END_NAMESPACE_OPENDAQ_QT_MODULE
