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
#include <limits>

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
                    qint64 currentTimeRange = maxTime.toMSecsSinceEpoch() - minTime.toMSecsSinceEpoch();
                    qreal timeRatio = currentTimeRange > 0 ? 
                        static_cast<qreal>(mouseTime - minTime.toMSecsSinceEpoch()) / static_cast<qreal>(currentTimeRange) : 0.5;
                    
                    qreal currentYRange = maxY - minY;
                    qreal yRatio = currentYRange > 0 ? (mouseY - minY) / currentYRange : 0.5;
                    
                    // Calculate new ranges
                    qint64 newTimeRange = static_cast<qint64>(currentTimeRange / zoomFactor);
                    qreal newYRange = currentYRange / zoomFactor;
                    
                    // Keep mouse position fixed - adjust min/max based on mouse position ratio
                    qint64 newMinTime = mouseTime - static_cast<qint64>(newTimeRange * timeRatio);
                    qint64 newMaxTime = mouseTime + static_cast<qint64>(newTimeRange * (1.0 - timeRatio));
                    
                    qreal newMinY = mouseY - newYRange * yRatio;
                    qreal newMaxY = mouseY + newYRange * (1.0 - yRatio);
                        
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
    , showLegend(true)
    , autoScale(true)
    , showGrid(true)
    , showLastValue(false)
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
    pointsBuffer.reserve(200);
    
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
        propertyChanged();
    };

    const auto durationProp = daq::FloatPropertyBuilder("Duration", 1.0)
                                  .setSuggestedValues(daq::List<daq::Float>(0.1, 0.5, 1.0, 5.0, 10.0))
                                  .setUnit(daq::Unit("s", -1, "second", "time"))
                                  .build();
    objPtr.addProperty(durationProp);
    objPtr.getOnPropertyValueWrite("Duration") += onPropertyValueWrite;


    const auto showLegendProp = daq::BoolProperty("ShowLegend", true);
    objPtr.addProperty(showLegendProp);
    objPtr.getOnPropertyValueWrite("ShowLegend") += onPropertyValueWrite;

    const auto autoScaleProp = daq::BoolProperty("AutoScale", true);
    objPtr.addProperty(autoScaleProp);
    objPtr.getOnPropertyValueWrite("AutoScale") += onPropertyValueWrite;

    const auto showGridProp = daq::BoolProperty("ShowGrid", true);
    objPtr.addProperty(showGridProp);
    objPtr.getOnPropertyValueWrite("ShowGrid") += onPropertyValueWrite;

    const auto showLastValueProp = daq::BoolProperty("ShowLastValue", false);
    objPtr.addProperty(showLastValueProp);
    objPtr.getOnPropertyValueWrite("ShowLastValue") += onPropertyValueWrite;

    const auto autoClearProp = daq::BoolProperty("AutoClear", true);
    objPtr.addProperty(autoClearProp);
    objPtr.getOnPropertyValueWrite("AutoClear") += onPropertyValueWrite;

    readProperties();
}

void QtPlotterFbImpl::propertyChanged()
{
    readProperties();

    // Update chart properties
    if (chart)
        chart->legend()->setVisible(showLegend);

    if (axisX)
        axisX->setGridLineVisible(showGrid);

    if (axisY)
        axisY->setGridLineVisible(showGrid);
}

void QtPlotterFbImpl::readProperties()
{
    duration = objPtr.getPropertyValue("Duration");
    showLegend = objPtr.getPropertyValue("ShowLegend");
    autoScale = objPtr.getPropertyValue("AutoScale");
    showGrid = objPtr.getPropertyValue("ShowGrid");
    showLastValue = objPtr.getPropertyValue("ShowLastValue");
    autoClear = objPtr.getPropertyValue("AutoClear");

    LOG_W("Properties: Duration={}, ShowLegend={}, AutoScale={}, ShowGrid={}, ShowLastValue={}",
          duration, showLegend, autoScale, showGrid, showLastValue)
}

void QtPlotterFbImpl::updateInputPorts()
{   
    const auto inputPort = createAndAddInputPort(
        fmt::format("Input{}", inputPortCount++),
        daq::PacketReadyNotification::SameThread);
    auto [it, success] = signalContexts.emplace(inputPort, inputPort);
    it->second.streamReader.setExternalListener(this->template borrowPtr<InputPortNotificationsPtr>());
}

void QtPlotterFbImpl::onConnected(const daq::InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();

    updateInputPorts();
    LOG_W("Connected to port {}", inputPort.getLocalId());
}

