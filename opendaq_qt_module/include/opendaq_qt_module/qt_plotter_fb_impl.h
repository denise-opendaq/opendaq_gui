#pragma once
#include <opendaq_qt_module/common.h>
#include <qt_widget_interface/qt_widget_interface.h>
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
class QTimer;
class QLineSeries;
class QScatterSeries;
class QGraphicsTextItem;
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
    QPoint m_clickStartPos;
    bool m_isClick;  // Track if this is a click (not drag)
    bool m_isDraggingMarker;  // Track if dragging a marker
    int m_draggedMarkerIndex;  // Index of marker being dragged
};

struct SignalContext
{
    daq::InputPortPtr inputPort;

    daq::StreamReaderPtr streamReader;
    daq::TimeReader<daq::StreamReaderPtr> timeReader;

    std::string caption;
    double lastValue;
    bool hasLastValue;

    SignalContext(const daq::InputPortPtr& port)
        : inputPort(port)
        , streamReader(daq::StreamReaderFromPort(port, daq::SampleType::Float64, daq::SampleType::Int64))
        , timeReader(streamReader)
        , lastValue(0.0)
        , hasLastValue(false)
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

class QtPlotterFbImpl : public daq::FunctionBlockImpl<daq::IFunctionBlock, IQTWidget>
{
    friend class ChartEventFilter;
    using Super = daq::FunctionBlockImpl<daq::IFunctionBlock, IQTWidget>;

public:
    explicit QtPlotterFbImpl(const daq::ContextPtr& ctx,
                             const daq::ComponentPtr& parent,
                             const daq::StringPtr& localId,
                             const daq::PropertyObjectPtr& config = nullptr);

    static daq::FunctionBlockTypePtr CreateType();

    void onConnected(const daq::InputPortPtr& inputPort) override;
    void onDisconnected(const daq::InputPortPtr& inputPort) override;

    // Implement IQTWidget interface
    void initWidget();
    ErrCode getWidget(struct QWidget** widget) override;
    
    // Widget initialization helpers
    void createChart();
    void createWidget();
    void setupButtons(QWidget* widget);
    void setupTimer();

private:
    void initProperties();
    void propertyChanged();
    void readProperties();

    void updateInputPorts();

    void updatePlot();

    void subscribeToSignalCoreEvent(const daq::SignalPtr& signal);
    void processCoreEvent(daq::ComponentPtr& component, daq::CoreEventArgsPtr& args);
    
    // Marker methods
    void addMarkerAtTime(qint64 timeMsec);
    void updateMarkers();
    void removeMarker(int index);
    void moveMarker(int index, qint64 newTimeMsec);
    int findMarkerAtPosition(const QPointF& scenePos, qreal tolerance = 10.0);
    bool isDeleteButtonAtPosition(int markerIndex, const QPointF& scenePos);
    void showDeleteButton(int markerIndex);
    void hideDeleteButtons();
    double getSignalValueAtTime(QLineSeries* series, qint64 timeMsec);
    QPointF constrainLabelPosition(const QPointF& pos, const QRectF& labelRect, const QRectF& plotArea);

private:
    std::unordered_map<daq::InputPortPtr, SignalContext, InputPortHash, InputPortEqual> signalContexts;
    size_t inputPortCount;

    // Properties
    double duration;  // Time window to display in seconds
    bool showLegend;
    bool autoScale;
    bool showGrid;
    bool showLastValue;
    
    // Performance limits
    static constexpr size_t MAX_POINTS_PER_SERIES = 5000;  // Maximum points to keep in each series (reduced for better performance)
    static constexpr size_t MAX_BATCH_SIZE = 200;  // Maximum points to read per update
    static constexpr size_t DOWNSAMPLE_THRESHOLD = 1000;  // Start downsampling if more points than this

    // Qt Widget
    QPointer<QChart> chart;
    QPointer<QChartView> chartView;  // Keep reference to chart view for scene access
    QPointer<QDateTimeAxis> axisX;
    QPointer<QValueAxis> axisY;
    bool userInteracting;  // Track if user is zooming/panning
    
    // Widget for embedding in tabs
    QPointer<QWidget> embeddedWidget;
    
    // Update timer for plot
    QPointer<QTimer> updateTimer;

    std::vector<double> samples;
    std::vector<std::chrono::system_clock::time_point> timeStamps;
    
    // Reusable buffer for QPointF to avoid allocations
    QVector<QPointF> pointsBuffer;
    
    // Markers (vertical lines with value annotations)
    struct Marker
    {
        qint64 timeMsec = 0;
        QPointer<QLineSeries> verticalLine;
        QPointer<QScatterSeries> valuePoints;  // Scatter points at signal intersections
        QList<QString> valueLabels;  // Store labels for each signal
        QList<QPointF> valuePositions;  // Store positions (time, value) for each signal
        QPointer<QGraphicsTextItem> timeLabel;  // Time label at bottom
        QList<QPointer<QGraphicsTextItem>> valueLabelsItems;  // Value labels near intersection points
        QPointer<QGraphicsTextItem> deleteButton;  // Delete button (X) shown on hover
    };
    QList<Marker> markers;
};

}  // namespace QtPlotter

END_NAMESPACE_OPENDAQ_QT_MODULE
