#include <opendaq_qt_module/opendaq_qt_module_impl.h>
#include <opendaq_qt_module/qt_plotter_fb_impl.h>
#include <opendaq_qt_module/version.h>
#include <coretypes/version_info_factory.h>
#include <opendaq/custom_log.h>

BEGIN_NAMESPACE_OPENDAQ_QT_MODULE

OpendaqQtModule::OpendaqQtModule(daq::ContextPtr ctx)
    : Module("OpenDAQQtModule",
             daq::VersionInfo(OPENDAQ_QT_MODULE_MAJOR_VERSION,
                              OPENDAQ_QT_MODULE_MINOR_VERSION,
                              OPENDAQ_QT_MODULE_PATCH_VERSION),
             std::move(ctx),
             "OpenDAQQtModule")
{
}

daq::DictPtr<daq::IString, daq::IFunctionBlockType> OpendaqQtModule::onGetAvailableFunctionBlockTypes()
{
    auto types = daq::Dict<daq::IString, daq::IFunctionBlockType>();

    const auto typePlotter = QtPlotter::QtPlotterFbImpl::CreateType();
    types.set(typePlotter.getId(), typePlotter);

    return types;
}

daq::FunctionBlockPtr OpendaqQtModule::onCreateFunctionBlock(const daq::StringPtr& id,
                                                              const daq::ComponentPtr& parent,
                                                              const daq::StringPtr& localId,
                                                              const daq::PropertyObjectPtr& config)
{
    if (id == QtPlotter::QtPlotterFbImpl::CreateType().getId())
    {
        daq::FunctionBlockPtr fb = daq::createWithImplementation<daq::IFunctionBlock, QtPlotter::QtPlotterFbImpl>(
            context, parent, localId, config);
        return fb;
    }

    LOG_W("Function block with id '{}' not found in OpenDAQ Qt Module", id)
    return nullptr;
}

END_NAMESPACE_OPENDAQ_QT_MODULE