void QtPlotterFbImpl::onDisconnected(const daq::InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();

    // Find and remove the series associated with this port
    if (autoClear && chart)
    {
        auto it = signalContexts.find(inputPort);
        if (it != signalContexts.end())
        {
            QString seriesName = QString::fromStdString(it->second.caption);

            // Find the series with this name and remove it
            for (auto* series : chart->series())
            {
                auto* lineSeries = qobject_cast<QLineSeries*>(series);
                if (lineSeries && !lineSeries->name().isEmpty())
                {
                    // Check if series name matches (handle case where last value is appended)
                    QString currentName = lineSeries->name();
                    if (currentName == seriesName || currentName.startsWith(seriesName + " ="))
                    {
                        chart->removeSeries(lineSeries);
                        lineSeries->deleteLater();
                        LOG_W("Removed series '{}' for disconnected port {}",
                              seriesName.toStdString(), inputPort.getLocalId());
                        break;
                    }
                }
            }
        }
    }

    signalContexts.erase(inputPort);
    removeInputPort(inputPort);
    LOG_W("Disconnected from port {}", inputPort.getLocalId());
}

void QtPlotterFbImpl::updatePlot()
{
    // Update plot if chart exists and embeddedWidget is available
    if (!chart || !embeddedWidget)
        return;

    qint64 globalLatestTime = 0;
    bool hasData = false;

    // For each valid signal, create or update series
    size_t seriesIndex = 0;
    auto seriesList = chart->series();

    for (auto& [port, sigCtx] : signalContexts)
    {
        if (!port.getSignal().assigned())
            continue;

        // Get or create series for this signal
        // Skip marker series (they have empty names and are in markers list)
        QLineSeries* series = nullptr;
        size_t validSeriesIndex = 0;
        for (int i = 0; i < seriesList.size(); ++i)
        {
            auto* lineSeries = qobject_cast<QLineSeries*>(seriesList[i]);
            if (lineSeries)
            {
                // Check if this is a marker series
                bool isMarker = false;
                for (const auto& marker : markers)
                {
                    if (marker.verticalLine == lineSeries)
                    {
                        isMarker = true;
                        break;
                    }
                }
                
                if (!isMarker && !lineSeries->name().isEmpty())
                {
                    if (validSeriesIndex == seriesIndex)
                    {
                        series = lineSeries;
                        break;
                    }
                    validSeriesIndex++;
                }
            }
        }
        
        if (!series)
        {
            series = new QLineSeries();
            series->setName(QString::fromStdString(sigCtx.caption));

            // Colors for different signals
            QColor colors[] = {
                QColor(255, 0, 0),      // Red
                QColor(255, 255, 0),    // Yellow
                QColor(0, 255, 0),      // Green
                QColor(255, 0, 255),    // Magenta
                QColor(0, 255, 255),    // Cyan
                QColor(0, 0, 255)       // Blue
            };
            QPen pen(colors[seriesIndex % 6], 2);
            series->setPen(pen);

            chart->addSeries(series);
            series->attachAxis(axisX);
            series->attachAxis(axisY);
        }

        // Extract and plot data using TimeReader
        if (sigCtx.timeReader.assigned())
        {
            try
            {
                size_t count = sigCtx.timeReader.getAvailableCount();

                count = std::min(count, samples.size());
                daq::ReaderStatusPtr status;
                sigCtx.timeReader.readWithDomain(samples.data(), timeStamps.data(), &count, 0, &status);
                if (status.assigned())
                {
                    auto eventPacket = status.getEventPacket();
                    if (eventPacket.assigned() && (eventPacket.getEventId() == event_packet_id::DATA_DESCRIPTOR_CHANGED))
                    {
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
                        }
                    }
                }
                
                if (count > 0)
                {
                    qint64 latestTime = 0;
                    
                    // Use count() instead of points().size() to avoid expensive copy
                    int currentPointCount = series->count();
                    
                    // Proactively remove points if we're near the limit (before adding new ones)
                    if (currentPointCount >= static_cast<int>(MAX_POINTS_PER_SERIES * 0.8))
                    {
                        // Remove enough points to make room for new data
                        int targetCount = static_cast<int>(MAX_POINTS_PER_SERIES * 0.6);
                        int removeCount = currentPointCount - targetCount;
                        if (removeCount > 0)
                            series->removePoints(0, removeCount);
                        currentPointCount = targetCount;
                    }
                    
                    // Downsample if adding would exceed threshold
                    size_t pointsToAdd = count;
                    if (currentPointCount + static_cast<int>(count) > static_cast<int>(DOWNSAMPLE_THRESHOLD))
                    {
                        // Calculate downsampling step
                        size_t totalAfterAdd = currentPointCount + count;
                        size_t step = (totalAfterAdd / DOWNSAMPLE_THRESHOLD) + 1;
                        pointsToAdd = (count + step - 1) / step;  // Ceiling division
                    }
                    
                    // Reuse buffer, resize if needed
                    pointsBuffer.resize(pointsToAdd);

                    size_t bufferIndex = 0;
                    size_t step = (pointsToAdd < count) ? (count / pointsToAdd) : 1;
                    
                    for (daq::SizeT i = 0; i < count && bufferIndex < pointsToAdd; i += step)
                    {
                        auto timeMsec = std::chrono::duration_cast<std::chrono::milliseconds>(timeStamps[i].time_since_epoch()).count();
                        pointsBuffer[bufferIndex++] = QPointF(timeMsec, samples[i]);
                        if (timeMsec > latestTime)
                            latestTime = timeMsec;
                    }
                    
                    // Adjust size if we got fewer points
                    if (bufferIndex < pointsToAdd)
                        pointsBuffer.resize(bufferIndex);

                    if (bufferIndex > 0)
                    {
                        series->append(pointsBuffer);

                        // Final check using count() instead of points().size()
                        currentPointCount = series->count();
                        if (currentPointCount > static_cast<int>(MAX_POINTS_PER_SERIES))
                        {
                            int removeCount = currentPointCount - static_cast<int>(MAX_POINTS_PER_SERIES);
                            series->removePoints(0, removeCount);
                        }
                    }

                    // Store last value from the most recent sample
                    sigCtx.lastValue = samples[count - 1];
                    sigCtx.hasLastValue = true;

                    if (latestTime > globalLatestTime)
                        globalLatestTime = latestTime;

                    hasData = true;
                }
            }
            catch (const std::exception& e)
            {
                LOG_W("Error reading data from TimeReader: {}", e.what())
            }
        }

        // Update series name with last value if enabled
        if (series)
        {
            std::string seriesName = sigCtx.caption;
            if (showLastValue && sigCtx.hasLastValue)
                seriesName += fmt::format(" = {:.6g}", sigCtx.lastValue);
            if (series->name() != QString::fromStdString(seriesName))
                series->setName(QString::fromStdString(seriesName));
        }

        seriesIndex++;
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
            
            // Check if any series has data in the visible range
            bool hasDataInRange = false;
            for (auto* series : chart->series())
            {
                auto* lineSeries = qobject_cast<QLineSeries*>(series);
                if (lineSeries)
                {
                    for (const auto& point : lineSeries->points())
                    {
                        qint64 pointTime = static_cast<qint64>(point.x());
                        if (pointTime >= visibleMin && pointTime <= visibleMax)
                        {
                            hasDataInRange = true;
                            break;
                        }
                    }
                    if (hasDataInRange)
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
        if (minTime.isValid() && maxTime.isValid() && minTime < maxTime)
        {
            axisX->setRange(minTime, maxTime);
            axisX->setTickCount(3);
            axisX->setLabelsVisible(true);
            axisX->setFormat("yyyy-MM-dd HH:mm:ss.zzz");
        }
    }

    // Auto-scale Y-axis if enabled
    if (autoScale && axisY && hasData)
    {
        qreal minY = std::numeric_limits<qreal>::max();
        qreal maxY = std::numeric_limits<qreal>::lowest();

        for (auto* series : chart->series())
        {
            auto* lineSeries = qobject_cast<QLineSeries*>(series);
            if (lineSeries)
            {
                // Skip marker series (they have empty names)
                if (lineSeries->name().isEmpty())
                    continue;
                    
                for (const auto& point : lineSeries->points())
                {
                    if (point.y() < minY) minY = point.y();
                    if (point.y() > maxY) maxY = point.y();
                }
            }
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
    
    // Find the two points that bracket the time
    for (int i = 0; i < points.size() - 1; ++i)
    {
        qint64 t1 = static_cast<qint64>(points[i].x());
        qint64 t2 = static_cast<qint64>(points[i + 1].x());
        
        if (timeMsec >= t1 && timeMsec <= t2)
        {
            // Linear interpolation
            double ratio = static_cast<double>(timeMsec - t1) / static_cast<double>(t2 - t1);
            return points[i].y() + ratio * (points[i + 1].y() - points[i].y());
        }
    }
    
    // If time is before first point or after last point, return closest value
    if (timeMsec <= static_cast<qint64>(points.first().x()))
        return points.first().y();
    if (timeMsec >= static_cast<qint64>(points.last().x()))
        return points.last().y();
    
    return std::numeric_limits<double>::quiet_NaN();
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
    // verticalLine->setName("");  // Don't show in legend
    
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
    // valuePoints->setName("");  // Don't show in legend
    
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
    if (index < 0 || index >= markers.size())
        return;
    
    Marker& marker = markers[index];
    marker.timeMsec = newTimeMsec;
    
    // Remove old series
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
    
    // Remove old labels
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
    marker.valueLabelsItems.clear();
    
    // Recreate marker at new position
    qreal minY = axisY->min();
    qreal maxY = axisY->max();
    
    // Create new vertical line
    auto* verticalLine = new QLineSeries();
    verticalLine->append(newTimeMsec, minY);
    verticalLine->append(newTimeMsec, maxY);
    
    QPalette palette = QApplication::palette();
    QPen pen(palette.color(QPalette::Highlight), 2, Qt::DashLine);
    verticalLine->setPen(pen);
    // verticalLine->setName("");
    
    chart->addSeries(verticalLine);
    verticalLine->attachAxis(axisX);
    verticalLine->attachAxis(axisY);
    
    // Explicitly hide marker from legend
    auto legendMarkers = chart->legend()->markers(verticalLine);
    for (auto* marker : legendMarkers)
        marker->setVisible(false);
    
    marker.verticalLine = verticalLine;
    
    // Create scatter series for value points
    auto* valuePoints = new QScatterSeries();
    valuePoints->setMarkerSize(8);
    valuePoints->setColor(palette.color(QPalette::Highlight));
    valuePoints->setBorderColor(palette.color(QPalette::Base));
    // valuePoints->setName("");  // Don't show in legend
    
    // Find intersections with all signal series
    QStringList valueLabels;
    QList<QPointF> valuePositions;
    for (auto* series : chart->series())
    {
        auto* lineSeries = qobject_cast<QLineSeries*>(series);
        if (lineSeries && lineSeries->count() > 0 && !lineSeries->name().isEmpty() && lineSeries != verticalLine)
        {
            double value = getSignalValueAtTime(lineSeries, newTimeMsec);
            if (!std::isnan(value))
            {
                valuePoints->append(newTimeMsec, value);
                valueLabels.append(QString("%1: %2").arg(lineSeries->name()).arg(value, 0, 'g', 6));
                valuePositions.append(QPointF(newTimeMsec, value));
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
    
    marker.valuePoints = valuePoints;
    marker.valueLabels = valueLabels;
    marker.valuePositions = valuePositions;  // Store positions for updating labels
    
    // Create time label
    if (chartView && chartView->scene())
    {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(newTimeMsec);
        QString timeText = dateTime.toString("HH:mm:ss.zzz");
        
        marker.timeLabel = new QGraphicsTextItem(timeText);
        marker.timeLabel->setDefaultTextColor(palette.color(QPalette::Text));
        QFont font = marker.timeLabel->font();
        font.setBold(true);
        marker.timeLabel->setFont(font);
        
        // Position below axis X labels
        QAbstractSeries* seriesForMapping = valuePoints ? static_cast<QAbstractSeries*>(valuePoints) : static_cast<QAbstractSeries*>(verticalLine);
        QPointF scenePos = chart->mapToPosition(QPointF(newTimeMsec, minY), seriesForMapping);
        QPointF labelPos(scenePos.x() - marker.timeLabel->boundingRect().width() / 2, 
                         scenePos.y());  // Below axis X labels
        
        // Constrain label position to plot area
        QRectF plotArea = chart->plotArea();
        labelPos = constrainLabelPosition(labelPos, marker.timeLabel->boundingRect(), plotArea);
        marker.timeLabel->setPos(labelPos);
        
        chartView->scene()->addItem(marker.timeLabel);
    }
    
    // Create value labels
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
            QPointF labelPos(scenePos.x() + 10, scenePos.y() - 15);
            
            // Constrain label position to plot area
            QRectF plotArea = chart->plotArea();
            labelPos = constrainLabelPosition(labelPos, valueLabel->boundingRect(), plotArea);
            valueLabel->setPos(labelPos);
            
            chartView->scene()->addItem(valueLabel);
            marker.valueLabelsItems.append(valueLabel);
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
    
    // Reduce chart margins
    chart->setMargins(QMargins(5, 5, 5, 5));
    
    // Create axes
    axisX = new QDateTimeAxis();
    axisX->setTickCount(3);
    axisX->setFormat("yyyy-MM-dd HH:mm:ss.zzz");
    axisX->setLabelsVisible(true);
    axisX->setLabelsColor(palette.color(QPalette::Text));
    axisX->setGridLineVisible(showGrid);
    axisX->setTitleBrush(palette.brush(QPalette::Text));
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

}  // namespace QtPlotter

END_NAMESPACE_OPENDAQ_QT_MODULE
