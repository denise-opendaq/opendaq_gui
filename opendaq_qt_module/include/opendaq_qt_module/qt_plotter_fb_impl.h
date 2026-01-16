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
#include <QtGlobal>
#include <unordered_map>
#include <utility>

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

// Downsampling methods for high-frequency signals
enum class DownsampleMethod
{
    None = 0,    // No downsampling - show all points (may be slow)
    Simple = 1,  // Take every Nth point
    MinMax = 2,  // Keep min and max in each bucket (preserves peaks)
    LTTB = 3     // Largest Triangle Three Buckets (best visual quality)
};

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
    bool isSignalConnected;

    // Value range from descriptor for auto-scale optimization
    double valueRangeMin;
    double valueRangeMax;

    // Time range of data in series (for fast visible range check)
    qint64 dataMinTime;
    qint64 dataMaxTime;

    // Direct pointer to the series for this signal (avoids O(n*m) lookup)
    QPointer<QLineSeries> series;

    // Reusable buffer for QPointF to avoid allocations
    QVector<QPointF> pointsBuffer;

    SignalContext(const daq::InputPortPtr& port)
        : inputPort(port)
        , streamReader(daq::StreamReaderFromPort(port, daq::SampleType::Float64, daq::SampleType::Int64))
        , timeReader(streamReader)
        , isSignalConnected(false)
        , valueRangeMin(0.0)
        , valueRangeMax(0.0)
        , dataMinTime(0)
        , dataMaxTime(0)
        , series(nullptr)
    {
        pointsBuffer.reserve(200);
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
    void propertyChanged(const StringPtr& propertyName, const BaseObjectPtr& value);

    void updateInputPorts();

    void updatePlot();
    void createSeriesForSignal(SignalContext& sigCtx, size_t seriesIndex);  // Create and configure QLineSeries for a signal
    void handleEventPacket(SignalContext& sigCtx, const daq::EventPacketPtr& eventPacket);  // Handle event packets (e.g., DATA_DESCRIPTOR_CHANGED)
    bool handleData(SignalContext& sigCtx, QLineSeries* series, size_t count, qint64& outLatestTime);  // Handle data reading, processing, and series update
    
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

    // Lazy rendering methods - work with visible points only
    std::pair<int, int> getVisibleRange(const SignalContext& sigCtx, qint64 visibleMin, qint64 visibleMax) const;
    void updateVisibleSeries(SignalContext& sigCtx, QLineSeries* series, qint64 visibleMin, qint64 visibleMax);
    
    // Binary search helpers for sorted QPointF vectors (sorted by x coordinate)
    int binarySearchFirstGE(const QVector<QPointF>& points, qint64 targetTime) const;
    int binarySearchLastLE(const QVector<QPointF>& points, qint64 targetTime, int startIdx = 0) const;
    
    // Downsampling methods for visible points (work with indices in pointsBuffer)
    QVector<QPointF> downsampleVisibleNone(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx) const;
    QVector<QPointF> downsampleVisibleSimple(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx, size_t targetPoints) const;
    QVector<QPointF> downsampleVisibleMinMax(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx, size_t targetPoints) const;
    QVector<QPointF> downsampleVisibleLTTB(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx, size_t targetPoints) const;

private:
    std::unordered_map<daq::InputPortPtr, SignalContext, InputPortHash, InputPortEqual> signalContexts;
    size_t inputPortCount;

    // Properties
    double duration;  // Time window to display in seconds
    double durationHistory;  // How long to keep history in seconds (for scrollback)
    bool showLegend;
    bool autoScale;
    bool showGrid;
    bool autoClear;
    double defaultMinY;  // Default Y-axis minimum when no value range from descriptor
    double defaultMaxY;  // Default Y-axis maximum when no value range from descriptor
    DownsampleMethod downsampleMethod;  // Downsampling algorithm to use
    size_t maxSamplesPerSeries;  // Maximum number of points to keep per series

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
    
    // Timer for debouncing visible series updates during zoom/pan
    QPointer<QTimer> visibleUpdateTimer;
    qint64 pendingVisibleMin = 0;
    qint64 pendingVisibleMax = 0;

    std::vector<double> samples;
    std::vector<std::chrono::system_clock::time_point> timeStamps;
    
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
