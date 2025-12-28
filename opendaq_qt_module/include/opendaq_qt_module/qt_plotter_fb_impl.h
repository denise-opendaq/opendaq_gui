#pragma once
#include <opendaq_qt_module/common.h>
#include <opendaq/function_block_impl.h>
#include <opendaq/function_block_type_factory.h>
#include <opendaq/signal_ptr.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/reader_factory.h>
#include <opendaq/time_reader.h>
#include <QWidget>
#include <QPointer>
#include <unordered_map>

class QCustomPlot;

BEGIN_NAMESPACE_OPENDAQ_QT_MODULE

namespace QtPlotter
{

struct SignalContext
{
    daq::InputPortPtr inputPort;

    daq::StreamReaderPtr streamReader;
    daq::TimeReader<daq::StreamReaderPtr> timeReader;

    std::string caption;
    std::chrono::system_clock::time_point firstSample;

    SignalContext(const daq::InputPortPtr& port)
        : inputPort(port)
        , streamReader(daq::StreamReaderFromPort(port, daq::SampleType::Float64, daq::SampleType::Int64))
        , timeReader(streamReader)
    {
    }
};

// Custom hash for InputPortPtr based on underlying object pointer
struct InputPortHash
{
    std::size_t operator()(const daq::InputPortPtr& port) const
    {
        return std::hash<void*>{}(port.getObject());
    }
};

// Custom equality for InputPortPtr
struct InputPortEqual
{
    bool operator()(const daq::InputPortPtr& lhs, const daq::InputPortPtr& rhs) const
    {
        return lhs.getObject() == rhs.getObject();
    }
};

class QtPlotterFbImpl : public daq::FunctionBlock
{
public:
    explicit QtPlotterFbImpl(const daq::ContextPtr& ctx,
                             const daq::ComponentPtr& parent,
                             const daq::StringPtr& localId,
                             const daq::PropertyObjectPtr& config = nullptr);

    ~QtPlotterFbImpl() override;

    static daq::FunctionBlockTypePtr CreateType();

    void removed() override;

protected:
    void onConnected(const daq::InputPortPtr& inputPort) override;
    void onDisconnected(const daq::InputPortPtr& inputPort) override;

private:
    void initProperties();
    void propertyChanged();
    void readProperties();

    void updateInputPorts();

    void openPlotWindow();
    void closePlotWindow();
    void updatePlot();

    void subscribeToSignalCoreEvent(const daq::SignalPtr& signal);
    void processCoreEvent(daq::ComponentPtr& component, daq::CoreEventArgsPtr& args);

private:
    std::unordered_map<daq::InputPortPtr, SignalContext, InputPortHash, InputPortEqual> signalContexts;
    size_t inputPortCount;

    // Properties
    double duration;  // Time window in seconds
    bool freeze;
    bool showLegend;
    bool autoScale;

    // Qt Widget
    QPointer<QWidget> plotWindow;
    QPointer<QCustomPlot> customPlot;
};

}  // namespace QtPlotter

END_NAMESPACE_OPENDAQ_QT_MODULE
