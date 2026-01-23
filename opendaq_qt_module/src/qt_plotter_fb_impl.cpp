#include <opendaq_qt_module/qt_plotter_fb_impl.h>
#include <opendaq/function_block_ptr.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/input_port_factory.h>
#include <opendaq/data_descriptor_ptr.h>
#include <opendaq/custom_log.h>
#include <opendaq/range_type.h>
#include <coretypes/complex_number_type.h>
#include <opendaq/reader_utils.h>
#include <coreobjects/callable_info_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_factory.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QTimer>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegendMarker>
#include <QDateTime>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QPalette>
#include <QGraphicsTextItem>
#include <QGraphicsScene>
#include <QGraphicsLayout>

BEGIN_NAMESPACE_OPENDAQ_QT_MODULE

namespace QtPlotter
{

// ChartEventFilter implementation
ChartEventFilter::ChartEventFilter(QChartView* chartView, QtPlotterFbImpl* plotter)
    : QObject(chartView)
    , m_chartView(chartView)
    , m_plotter(plotter)
    , m_isPanning(false)
    , m_isClick(false)
    , m_isDraggingMarker(false)
    , m_draggedMarkerIndex(-1)
{}

bool ChartEventFilter::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel)
    {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);

        // Only zoom if Ctrl key is pressed
        if (wheelEvent->modifiers() & Qt::ControlModifier)
        {
            m_plotter->userInteracting = true;
            
            // Get mouse position in chart coordinates
            QPointF scenePos = m_chartView->mapToScene(wheelEvent->position().toPoint());
            auto seriesList = m_chartView->chart()->series();
            if (!seriesList.isEmpty())
            {
                QPointF valuePos = m_chartView->chart()->mapToValue(scenePos, seriesList.first());
                
                // Get current axis ranges
                auto axes = m_chartView->chart()->axes();
                QDateTimeAxis* axisX = nullptr;
                QValueAxis* axisY = nullptr;
                for (auto* axis : axes)
                {
                    if (!axisX)
                        axisX = qobject_cast<QDateTimeAxis*>(axis);
                    if (!axisY)
                        axisY = qobject_cast<QValueAxis*>(axis);
                }

                if (axisX && axisY)
                {
                    QDateTime minTime = axisX->min();
                    QDateTime maxTime = axisX->max();
                    qreal minY = axisY->min();
                    qreal maxY = axisY->max();
                    
                    // Calculate zoom factor (1.1 for scroll up, 1/1.1 for scroll down)
                    qreal zoomFactor = wheelEvent->angleDelta().y() > 0 ? 1.1 : 1.0 / 1.1;
                    
                    // Get mouse position in chart value coordinates
                    qint64 mouseTime = static_cast<qint64>(valuePos.x());
                    qreal mouseY = valuePos.y();
                    
                    // Calculate relative position of mouse in current range (0.0 to 1.0)
                    qint64 minTimeMsec = minTime.toMSecsSinceEpoch();
                    qint64 maxTimeMsec = maxTime.toMSecsSinceEpoch();
                    qint64 currentTimeRange = maxTimeMsec - minTimeMsec;
                    
                    // Prevent zoom if range is zero or invalid
                    if (currentTimeRange <= 0)
                        return true;
                    
                    qreal timeRatio = static_cast<qreal>(mouseTime - minTimeMsec) / static_cast<qreal>(currentTimeRange);
                    
                    qreal currentYRange = maxY - minY;
                    qreal yRatio = currentYRange > 0 ? (mouseY - minY) / currentYRange : 0.5;
                    
                    // Get plot area size for aspect ratio calculation
                    QRectF plotArea = m_chartView->chart()->plotArea();
                    qreal plotWidth = plotArea.width();
                    qreal plotHeight = plotArea.height();
                    
                    // Calculate or update screen aspect ratio
                    if (plotWidth > 0 && plotHeight > 0 && currentTimeRange > 0 && currentYRange > 0)
                        m_plotter->screenAspectRatio = (plotWidth / static_cast<qreal>(currentTimeRange)) / 
                                                       (plotHeight / currentYRange);
                    
                    // Calculate new time range
                    qreal newTimeRangeReal = static_cast<qreal>(currentTimeRange) / zoomFactor;
                    
                    // Prevent zoom in if range becomes too small
                    if (zoomFactor > 1.0 && newTimeRangeReal < 1.0)
                        return true;
                    
                    // Ensure zoom out increases range
                    if (zoomFactor < 1.0 && currentTimeRange <= 1)
                        newTimeRangeReal = qMax(newTimeRangeReal, 2.0);
                    
                    qint64 newTimeRange = static_cast<qint64>(newTimeRangeReal + 0.5);
                    if (newTimeRange < 1)
                        newTimeRange = 1;
                    
                    // Ensure zoom out actually increases range
                    if (zoomFactor < 1.0 && newTimeRange <= currentTimeRange)
                    {
                        qint64 minIncrease = qMax(static_cast<qint64>(2), currentTimeRange / 10);
                        newTimeRange = currentTimeRange + minIncrease;
                    }
                    
                    newTimeRangeReal = static_cast<qreal>(newTimeRange);
                    
                    // Calculate new Y range maintaining screen aspect ratio
                    qreal newYRange;
                    if (plotWidth > 0 && plotHeight > 0 && m_plotter->screenAspectRatio > 0)
                        newYRange = (m_plotter->screenAspectRatio * plotHeight * newTimeRangeReal) / plotWidth;
                    else
                        newYRange = currentYRange / zoomFactor;
                    
                    // Calculate new time bounds keeping mouse position fixed
                    qreal mouseTimeReal = static_cast<qreal>(mouseTime);
                    qreal newMinTimeReal = mouseTimeReal - newTimeRangeReal * timeRatio;
                    qreal newMaxTimeReal = mouseTimeReal + newTimeRangeReal * (1.0 - timeRatio);
                    
                    // Ensure valid range
                    if (newMinTimeReal >= newMaxTimeReal)
                        return true;
                    
                    qint64 newMinTime = static_cast<qint64>(newMinTimeReal);
                    qint64 newMaxTime = static_cast<qint64>(newMaxTimeReal);
                    
                    // Ensure valid range
                    if (newMinTime >= newMaxTime)
                    {
                        qint64 centerTime = (newMinTime + newMaxTime) / 2;
                        newMinTime = centerTime - newTimeRange / 2;
                        newMaxTime = centerTime + newTimeRange / 2;
                        if (newMaxTime <= newMinTime)
                            newMaxTime = newMinTime + 1;
                    }
                    
                    // Calculate new Y bounds
                    qreal newMinY = mouseY - newYRange * yRatio;
                    qreal newMaxY = mouseY + newYRange * (1.0 - yRatio);
                    
                    // Ensure valid Y range
                    if (newMinY >= newMaxY)
                    {
                        qreal minYRange = qMax(std::abs(newMinY) * 1e-10, 1e-10);
                        newMaxY = newMinY + minYRange;
                    }
                        
                    // Apply zoom
                    axisX->setRange(QDateTime::fromMSecsSinceEpoch(newMinTime), QDateTime::fromMSecsSinceEpoch(newMaxTime));
                    axisY->setRange(newMinY, newMaxY);
                }
            }
            
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF scenePos = m_chartView->mapToScene(mouseEvent->pos());
        
        // Check if hovering over a marker
        int markerIndex = m_plotter->findMarkerAtPosition(scenePos);
        if (markerIndex >= 0)
        {
            m_chartView->setCursor(Qt::SizeHorCursor);  // Horizontal resize cursor
            m_plotter->showDeleteButton(markerIndex);
        }
        else
        {
            if (!m_isDraggingMarker)
                m_chartView->setCursor(Qt::ArrowCursor);
            m_plotter->hideDeleteButtons();
        }
        
        if (m_isDraggingMarker && m_draggedMarkerIndex >= 0)
        {
            // Drag marker - check if within plot area
            QRectF plotArea = m_chartView->chart()->plotArea();
            if (plotArea.contains(scenePos))
            {
                auto seriesList = m_chartView->chart()->series();
                if (!seriesList.isEmpty())
                {
                    QPointF valuePos = m_chartView->chart()->mapToValue(scenePos, seriesList.first());
                    qint64 timeMsec = static_cast<qint64>(valuePos.x());
                    
                    // Check if time is within visible axis range
                    QDateTimeAxis* axisX = nullptr;
                    for (auto* axis : m_chartView->chart()->axes())
                    {
                        axisX = qobject_cast<QDateTimeAxis*>(axis);
                        if (axisX)
                            break;
                    }

                    if (axisX)
                    {
                        qint64 minTime = axisX->min().toMSecsSinceEpoch();
                        qint64 maxTime = axisX->max().toMSecsSinceEpoch();

                        // Clamp time to visible range
                        if (timeMsec < minTime)
                            timeMsec = minTime;
                        else if (timeMsec > maxTime)
                            timeMsec = maxTime;

                        m_plotter->moveMarker(m_draggedMarkerIndex, timeMsec);
                    }
                }
            }
            return true;
        }
        else if (m_isPanning)
        {
            QPoint delta = mouseEvent->pos() - m_lastMousePos;
            m_lastMousePos = mouseEvent->pos();

            // If moved more than a few pixels, it's a drag, not a click
            if (m_isClick && (mouseEvent->pos() - m_clickStartPos).manhattanLength() > 5)
                m_isClick = false;

            // Scroll the chart
            m_chartView->chart()->scroll(-delta.x(), delta.y());
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QPointF scenePos = m_chartView->mapToScene(mouseEvent->pos());
            int markerIndex = m_plotter->findMarkerAtPosition(scenePos);
            
            if (markerIndex >= 0)
            {
                // Check if clicking on delete button first
                if (m_plotter->isDeleteButtonAtPosition(markerIndex, scenePos))
                {
                    m_plotter->removeMarker(markerIndex);
                    return true;
                }
                
                // Start dragging marker
                m_isDraggingMarker = true;
                m_draggedMarkerIndex = markerIndex;
                m_isPanning = false;
                m_chartView->setCursor(Qt::SizeHorCursor);
                return true;
            }
            else
            {
                m_isPanning = true;
                m_isClick = true;
                m_lastMousePos = mouseEvent->pos();
                m_clickStartPos = mouseEvent->pos();
                m_plotter->userInteracting = true;
                m_chartView->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            if (m_isDraggingMarker)
            {
                m_isDraggingMarker = false;
                m_draggedMarkerIndex = -1;
                m_chartView->setCursor(Qt::ArrowCursor);
                return true;
            }
            else if (m_isPanning)
            {
                m_isPanning = false;
                m_chartView->setCursor(Qt::ArrowCursor);
                
                // If it was a click (not a drag), add marker
                if (m_isClick)
                {
                    // Convert screen coordinates to chart value coordinates
                    QPointF scenePos = m_chartView->mapToScene(mouseEvent->pos());
                    
                    // Check if click is within plot area
                    QRectF plotArea = m_chartView->chart()->plotArea();
                    if (!plotArea.contains(scenePos))
                    {
                        m_isClick = false;
                        return true;
                    }
                    
                    // Get first series for coordinate mapping
                    auto seriesList = m_chartView->chart()->series();
                    if (!seriesList.isEmpty())
                    {
                        QPointF valuePos = m_chartView->chart()->mapToValue(scenePos, seriesList.first());
                        
                        // Get time from X coordinate
                        qint64 timeMsec = static_cast<qint64>(valuePos.x());
                        
                        // Check if time is within visible axis range
                        QDateTimeAxis* axisX = nullptr;
                        for (auto* axis : m_chartView->chart()->axes())
                        {
                            axisX = qobject_cast<QDateTimeAxis*>(axis);
                            if (axisX)
                                break;
                        }

                        if (axisX)
                        {
                            qint64 minTime = axisX->min().toMSecsSinceEpoch();
                            qint64 maxTime = axisX->max().toMSecsSinceEpoch();

                            // Only add marker if time is within visible range
                            if (timeMsec >= minTime && timeMsec <= maxTime)
                                m_plotter->addMarkerAtTime(timeMsec);
                        }
                    }
                }
                
                m_isClick = false;
                return true;
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

QtPlotterFbImpl::QtPlotterFbImpl(const daq::ContextPtr& ctx,
                                 const daq::ComponentPtr& parent,
                                 const daq::StringPtr& localId,
                                 const daq::PropertyObjectPtr& config)
    : Super(CreateType(), ctx, parent, localId)
    , inputPortCount(0)
    , duration(1.0)
    , durationHistory(10.0)
    , showLegend(true)
    , autoScale(true)
    , showGrid(true)
    , autoClear(true)
    , defaultMinY(-10.0)
    , defaultMaxY(10.0)
    , downsampleMethod(QtPlotter::DownsampleMethod::LTTB)
    , maxSamplesPerSeries(10000)
    , lineStyle(QtPlotter::LineStyle::Solid)
    , chart(nullptr)
    , axisX(nullptr)
    , axisY(nullptr)
    , userInteracting(false)
    , embeddedWidget(nullptr)
    , updateTimer(nullptr)
{
    // Pre-allocate buffers with reasonable initial size
    samples.resize(200);
    timeStamps.resize(200);
    
    initProperties();
    updateInputPorts();
    
    // Initialize widget in constructor
    initWidget();
}


daq::FunctionBlockTypePtr QtPlotterFbImpl::CreateType()
{
    return daq::FunctionBlockType(
        "opendaq_qt_plotter",
        "Qt Signal Plotter",
        "Real-time signal visualization with zoom and time range control",
        daq::PropertyObject()
    );
}

void QtPlotterFbImpl::initProperties()
{
    auto onPropertyValueWrite = [this](daq::PropertyObjectPtr& obj, daq::PropertyValueEventArgsPtr& args)
    {
        propertyChanged(args.getProperty().getName(), args.getValue());
    };

    const auto durationProp = daq::FloatPropertyBuilder("Duration", duration)
                                  .setSuggestedValues(daq::List<daq::Float>(0.1, 0.5, 1.0, 5.0, 10.0))
                                  .setUnit(daq::Unit("s", -1, "second", "time"))
                                  .build();
    objPtr.addProperty(durationProp);
    objPtr.getOnPropertyValueWrite("Duration") += onPropertyValueWrite;

    const auto durationHistoryProp = daq::FloatPropertyBuilder("DurationHistory", durationHistory)
                                         .setSuggestedValues(daq::List<daq::Float>(1.0, 5.0, 10.0, 30.0, 60.0))
                                         .setUnit(daq::Unit("s", -1, "second", "time"))
                                         .build();
    objPtr.addProperty(durationHistoryProp);
    objPtr.getOnPropertyValueWrite("DurationHistory") += onPropertyValueWrite;

    const auto showLegendProp = daq::BoolProperty("ShowLegend", showLegend);
    objPtr.addProperty(showLegendProp);
    objPtr.getOnPropertyValueWrite("ShowLegend") += onPropertyValueWrite;

    const auto autoScaleProp = daq::BoolProperty("AutoScale", autoScale);
    objPtr.addProperty(autoScaleProp);
    objPtr.getOnPropertyValueWrite("AutoScale") += onPropertyValueWrite;

    const auto showGridProp = daq::BoolProperty("ShowGrid", showGrid);
    objPtr.addProperty(showGridProp);
    objPtr.getOnPropertyValueWrite("ShowGrid") += onPropertyValueWrite;

    const auto autoClearProp = daq::BoolProperty("AutoClear", autoClear);
    objPtr.addProperty(autoClearProp);
    objPtr.getOnPropertyValueWrite("AutoClear") += onPropertyValueWrite;

    const auto defaultMinYProp = daq::FloatProperty("DefaultMinY", defaultMinY);
    objPtr.addProperty(defaultMinYProp);
    objPtr.getOnPropertyValueWrite("DefaultMinY") += onPropertyValueWrite;

    const auto defaultMaxYProp = daq::FloatProperty("DefaultMaxY", defaultMaxY);
    objPtr.addProperty(defaultMaxYProp);
    objPtr.getOnPropertyValueWrite("DefaultMaxY") += onPropertyValueWrite;

    const auto downsampleMethodProp = daq::SelectionProperty("DownsampleMethod", List<IString>("None", "Simple", "MinMax", "LTTB"), static_cast<Int>(downsampleMethod));
    objPtr.addProperty(downsampleMethodProp);
    objPtr.getOnPropertyValueWrite("DownsampleMethod") += onPropertyValueWrite;

    const auto maxSamplesPerSeriesProp = daq::IntProperty("MaxSamplesPerSeries", static_cast<Int>(maxSamplesPerSeries));
    objPtr.addProperty(maxSamplesPerSeriesProp);
    objPtr.getOnPropertyValueWrite("MaxSamplesPerSeries") += onPropertyValueWrite;

    const auto lineStyleProp = daq::SelectionProperty("LineStyle", List<IString>("Solid", "Dashed", "Dotted"), static_cast<Int>(lineStyle));
    objPtr.addProperty(lineStyleProp);
    objPtr.getOnPropertyValueWrite("LineStyle") += onPropertyValueWrite;
}

void QtPlotterFbImpl::propertyChanged(const StringPtr& propertyName, const BaseObjectPtr& value)
{
    auto lock = getRecursiveConfigLock();

    if (propertyName == "Duration")
        duration = value;
    else if (propertyName == "DurationHistory")
        durationHistory = value;
    else if (propertyName == "ShowLegend")
    {
        showLegend = value;
        if (chart)
            chart->legend()->setVisible(showLegend);
    }
    else if (propertyName == "AutoScale")
        autoScale = value;
    else if (propertyName == "ShowGrid")
    {
        showGrid = value;
        if (axisX)
            axisX->setGridLineVisible(showGrid);
        if (axisY)
            axisY->setGridLineVisible(showGrid);
    }
    else if (propertyName == "AutoClear")
        autoClear = value;
    else if (propertyName == "DefaultMinY")
        defaultMinY = value;
    else if (propertyName == "DefaultMaxY")
        defaultMaxY = value;
    else if (propertyName == "DownsampleMethod")
        downsampleMethod = static_cast<QtPlotter::DownsampleMethod>(value.asPtr<IInteger>(true));
    else if (propertyName == "MaxSamplesPerSeries")
        maxSamplesPerSeries = value;
    else if (propertyName == "LineStyle")
    {
        lineStyle = static_cast<QtPlotter::LineStyle>(value.asPtr<IInteger>(true));
        updateSeriesLineStyle();
    }

    LOG_W("Property {} changed to {}", propertyName, value.toString());
}

void QtPlotterFbImpl::updateInputPorts()
{   
    const auto inputPort = createAndAddInputPort(
        fmt::format("Input{}", inputPortCount++),
        daq::PacketReadyNotification::SameThread);
    auto [it, _] = signalContexts.emplace(inputPort, inputPort);
    it->second.streamReader.setExternalListener(this->template borrowPtr<InputPortNotificationsPtr>());
}

void QtPlotterFbImpl::onConnected(const daq::InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();

    auto it = signalContexts.find(inputPort);
    bool createNewPort = true;
    if (it != signalContexts.end())
    {
        if (it->second.isSignalConnected)
            createNewPort = false;
        it->second.isSignalConnected = true;
    }

    if (createNewPort)
        updateInputPorts();

    LOG_W("Connected to port {}", inputPort.getLocalId());
}

void QtPlotterFbImpl::onDisconnected(const daq::InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();

    if (auto it = signalContexts.find(inputPort); it != signalContexts.end())
    {
        if (autoClear && chart && it->second.series)
        {
            chart->removeSeries(it->second.series);
            it->second.series->deleteLater();
            LOG_W("Removed series '{}' for disconnected port {}",
                  it->second.caption, inputPort.getLocalId());
        }
        signalContexts.erase(it);
    }

    removeInputPort(inputPort);
    LOG_W("Disconnected from port {}", inputPort.getLocalId());
}

Qt::PenStyle QtPlotterFbImpl::getQtPenStyle() const
{
    switch (lineStyle)
    {
        case LineStyle::Dashed:
            return Qt::DashLine;
        case LineStyle::Dotted:
            return Qt::DotLine;
        case LineStyle::Solid:
        default:
            return Qt::SolidLine;
    }
}

void QtPlotterFbImpl::updateSeriesLineStyle()
{
    Qt::PenStyle penStyle = getQtPenStyle();

    for (auto& [port, sigCtx] : signalContexts)
    {
        if (sigCtx.series)
        {
            QPen pen = sigCtx.series->pen();
            pen.setStyle(penStyle);
            sigCtx.series->setPen(pen);
        }
    }
}

void QtPlotterFbImpl::createSeriesForSignal(SignalContext& sigCtx)
{
    if (!chart || !axisX || !axisY)
        return;

    sigCtx.series = new QLineSeries();
    sigCtx.series->setName(QString::fromStdString(sigCtx.caption));

    // Colors for different signals
    static const QColor colors[] = {
        QColor(255, 0, 0),      // Red
        QColor(255, 255, 0),    // Yellow
        QColor(0, 255, 0),      // Green
        QColor(255, 0, 255),    // Magenta
        QColor(0, 255, 255),    // Cyan
        QColor(0, 0, 255)       // Blue
    };
    QPen pen(colors[(seriesIndex++) % 6], 2, getQtPenStyle());
    sigCtx.series->setPen(pen);

    chart->addSeries(sigCtx.series);
    sigCtx.series->attachAxis(axisX);
    sigCtx.series->attachAxis(axisY);
}

void QtPlotterFbImpl::handleEventPacket(SignalContext& sigCtx, const daq::EventPacketPtr& eventPacket)
{
    if (!eventPacket.assigned() || eventPacket.getEventId() != event_packet_id::DATA_DESCRIPTOR_CHANGED)
        return;

    auto sig = sigCtx.inputPort.getSignal();
    if (sig.assigned())
        sigCtx.caption = sig.getName().toStdString();
    else
        sigCtx.caption = "N/A";

    const DataDescriptorPtr descriptor = eventPacket.getParameters()[event_packet_param::DATA_DESCRIPTOR];
    if (descriptor.assigned())
    {
        auto unit = descriptor.getUnit();
        if (unit.assigned() && !unit.getSymbol().toStdString().empty())
            sigCtx.caption += fmt::format(" [{}]", unit.getSymbol().toStdString());

        // Extract value range for auto-scale
        auto valueRange = descriptor.getValueRange();
        if (valueRange.assigned())
        {
            sigCtx.valueRangeMin = valueRange.getLowValue();
            sigCtx.valueRangeMax = valueRange.getHighValue();
        }
    }

}

bool QtPlotterFbImpl::handleData(SignalContext& sigCtx, QLineSeries* series, size_t count, qint64& outLatestTime)
{
    if (!series)
        return false;

    outLatestTime = 0;

    // Sync buffer with series - but optimize: only copy if buffer is empty
    // After first sync, buffer is maintained separately and series is updated from buffer
    int currentPointCount = series->count();
    if (sigCtx.pointsBuffer.isEmpty() && currentPointCount > 0)
    {
        // Initial sync: copy from series to buffer
        const auto& existingPoints = series->points();
        sigCtx.pointsBuffer = existingPoints;
    }

    // Trim old points by time (keep only durationHistory seconds)
    if (!sigCtx.pointsBuffer.isEmpty())
    {
        auto timeMsec = std::chrono::duration_cast<std::chrono::milliseconds>(timeStamps[count - 1].time_since_epoch()).count();
        qint64 latestTime = static_cast<qint64>(timeMsec);
        qint64 historyMsec = static_cast<qint64>(durationHistory * 1000);
        qint64 minTimeToKeep = latestTime - historyMsec;

        // Use binary search to find first point >= minTimeToKeep
        if (minTimeToKeep > 0)
        {
            int firstValidIdx = binarySearchFirstGE(sigCtx.pointsBuffer, minTimeToKeep);
            if (firstValidIdx > 0)
                sigCtx.pointsBuffer.remove(0, firstValidIdx);
        }
    }

    // Add all new points to history buffer (pointsBuffer stores full duration history)
    // No downsampling here - we'll downsample only visible points later
    for (size_t i = 0; i < count; ++i)
    {
        auto timeMsec = std::chrono::duration_cast<std::chrono::milliseconds>(timeStamps[i].time_since_epoch()).count();
        sigCtx.pointsBuffer.append(QPointF(timeMsec, samples[i]));
    }

    // Update time range for fast visible range check
    if (!sigCtx.pointsBuffer.isEmpty())
    {
        sigCtx.dataMinTime = static_cast<qint64>(sigCtx.pointsBuffer.first().x());
        sigCtx.dataMaxTime = static_cast<qint64>(sigCtx.pointsBuffer.last().x());
        outLatestTime = sigCtx.dataMaxTime;
    }

    return true;
}

void QtPlotterFbImpl::updatePlot()
{
    auto lock = getRecursiveConfigLock();
    
    // Update plot if chart exists and embeddedWidget is available
    if (!chart || !embeddedWidget)
        return;

    qint64 globalLatestTime = 0;
    bool hasData = false;

    for (auto& [port, sigCtx] : signalContexts)
    {
        if (!sigCtx.isSignalConnected)
            continue;

        // Get or create series for this signal (direct pointer access - O(1))
        if (!sigCtx.series)
            createSeriesForSignal(sigCtx);

        QLineSeries* series = sigCtx.series;

        // Extract and plot data using TimeReader
        if (sigCtx.timeReader.assigned())
        {
            try
            {
                size_t count = sigCtx.timeReader.getAvailableCount();
                if (count > samples.size())
                {
                    samples.resize(count);
                    timeStamps.resize(count);
                }
                
                daq::ReaderStatusPtr status;
                sigCtx.timeReader.readWithDomain(samples.data(), timeStamps.data(), &count, 0, &status);
                if (status.assigned())
                {
                    auto eventPacket = status.getEventPacket();
                    if (eventPacket.assigned())
                        handleEventPacket(sigCtx, eventPacket);
                }
                
                if (count > 0)
                {
                    qint64 latestTime = 0;
                    if (handleData(sigCtx, series, count, latestTime))
                    {
                        if (latestTime > globalLatestTime)
                            globalLatestTime = latestTime;
                        hasData = true;
                        
                        // Update visible series immediately when adding new data
                        // (debouncing only applies to zoom/pan operations)
                        if (axisX)
                        {
                            qint64 visibleMin = axisX->min().toMSecsSinceEpoch();
                            qint64 visibleMax = axisX->max().toMSecsSinceEpoch();
                            updateVisibleSeries(sigCtx, series, visibleMin, visibleMax);
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                LOG_W("Error reading data from TimeReader: {}", e.what())
            }
        }

        // Update series name
        if (series)
        {
            std::string seriesName = sigCtx.caption;
            if (series->name() != QString::fromStdString(seriesName))
                series->setName(QString::fromStdString(seriesName));
        }
    }

    // Update axis labels and range
    if (hasData && axisX)
    {
        // Get current visible range (either from user interaction or auto-follow)
        QDateTime minTime, maxTime;

        if (!userInteracting)
        {
            // Auto-follow mode: show last 'duration' seconds
            qint64 durationMsec = static_cast<qint64>(duration * 1000);
            minTime = QDateTime::fromMSecsSinceEpoch(globalLatestTime - durationMsec);
            maxTime = QDateTime::fromMSecsSinceEpoch(globalLatestTime);
        }
        else
        {
            // Use current axis range
            minTime = axisX->min();
            maxTime = axisX->max();
            
            qint64 visibleMin = minTime.toMSecsSinceEpoch();
            qint64 visibleMax = maxTime.toMSecsSinceEpoch();

            // Check if any signal has data overlapping with visible range (O(n) where n = signals, not points)
            bool hasDataInRange = false;
            for (const auto& [port, sigCtx] : signalContexts)
            {
                if (!sigCtx.isSignalConnected)
                    continue;

                // Check if data time range overlaps with visible range
                if (sigCtx.dataMaxTime >= visibleMin && sigCtx.dataMinTime <= visibleMax)
                {
                    hasDataInRange = true;
                    break;
                }
            }
            
            // If no data in visible range, reset zoom (like resetZoomBtn)
            if (!hasDataInRange)
            {
                userInteracting = false;
                qint64 durationMsec = static_cast<qint64>(duration * 1000);
                minTime = QDateTime::fromMSecsSinceEpoch(globalLatestTime - durationMsec);
                maxTime = QDateTime::fromMSecsSinceEpoch(globalLatestTime);
                chart->zoomReset();
            }
        }

        // Update range - QDateTimeAxis handles labels automatically
        // Visible series will be updated via rangeChanged signal with debouncing
        if (minTime.isValid() && maxTime.isValid() && minTime < maxTime)
        {
            axisX->setRange(minTime, maxTime);
            axisX->setTickCount(3);
            axisX->setLabelsVisible(true);
            axisX->setFormat("HH:mm:ss.zzz");
        }
    }

    // Auto-scale Y-axis if enabled
    if (autoScale && axisY && hasData)
    {
        qreal minY = std::numeric_limits<qreal>::max();
        qreal maxY = std::numeric_limits<qreal>::lowest();

        // Use value range from descriptor if available, otherwise use default
        for (const auto& [port, sigCtx] : signalContexts)
        {
            if (!sigCtx.isSignalConnected)
                continue;

            if (sigCtx.valueRangeMin < sigCtx.valueRangeMax)
            {
                // Use range from descriptor
                if (sigCtx.valueRangeMin < minY)
                    minY = sigCtx.valueRangeMin;
                if (sigCtx.valueRangeMax > maxY)
                    maxY = sigCtx.valueRangeMax;
            }
        }

        // Fall back to default range if no valid ranges from descriptors
        if (minY >= maxY)
        {
            minY = defaultMinY;
            maxY = defaultMaxY;
        }

        if (minY < maxY)
        {
            qreal margin = (maxY - minY) * 0.1;
            axisY->setRange(minY - margin, maxY + margin);
            axisY->setTickCount(5);
        }
    }
    
    // Update markers when plot updates
    updateMarkers();
}

double QtPlotterFbImpl::getSignalValueAtTime(QLineSeries* series, qint64 timeMsec)
{
    if (!series || series->count() == 0)
        return std::numeric_limits<double>::quiet_NaN();

    const auto& points = series->points();
    int n = points.size();

    // Handle edge cases
    if (timeMsec <= static_cast<qint64>(points.first().x()))
        return points.first().y();
    if (timeMsec >= static_cast<qint64>(points.last().x()))
        return points.last().y();

    // Use binary search to find the interval containing timeMsec
    // Find first index where points[i].x() >= timeMsec
    int right = binarySearchFirstGE(points, timeMsec);
    
    // If right is 0, we're at the beginning (already handled above)
    // If right >= n, we're at the end (already handled above)
    if (right <= 0 || right >= n)
    {
        // Fallback: should not happen due to edge cases above, but handle it
        if (right >= n)
            return points.last().y();
        return points.first().y();
    }
    
    // left is the point before right
    int left = right - 1;

    // Linear interpolation between points[left] and points[right]
    qint64 t1 = static_cast<qint64>(points[left].x());
    qint64 t2 = static_cast<qint64>(points[right].x());

    if (t2 == t1)
        return points[left].y();

    double ratio = static_cast<double>(timeMsec - t1) / static_cast<double>(t2 - t1);
    return points[left].y() + ratio * (points[right].y() - points[left].y());
}

QPointF QtPlotterFbImpl::constrainLabelPosition(const QPointF& pos, const QRectF& labelRect, const QRectF& plotArea)
{
    QPointF constrainedPos = pos;
    QRectF labelBounds = QRectF(constrainedPos, labelRect.size());
    
    // Constrain horizontally
    if (labelBounds.left() < plotArea.left())
        constrainedPos.setX(plotArea.left());
    else if (labelBounds.right() > plotArea.right())
        constrainedPos.setX(plotArea.right() - labelRect.width());
    
    // Constrain vertically
    if (labelBounds.top() < plotArea.top())
        constrainedPos.setY(plotArea.top());
    else if (labelBounds.bottom() > plotArea.bottom())
        constrainedPos.setY(plotArea.bottom() - labelRect.height());
    
    return constrainedPos;
}

void QtPlotterFbImpl::addMarkerAtTime(qint64 timeMsec)
{
    if (!chart || !axisX || !axisY)
        return;
    
    // Get Y-axis range
    qreal minY = axisY->min();
    qreal maxY = axisY->max();
    
    // Create vertical line
    auto* verticalLine = new QLineSeries();
    verticalLine->append(timeMsec, minY);
    verticalLine->append(timeMsec, maxY);
    
    QPalette palette = QApplication::palette();
    QPen pen(palette.color(QPalette::Highlight), 2, Qt::DashLine);
    verticalLine->setPen(pen);
    
    chart->addSeries(verticalLine);
    verticalLine->attachAxis(axisX);
    verticalLine->attachAxis(axisY);
    
    // Explicitly hide marker from legend
    auto legendMarkers = chart->legend()->markers(verticalLine);
    for (auto* marker : legendMarkers)
        marker->setVisible(false);
    
    // Create scatter series for value points
    auto* valuePoints = new QScatterSeries();
    valuePoints->setMarkerSize(8);
    valuePoints->setColor(palette.color(QPalette::Highlight));
    valuePoints->setBorderColor(palette.color(QPalette::Base));
    
    // Find intersections with all signal series
    QStringList valueLabels;
    QList<QPointF> valuePositions;  // Store positions for value labels
    for (auto* series : chart->series())
    {
        auto* lineSeries = qobject_cast<QLineSeries*>(series);
        if (lineSeries && lineSeries->count() > 0 && !lineSeries->name().isEmpty() && lineSeries != verticalLine)
        {
            double value = getSignalValueAtTime(lineSeries, timeMsec);
            if (!std::isnan(value))
            {
                valuePoints->append(timeMsec, value);
                valueLabels.append(QString("%1: %2").arg(lineSeries->name()).arg(value, 0, 'g', 6));
                valuePositions.append(QPointF(timeMsec, value));
            }
        }
    }
    
    if (valuePoints->count() > 0)
    {
        chart->addSeries(valuePoints);
        valuePoints->attachAxis(axisX);
        valuePoints->attachAxis(axisY);
        
        // Explicitly hide marker from legend
        auto legendMarkers = chart->legend()->markers(valuePoints);
        for (auto* marker : legendMarkers)
            marker->setVisible(false);
    }
    else
    {
        delete valuePoints;
        valuePoints = nullptr;
    }
    
    // Create time label at bottom of marker
    QGraphicsTextItem* timeLabel = nullptr;
    if (chartView && chartView->scene())
    {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timeMsec);
        QString timeText = dateTime.toString("HH:mm:ss.zzz");
        
        timeLabel = new QGraphicsTextItem(timeText);
        timeLabel->setDefaultTextColor(palette.color(QPalette::Text));
        QFont font = timeLabel->font();
        font.setBold(true);
        timeLabel->setFont(font);
        
        // Position below axis X labels
        QAbstractSeries* seriesForMapping = valuePoints ? static_cast<QAbstractSeries*>(valuePoints) : static_cast<QAbstractSeries*>(verticalLine);
        QPointF scenePos = chart->mapToPosition(QPointF(timeMsec, minY), seriesForMapping);
        // Position label below the axis X labels area
        QPointF labelPos(scenePos.x() - timeLabel->boundingRect().width() / 2, 
                         scenePos.y());
        
        // Constrain label position to plot area
        QRectF plotArea = chart->plotArea();
        labelPos = constrainLabelPosition(labelPos, timeLabel->boundingRect(), plotArea);
        timeLabel->setPos(labelPos);
        
        chartView->scene()->addItem(timeLabel);
    }
    
    // Create value labels near intersection points
    QList<QPointer<QGraphicsTextItem>> valueLabelsItems;
    if (chartView && chartView->scene() && valuePoints)
    {
        for (int i = 0; i < valueLabels.size() && i < valuePositions.size(); ++i)
        {
            auto* valueLabel = new QGraphicsTextItem(valueLabels[i]);
            valueLabel->setDefaultTextColor(palette.color(QPalette::Text));
            QFont font = valueLabel->font();
            font.setPointSize(font.pointSize() - 1);
            valueLabel->setFont(font);
            
            QPointF scenePos = chart->mapToPosition(valuePositions[i], valuePoints);
            QPointF labelPos(scenePos.x() + 10, scenePos.y() - 15);  // Offset to the right and up
            
            // Constrain label position to plot area
            QRectF plotArea = chart->plotArea();
            labelPos = constrainLabelPosition(labelPos, valueLabel->boundingRect(), plotArea);
            valueLabel->setPos(labelPos);
            
            chartView->scene()->addItem(valueLabel);
            valueLabelsItems.append(valueLabel);
        }
    }
    
    // Store marker
    Marker marker;
    marker.timeMsec = timeMsec;
    marker.verticalLine = verticalLine;
    marker.valuePoints = valuePoints;
    marker.valueLabels = valueLabels;
    marker.valuePositions = valuePositions;  // Store positions for updating labels
    marker.timeLabel = timeLabel;
    marker.valueLabelsItems = valueLabelsItems;
    marker.deleteButton = nullptr;  // Will be created on hover
    markers.append(marker);
    
    // Update markers to refresh display
    updateMarkers();
}

int QtPlotterFbImpl::findMarkerAtPosition(const QPointF& scenePos, qreal tolerance)
{
    if (!chart || !chartView)
        return -1;
    
    for (int i = 0; i < markers.size(); ++i)
    {
        const Marker& marker = markers[i];
        if (!marker.verticalLine)
            continue;
        
        // Convert marker time to scene position
        QAbstractSeries* seriesForMapping = marker.valuePoints ? static_cast<QAbstractSeries*>(marker.valuePoints) : static_cast<QAbstractSeries*>(marker.verticalLine);
        QPointF markerScenePos = chart->mapToPosition(QPointF(marker.timeMsec, axisY ? axisY->min() : 0), seriesForMapping);
        
        // Check if mouse is near the marker line (within tolerance)
        qreal distance = qAbs(scenePos.x() - markerScenePos.x());
        if (distance <= tolerance)
            return i;
    }
    
    return -1;
}

bool QtPlotterFbImpl::isDeleteButtonAtPosition(int markerIndex, const QPointF& scenePos)
{
    if (markerIndex < 0 || markerIndex >= markers.size())
        return false;
    
    const Marker& marker = markers[markerIndex];
    if (marker.deleteButton && marker.deleteButton->isVisible())
    {
        QRectF deleteRect = marker.deleteButton->boundingRect();
        QPointF deletePos = marker.deleteButton->pos();
        QRectF deleteArea(deletePos, deleteRect.size());
        return deleteArea.contains(scenePos);
    }
    return false;
}

void QtPlotterFbImpl::showDeleteButton(int markerIndex)
{
    if (markerIndex < 0 || markerIndex >= markers.size() || !chartView || !chartView->scene())
        return;
    
    Marker& marker = markers[markerIndex];
    
    // Create delete button if it doesn't exist
    if (!marker.deleteButton)
    {
        marker.deleteButton = new QGraphicsTextItem("✕");
        QPalette palette = QApplication::palette();
        marker.deleteButton->setDefaultTextColor(palette.color(QPalette::Highlight));
        QFont font = marker.deleteButton->font();
        font.setBold(true);
        font.setPointSize(font.pointSize() + 2);
        marker.deleteButton->setFont(font);
        marker.deleteButton->setZValue(1000);  // Make sure it's on top
        chartView->scene()->addItem(marker.deleteButton);
    }
    
    // Position delete button at top of marker
    if (marker.verticalLine && axisY)
    {
        QAbstractSeries* seriesForMapping = marker.valuePoints ? static_cast<QAbstractSeries*>(marker.valuePoints) : static_cast<QAbstractSeries*>(marker.verticalLine);
        QPointF scenePos = chart->mapToPosition(QPointF(marker.timeMsec, axisY->max()), seriesForMapping);
        marker.deleteButton->setPos(scenePos.x() - marker.deleteButton->boundingRect().width() / 2, 
                                   scenePos.y() - marker.deleteButton->boundingRect().height() - 5);
        marker.deleteButton->setVisible(true);
    }
}

void QtPlotterFbImpl::hideDeleteButtons()
{
    for (auto& marker : markers)
        if (marker.deleteButton)
            marker.deleteButton->setVisible(false);
}

void QtPlotterFbImpl::moveMarker(int index, qint64 newTimeMsec)
{
    if (index < 0 || index >= markers.size() || !axisY)
        return;

    Marker& marker = markers[index];
    marker.timeMsec = newTimeMsec;

    qreal minY = axisY->min();
    qreal maxY = axisY->max();

    // Update vertical line position (reuse existing series - no allocation)
    if (marker.verticalLine)
    {
        marker.verticalLine->clear();
        marker.verticalLine->append(newTimeMsec, minY);
        marker.verticalLine->append(newTimeMsec, maxY);
    }

    // Update value points positions
    if (marker.valuePoints)
    {
        marker.valuePoints->clear();
        marker.valuePositions.clear();
        marker.valueLabels.clear();

        // Find new intersections with signal series using signalContexts (O(signals) not O(all series))
        for (const auto& [port, sigCtx] : signalContexts)
        {
            if (!sigCtx.isSignalConnected || !sigCtx.series)
                continue;

            double value = getSignalValueAtTime(sigCtx.series, newTimeMsec);
            if (!std::isnan(value))
            {
                marker.valuePoints->append(newTimeMsec, value);
                marker.valueLabels.append(QString("%1: %2").arg(sigCtx.series->name()).arg(value, 0, 'g', 6));
                marker.valuePositions.append(QPointF(newTimeMsec, value));
            }
        }
    }

    // Update time label text and position (reuse existing item)
    if (marker.timeLabel)
    {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(newTimeMsec);
        marker.timeLabel->setPlainText(dateTime.toString("HH:mm:ss.zzz"));

        QAbstractSeries* seriesForMapping = marker.valuePoints ? static_cast<QAbstractSeries*>(marker.valuePoints) : static_cast<QAbstractSeries*>(marker.verticalLine);
        if (seriesForMapping)
        {
            QPointF scenePos = chart->mapToPosition(QPointF(newTimeMsec, minY), seriesForMapping);
            QPointF labelPos(scenePos.x() - marker.timeLabel->boundingRect().width() / 2, scenePos.y());

            QRectF plotArea = chart->plotArea();
            labelPos = constrainLabelPosition(labelPos, marker.timeLabel->boundingRect(), plotArea);
            marker.timeLabel->setPos(labelPos);
        }
    }

    // Update value labels text and positions (reuse existing items where possible)
    if (marker.valuePoints && chartView && chartView->scene())
    {
        QRectF plotArea = chart->plotArea();

        // Resize labels list to match valuePositions
        while (marker.valueLabelsItems.size() > marker.valuePositions.size())
        {
            auto item = marker.valueLabelsItems.takeLast();
            if (item)
            {
                chartView->scene()->removeItem(item);
                item->deleteLater();
            }
        }

        QPalette palette = QApplication::palette();
        while (marker.valueLabelsItems.size() < marker.valuePositions.size())
        {
            auto* valueLabel = new QGraphicsTextItem();
            valueLabel->setDefaultTextColor(palette.color(QPalette::Text));
            QFont font = valueLabel->font();
            font.setPointSize(font.pointSize() - 1);
            valueLabel->setFont(font);
            chartView->scene()->addItem(valueLabel);
            marker.valueLabelsItems.append(valueLabel);
        }

        // Update existing labels
        for (int i = 0; i < marker.valueLabelsItems.size() && i < marker.valueLabels.size(); ++i)
        {
            if (marker.valueLabelsItems[i])
            {
                marker.valueLabelsItems[i]->setPlainText(marker.valueLabels[i]);

                QPointF scenePos = chart->mapToPosition(marker.valuePositions[i], marker.valuePoints);
                QPointF labelPos(scenePos.x() + 10, scenePos.y() - 15);
                labelPos = constrainLabelPosition(labelPos, marker.valueLabelsItems[i]->boundingRect(), plotArea);
                marker.valueLabelsItems[i]->setPos(labelPos);
            }
        }
    }

    // Update delete button position if it exists
    if (marker.deleteButton)
        showDeleteButton(index);
}

void QtPlotterFbImpl::removeMarker(int index)
{
    if (index < 0 || index >= markers.size())
        return;
    
    Marker& marker = markers[index];
    
    if (marker.verticalLine)
    {
        chart->removeSeries(marker.verticalLine);
        marker.verticalLine->deleteLater();
    }
    
    if (marker.valuePoints)
    {
        chart->removeSeries(marker.valuePoints);
        marker.valuePoints->deleteLater();
    }
    
    if (marker.timeLabel && chartView && chartView->scene())
    {
        chartView->scene()->removeItem(marker.timeLabel);
        marker.timeLabel->deleteLater();
    }
    
    for (auto& valueLabel : marker.valueLabelsItems)
    {
        if (valueLabel && chartView && chartView->scene())
        {
            chartView->scene()->removeItem(valueLabel);
            valueLabel->deleteLater();
        }
    }
    
    if (marker.deleteButton && chartView && chartView->scene())
    {
        chartView->scene()->removeItem(marker.deleteButton);
        marker.deleteButton->deleteLater();
    }
    
    markers.removeAt(index);
}

void QtPlotterFbImpl::updateMarkers()
{
    if (!chart || !axisX || !axisY || !chartView)
        return;
    
    qreal minY = axisY->min();
    qreal maxY = axisY->max();
    
    // Update all markers
    for (int markerIndex = 0; markerIndex < markers.size(); ++markerIndex)
    {
        Marker& marker = markers[markerIndex];
        if (!marker.verticalLine)
            continue;
        
        // Update vertical line range
        marker.verticalLine->clear();
        marker.verticalLine->append(marker.timeMsec, minY);
        marker.verticalLine->append(marker.timeMsec, maxY);
        
        // Update time label position
        if (marker.timeLabel && chartView->scene())
        {
            // Position below axis X labels
            QAbstractSeries* seriesForMapping = marker.valuePoints ? static_cast<QAbstractSeries*>(marker.valuePoints) : static_cast<QAbstractSeries*>(marker.verticalLine);
            QPointF scenePos = chart->mapToPosition(QPointF(marker.timeMsec, minY), seriesForMapping);
            QPointF labelPos(scenePos.x() - marker.timeLabel->boundingRect().width() / 2, 
                            scenePos.y());  // Below axis X labels
            
            // Constrain label position to plot area
            QRectF plotArea = chart->plotArea();
            labelPos = constrainLabelPosition(labelPos, marker.timeLabel->boundingRect(), plotArea);
            marker.timeLabel->setPos(labelPos);
        }
        
        // Update value points if they exist
        if (marker.valuePoints)
            // Value points are already at correct positions, just ensure they're visible
            marker.valuePoints->setVisible(true);
        
        // Update value labels positions
        if (marker.valuePoints && chartView->scene())
        {
            const auto& points = marker.valuePoints->points();
            QRectF plotArea = chart->plotArea();
            for (int i = 0; i < marker.valueLabelsItems.size() && i < points.size(); ++i)
            {
                if (marker.valueLabelsItems[i])
                {
                    QPointF scenePos = chart->mapToPosition(points[i], marker.valuePoints);
                    QPointF labelPos(scenePos.x() + 10, scenePos.y() - 15);
                    
                    // Constrain label position to plot area
                    labelPos = constrainLabelPosition(labelPos, marker.valueLabelsItems[i]->boundingRect(), plotArea);
                    marker.valueLabelsItems[i]->setPos(labelPos);
                }
            }
        }
        
        // Update delete button position if visible
        if (marker.deleteButton && marker.deleteButton->isVisible())
            showDeleteButton(markerIndex);
    }
}

ErrCode QtPlotterFbImpl::getWidget(struct QWidget** widget)
{
    if (widget == nullptr)
        return OPENDAQ_ERR_ARGUMENT_NULL;

    // Recreate widget if it was deleted by parent
    if (!embeddedWidget)
        initWidget();

    if (!embeddedWidget)
        return OPENDAQ_ERR_NOTFOUND;

    *widget = embeddedWidget;
    return OPENDAQ_SUCCESS;
}

void QtPlotterFbImpl::initWidget()
{
    createChart();
    createWidget();
    setupTimer();
}

void QtPlotterFbImpl::createChart()
{
    // Don't recreate chart if it already exists
    if (chart)
        return;
    
    // Initialize chart
    chart = new QChart();
    chart->setTitle("Signal Plotter");
    chart->setAnimationOptions(QChart::NoAnimation);
    
    // Use system colors from palette
    QPalette palette = QApplication::palette();
    chart->setBackgroundBrush(palette.brush(QPalette::Base));
    chart->setTitleBrush(palette.brush(QPalette::Text));
    chart->legend()->setLabelColor(palette.color(QPalette::Text));
    chart->legend()->setVisible(showLegend);
    
    // Reduce chart margins - minimal horizontal margins for tight fit
    chart->setMargins(QMargins(0, 5, 0, 5));
    chart->layout()->setContentsMargins(0, 0, 0, 0);

    // Create axes
    axisX = new QDateTimeAxis();
    axisX->setTickCount(3);
    axisX->setFormat("HH:mm:ss.zzz");
    axisX->setLabelsVisible(true);
    axisX->setLabelsColor(palette.color(QPalette::Text));
    axisX->setGridLineVisible(showGrid);
    axisX->setTitleBrush(palette.brush(QPalette::Text));
    axisX->setTruncateLabels(true);
    QDateTime now = QDateTime::currentDateTime();
    axisX->setRange(now.addSecs(-1), now);
    chart->addAxis(axisX, Qt::AlignBottom);
    
    axisY = new QValueAxis();
    axisY->setTickCount(5);
    axisY->setLabelsColor(palette.color(QPalette::Text));
    axisY->setGridLineVisible(showGrid);
    axisY->setTitleBrush(palette.brush(QPalette::Text));
    chart->addAxis(axisY, Qt::AlignLeft);
}

void QtPlotterFbImpl::createWidget()
{
    // Don't recreate widget if it already exists and is valid
    if (embeddedWidget)
        return;
    
    // Clean up timer if it exists
    if (updateTimer)
    {
        updateTimer->stop();
        updateTimer->deleteLater();
        updateTimer = nullptr;
    }
    
    // Create widget for embedding
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Create toolbar
    auto* toolbarWidget = new QWidget(widget);
    auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    
    setupButtons(toolbarWidget);
    
    toolbarLayout->addStretch();
    layout->addWidget(toolbarWidget);
    
    // Create chart view (reuse the same chart)
    auto* embeddedChartView = new QChartView(chart, widget);
    embeddedChartView->setRenderHint(QPainter::Antialiasing);
    embeddedChartView->setRubberBand(QChartView::NoRubberBand);
    embeddedChartView->setDragMode(QGraphicsView::NoDrag);
    
    // Store reference to chart view
    chartView = embeddedChartView;
    
    // Install event filter
    embeddedChartView->viewport()->installEventFilter(new ChartEventFilter(embeddedChartView, this));
    
    layout->addWidget(embeddedChartView);
    
    embeddedWidget = widget;
    
    // Create debounce timer for visible series updates during zoom/pan
    if (!visibleUpdateTimer && axisX)
    {
        visibleUpdateTimer = new QTimer(embeddedWidget);
        visibleUpdateTimer->setSingleShot(true);
        visibleUpdateTimer->setInterval(50); // 50ms debounce delay
        
        QObject::connect(visibleUpdateTimer, &QTimer::timeout, [this]()
        {
            if (axisX)
            {
                qint64 visibleMin = pendingVisibleMin;
                qint64 visibleMax = pendingVisibleMax;
                
                // When zooming stops: restore original values
                if (isZooming)
                {
                    downsampleMethod = static_cast<QtPlotter::DownsampleMethod>(this->objPtr.getPropertyValue("DownsampleMethod").asPtr<IInteger>());
                    maxSamplesPerSeries = this->objPtr.getPropertyValue("MaxSamplesPerSeries");
                    isZooming = false;
                }
                
                // Update visible series with restored settings
                for (auto& [port, sigCtx] : signalContexts)
                {
                    if (sigCtx.isSignalConnected && sigCtx.series)
                    {
                        updateVisibleSeries(sigCtx, sigCtx.series, visibleMin, visibleMax);
                    }
                }
            }
        });
        
        // Connect axis range change to update visible series (lazy rendering with debouncing)
        QObject::connect(axisX, &QDateTimeAxis::rangeChanged, [this](QDateTime min, QDateTime max)
        {
            pendingVisibleMin = min.toMSecsSinceEpoch();
            pendingVisibleMax = max.toMSecsSinceEpoch();
            
            // During zooming: save original values and set simple downsampling with 100 max samples
            if (!isZooming)
            {
                downsampleMethod = static_cast<QtPlotter::DownsampleMethod>(this->objPtr.getPropertyValue("DownsampleMethod").asPtr<IInteger>());
                maxSamplesPerSeries = this->objPtr.getPropertyValue("MaxSamplesPerSeries");
                isZooming = true;
            }
            
            downsampleMethod = DownsampleMethod::Simple;
            maxSamplesPerSeries = 100;
            
            // Update visible series with zoom settings
            for (auto& [port, sigCtx] : signalContexts)
            {
                if (sigCtx.isSignalConnected && sigCtx.series)
                {
                    updateVisibleSeries(sigCtx, sigCtx.series, pendingVisibleMin, pendingVisibleMax);
                }
            }
            
            // Restart timer - this debounces rapid range changes
            if (visibleUpdateTimer)
                visibleUpdateTimer->start();
        });
    }
}

void QtPlotterFbImpl::setupButtons(QWidget* toolbarWidget)
{
    auto* toolbarLayout = qobject_cast<QHBoxLayout*>(toolbarWidget->layout());
    if (!toolbarLayout)
        return;
    
    auto* zoomInBtn = new QPushButton("+", toolbarWidget);
    zoomInBtn->setToolTip("Zoom In");
    zoomInBtn->setMaximumWidth(40);
    
    auto* zoomOutBtn = new QPushButton("-", toolbarWidget);
    zoomOutBtn->setToolTip("Zoom Out");
    zoomOutBtn->setMaximumWidth(40);
    
    auto* resetZoomBtn = new QPushButton("Reset", toolbarWidget);
    resetZoomBtn->setToolTip("Reset Zoom");
    resetZoomBtn->setMaximumWidth(60);
    
    auto* freezeBtn = new QPushButton("Freeze", toolbarWidget);
    freezeBtn->setToolTip("Freeze/Unfreeze plot updates");
    freezeBtn->setCheckable(true);
    freezeBtn->setMaximumWidth(70);
    
    auto* clearBtn = new QPushButton("Clear", toolbarWidget);
    clearBtn->setToolTip("Clear all plot data");
    clearBtn->setMaximumWidth(60);
    
    auto* clearMarkersBtn = new QPushButton("Clear Markers", toolbarWidget);
    clearMarkersBtn->setToolTip("Clear all markers");
    clearMarkersBtn->setMaximumWidth(120);
    
    toolbarLayout->addWidget(zoomInBtn);
    toolbarLayout->addWidget(zoomOutBtn);
    toolbarLayout->addWidget(resetZoomBtn);
    toolbarLayout->addWidget(freezeBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addWidget(clearMarkersBtn);
    
    // Connect buttons
    QObject::connect(zoomInBtn, &QPushButton::clicked, [this]()
    {
        if (chart)
        {
            userInteracting = true;
            chart->zoomIn();
        }
    });
    
    QObject::connect(zoomOutBtn, &QPushButton::clicked, [this]()
    {
        if (chart)
        {
            userInteracting = true;
            chart->zoomOut();
        }
    });
    
    QObject::connect(resetZoomBtn, &QPushButton::clicked, [this]()
    {
        if (chart)
        {
            userInteracting = false;
            chart->zoomReset();
        }
    });
    
    QObject::connect(freezeBtn, &QPushButton::toggled, [this, freezeBtn](bool checked)
    {
        this->setActive(!checked);
        if (checked)
        {
            freezeBtn->setText("Unfreeze");
            freezeBtn->setStyleSheet("background-color: #ff6b6b; color: white;");
        }
        else
        {
            freezeBtn->setText("Freeze");
            freezeBtn->setStyleSheet("");
        }
    });
    
    QObject::connect(clearBtn, &QPushButton::clicked, [this]()
    {
        if (chart)
        {
            // Clear all series
            for (auto* series : chart->series())
            {
                auto* lineSeries = qobject_cast<QLineSeries*>(series);
                if (lineSeries)
                    lineSeries->clear();
            }
            userInteracting = false;
        }
    });
    
    QObject::connect(clearMarkersBtn, &QPushButton::clicked, [this]()
    {
        // Clear all markers
        while (!markers.isEmpty())
            removeMarker(0);
    });
}

void QtPlotterFbImpl::setupTimer()
{
    if (!updateTimer)
    {
        updateTimer = new QTimer(embeddedWidget);
        QObject::connect(updateTimer, &QTimer::timeout, [this]()
        {
            // Update plot if embeddedWidget exists
            if (embeddedWidget)
                updatePlot();
        });
        updateTimer->start(40);  // Redraw at ~20 FPS
    }
}

// Lazy rendering method implementations

int QtPlotterFbImpl::binarySearchFirstGE(const QVector<QPointF>& points, qint64 targetTime) const
{
    // Binary search for first index where points[i].x() >= targetTime
    int left = 0;
    int right = points.size() - 1;
    int result = points.size(); // Default: not found, return size (out of bounds)
    
    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        qint64 midTime = static_cast<qint64>(points[mid].x());
        
        if (midTime >= targetTime)
        {
            result = mid;
            right = mid - 1; // Continue searching in left half
        }
        else
        {
            left = mid + 1; // Search in right half
        }
    }
    
    return result;
}

int QtPlotterFbImpl::binarySearchLastLE(const QVector<QPointF>& points, qint64 targetTime, int startIdx) const
{
    // Binary search for last index where points[i].x() <= targetTime
    // Search starts from startIdx (optimization: we know all points before startIdx are < targetTime)
    int left = startIdx;
    int right = points.size() - 1;
    int result = startIdx - 1; // Default: not found
    
    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        qint64 midTime = static_cast<qint64>(points[mid].x());
        
        if (midTime <= targetTime)
        {
            result = mid;
            left = mid + 1; // Continue searching in right half
        }
        else
        {
            right = mid - 1; // Search in left half
        }
    }
    
    return result;
}

std::pair<int, int> QtPlotterFbImpl::getVisibleRange(const SignalContext& sigCtx, qint64 visibleMin, qint64 visibleMax) const
{
    // Return invalid range if buffer is empty
    if (sigCtx.pointsBuffer.isEmpty())
        return std::make_pair(-1, -1);

    int startIdx = binarySearchFirstGE(sigCtx.pointsBuffer, visibleMin);
    int lastIdx = binarySearchLastLE(sigCtx.pointsBuffer, visibleMax, startIdx);

    // Include nearest invisible point on the left side (if exists) to not lose left boundary point
    if (startIdx > 0 && startIdx < sigCtx.pointsBuffer.size())
        startIdx--; // Include previous point to not lose left boundary
    
    // Include nearest invisible point on the right side (if exists) to not lose right boundary point
    if (lastIdx >= 0 && lastIdx + 1 < sigCtx.pointsBuffer.size())
        lastIdx++; // Include next point to not lose right boundary
    
    // Check if we found valid range
    if (startIdx > lastIdx || startIdx >= sigCtx.pointsBuffer.size() || lastIdx < 0)
        return std::make_pair(-1, -1);
    
    // Ensure indices are within bounds
    if (startIdx < 0)
        startIdx = 0;
    if (lastIdx >= sigCtx.pointsBuffer.size())
        lastIdx = sigCtx.pointsBuffer.size() - 1;
    
    return std::make_pair(startIdx, lastIdx);
}

void QtPlotterFbImpl::updateVisibleSeries(SignalContext& sigCtx, QLineSeries* series, qint64 visibleMin, qint64 visibleMax)
{
    if (!series || sigCtx.pointsBuffer.isEmpty())
        return;
    
    // Get visible range indices from history buffer
    auto [beginIdx, endIdx] = getVisibleRange(sigCtx, visibleMin, visibleMax);
    
    if (beginIdx < 0 || endIdx < 0 || beginIdx > endIdx)
    {
        series->clear();
        return;
    }
    
    // Calculate visible count
    size_t visibleCount = endIdx - beginIdx + 1;
    QVector<QPointF> pointsToShow;
    
    // Reserve space for better performance
    size_t targetSize = std::min(visibleCount, maxSamplesPerSeries);
    pointsToShow.reserve(targetSize);
    
    if (visibleCount > maxSamplesPerSeries && downsampleMethod != DownsampleMethod::None)
    {
        size_t targetPoints = maxSamplesPerSeries;
        
        switch (downsampleMethod)
        {
            case DownsampleMethod::None:
                pointsToShow = downsampleVisibleNone(sigCtx.pointsBuffer, beginIdx, endIdx);
                break;
            case DownsampleMethod::Simple:
                pointsToShow = downsampleVisibleSimple(sigCtx.pointsBuffer, beginIdx, endIdx, targetPoints);
                break;
            case DownsampleMethod::MinMax:
                pointsToShow = downsampleVisibleMinMax(sigCtx.pointsBuffer, beginIdx, endIdx, targetPoints);
                break;
            case DownsampleMethod::LTTB:
                pointsToShow = downsampleVisibleLTTB(sigCtx.pointsBuffer, beginIdx, endIdx, targetPoints);
                break;
            default:
                // Copy all visible points
                pointsToShow.reserve(visibleCount);
                for (int i = beginIdx; i <= endIdx; ++i)
                    pointsToShow.append(sigCtx.pointsBuffer[i]);
                break;
        }
    }
    else
    {
        // No downsampling needed or method is None - copy all visible points
        pointsToShow.reserve(visibleCount);
        for (int i = beginIdx; i <= endIdx; ++i)
            pointsToShow.append(sigCtx.pointsBuffer[i]);
    }
    
    // Update series with downsampled visible points
    series->replace(pointsToShow);
}

QVector<QPointF> QtPlotterFbImpl::downsampleVisibleNone(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx) const
{
    // No downsampling - return all points in range
    QVector<QPointF> result;
    result.reserve(endIdx - beginIdx + 1);
    for (int i = beginIdx; i <= endIdx; ++i)
        result.append(pointsBuffer[i]);
    return result;
}

QVector<QPointF> QtPlotterFbImpl::downsampleVisibleSimple(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx, size_t targetPoints) const
{
    // Simple downsampling - take every Nth point
    QVector<QPointF> result;
    result.reserve(targetPoints);
    
    if (beginIdx < 0 || endIdx < 0 || beginIdx > endIdx || targetPoints == 0)
        return result;
    
    size_t visibleCount = endIdx - beginIdx + 1;
    if (visibleCount <= targetPoints)
    {
        // Return all points
        for (int i = beginIdx; i <= endIdx; ++i)
            result.append(pointsBuffer[i]);
        return result;
    }
    
    size_t step = visibleCount / targetPoints;
    if (step < 1) step = 1;
    
    for (int i = beginIdx; i <= endIdx && result.size() < targetPoints; i += static_cast<int>(step))
    {
        result.append(pointsBuffer[i]);
    }
    
    return result;
}

QVector<QPointF> QtPlotterFbImpl::downsampleVisibleMinMax(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx, size_t targetPoints) const
{
    // Min-Max downsampling - preserves signal peaks and valleys
    // Each bucket produces 2 points (min and max)
    QVector<QPointF> result;
    
    if (beginIdx < 0 || endIdx < 0 || beginIdx > endIdx || targetPoints == 0)
        return result;
    
    size_t visibleCount = endIdx - beginIdx + 1;
    if (visibleCount <= targetPoints)
    {
        // Return all points
        for (int i = beginIdx; i <= endIdx; ++i)
            result.append(pointsBuffer[i]);
        return result;
    }
    
    size_t bucketSize = visibleCount / (targetPoints / 2);  // /2 because each bucket adds 2 points
    if (bucketSize < 2) bucketSize = 2;
    
    // Reserve estimated space (each bucket adds up to 2 points)
    size_t estimatedPoints = (visibleCount / bucketSize) * 2 + 2;
    result.reserve(std::min(estimatedPoints, targetPoints));
    
    for (int bucketStart = beginIdx; bucketStart <= endIdx && result.size() < targetPoints - 1; bucketStart += static_cast<int>(bucketSize))
    {
        int bucketEnd = std::min(bucketStart + static_cast<int>(bucketSize) - 1, endIdx);
        
        // Find min and max in bucket
        double minVal = pointsBuffer[bucketStart].y(), maxVal = pointsBuffer[bucketStart].y();
        int minIdx = bucketStart, maxIdx = bucketStart;
        
        for (int i = bucketStart + 1; i <= bucketEnd; ++i)
        {
            if (pointsBuffer[i].y() < minVal) { minVal = pointsBuffer[i].y(); minIdx = i; }
            if (pointsBuffer[i].y() > maxVal) { maxVal = pointsBuffer[i].y(); maxIdx = i; }
        }
        
        // Add points in time order (important for correct line rendering)
        if (minIdx < maxIdx)
        {
            result.append(pointsBuffer[minIdx]);
            if (minIdx != maxIdx && result.size() < targetPoints)
                result.append(pointsBuffer[maxIdx]);
        }
        else
        {
            result.append(pointsBuffer[maxIdx]);
            if (result.size() < targetPoints)
                result.append(pointsBuffer[minIdx]);
        }
    }
    
    return result;
}

QVector<QPointF> QtPlotterFbImpl::downsampleVisibleLTTB(const QVector<QPointF>& pointsBuffer, int beginIdx, int endIdx, size_t targetPoints) const
{
    // Largest Triangle Three Buckets (LTTB) - best visual quality downsampling
    // Algorithm: https://github.com/sveinn-steinarsson/flot-downsample
    QVector<QPointF> result;
    
    if (beginIdx < 0 || endIdx < 0 || beginIdx > endIdx || targetPoints == 0)
        return result;
    
    size_t count = endIdx - beginIdx + 1;
    if (count <= targetPoints)
    {
        // Return all points
        for (int i = beginIdx; i <= endIdx; ++i)
            result.append(pointsBuffer[i]);
        return result;
    }
    
    if (targetPoints < 3)
    {
        // Need at least 3 points for LTTB, fall back to simple
        return downsampleVisibleSimple(pointsBuffer, beginIdx, endIdx, targetPoints);
    }
    
    result.reserve(targetPoints);
    
    // Always include first point
    result.append(pointsBuffer[beginIdx]);
    
    // Calculate bucket size
    size_t bucketSize = (count - 2) / (targetPoints - 2);  // -2 for first and last points
    if (bucketSize < 1) bucketSize = 1;
    
    int a = beginIdx;  // Index of first point in current bucket (absolute index in pointsBuffer)
    
    for (size_t i = 0; i < targetPoints - 2; ++i)
    {
        // Calculate range for next bucket (relative to beginIdx)
        size_t rangeStartRel = (i + 1) * bucketSize;
        size_t rangeEndRel = std::min((i + 2) * bucketSize, count - 1);
        int rangeStart = beginIdx + static_cast<int>(rangeStartRel);
        int rangeEnd = beginIdx + static_cast<int>(rangeEndRel);
        
        // Calculate average point of next bucket for triangle comparison
        double avgX = 0.0, avgY = 0.0;
        size_t avgCount = 0;
        for (int j = rangeStart; j <= rangeEnd; ++j)
        {
            avgX += pointsBuffer[j].x();
            avgY += pointsBuffer[j].y();
            avgCount++;
        }
        if (avgCount > 0)
        {
            avgX /= avgCount;
            avgY /= avgCount;
        }
        
        // Calculate range for current bucket (from a+1 to rangeStart)
        int rangeStartCurr = a + 1;
        int rangeEndCurr = std::min(rangeStart - 1, endIdx);
        
        // Find point in current bucket that forms largest triangle with avg point
        double maxArea = -1.0;
        int maxAreaIdx = rangeStartCurr;
        
        if (rangeStartCurr <= rangeEndCurr)
        {
            double timeA = pointsBuffer[a].x();
            double valA = pointsBuffer[a].y();
            
            for (int j = rangeStartCurr; j <= rangeEndCurr; ++j)
            {
                double timeJ = pointsBuffer[j].x();
                double valJ = pointsBuffer[j].y();
                
                // Calculate triangle area using cross product
                // Area = |(avgX - timeA) * (valJ - valA) - (timeJ - timeA) * (avgY - valA)| / 2
                double area = std::abs((avgX - timeA) * (valJ - valA) - (timeJ - timeA) * (avgY - valA));
                
                if (area > maxArea)
                {
                    maxArea = area;
                    maxAreaIdx = j;
                }
            }
        }
        
        // Add the point with largest triangle area
        if (maxAreaIdx < beginIdx || maxAreaIdx > endIdx)
            maxAreaIdx = rangeStart - 1;
        
        if (maxAreaIdx >= beginIdx && maxAreaIdx <= endIdx)
        {
            result.append(pointsBuffer[maxAreaIdx]);
            a = maxAreaIdx;
        }
        else
        {
            a = rangeStart;
        }
    }
    
    // Always include last point
    result.append(pointsBuffer[endIdx]);
    
    return result;
}

}  // namespace QtPlotter

END_NAMESPACE_OPENDAQ_QT_MODULE
