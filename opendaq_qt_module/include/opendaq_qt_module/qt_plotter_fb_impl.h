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
#include <QPoint>
#include <QVector>
#include <unordered_map>

QT_BEGIN_NAMESPACE
class QChartView;
class QChart;
class QValueAxis;
class QDateTimeAxis;
QT_END_NAMESPACE

BEGIN_NAMESPACE_OPENDAQ_QT_MODULE

namespace QtPlotter
{

// Forward declarations
class QtPlotterFbImpl;

// Event filter for custom chart interactions
class ChartEventFilter : public QObject
{
    Q_OBJECT
public:
    ChartEventFilter(QChartView* chartView, QtPlotterFbImpl* plotter);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QPointer<QChartView> m_chartView;
    QtPlotterFbImpl* m_plotter;
    bool m_isPanning;
    QPoint m_lastMousePos;
};

struct SignalContext
{
    daq::InputPortPtr inputPort;

    daq::StreamReaderPtr streamReader;
    daq::TimeReader<daq::StreamReaderPtr> timeReader;

    std::string caption;

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
    friend class ChartEventFilter;

public:
    explicit QtPlotterFbImpl(const daq::ContextPtr& ctx,
                             const daq::ComponentPtr& parent,
                             const daq::StringPtr& localId,
                             const daq::PropertyObjectPtr& config = nullptr);

    ~QtPlotterFbImpl() override;

    static daq::FunctionBlockTypePtr CreateType();

    void removed() override;

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
    double duration;  // Time window to display in seconds
    bool showLegend;
    bool autoScale;
    
    // Performance limits
    static constexpr size_t MAX_POINTS_PER_SERIES = 5000;  // Maximum points to keep in each series (reduced for better performance)
    static constexpr size_t MAX_BATCH_SIZE = 200;  // Maximum points to read per update
    static constexpr size_t DOWNSAMPLE_THRESHOLD = 1000;  // Start downsampling if more points than this

    // Qt Widget
    QPointer<QWidget> plotWindow;
    QPointer<QChartView> chartView;
    QPointer<QChart> chart;
    QPointer<QDateTimeAxis> axisX;
    QPointer<QValueAxis> axisY;
    bool userInteracting;  // Track if user is zooming/panning

    std::vector<double> samples;
    std::vector<std::chrono::system_clock::time_point> timeStamps;
    
    // Reusable buffer for QPointF to avoid allocations
    QVector<QPointF> pointsBuffer;
};

}  // namespace QtPlotter

END_NAMESPACE_OPENDAQ_QT_MODULE
