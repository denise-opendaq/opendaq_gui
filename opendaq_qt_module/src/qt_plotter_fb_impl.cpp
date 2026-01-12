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
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QDateTime>
#include <QMouseEvent>
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
{}

bool ChartEventFilter::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        m_plotter->userInteracting = true;

        if (mouseEvent->button() == Qt::LeftButton)
        {
            // Zoom in at mouse position
            m_chartView->chart()->zoomIn();
            return true;
        }
        else if (mouseEvent->button() == Qt::RightButton)
        {
            // Zoom out at mouse position
            m_chartView->chart()->zoomOut();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            m_isPanning = true;
            m_lastMousePos = mouseEvent->pos();
            m_plotter->userInteracting = true;
            m_chartView->setCursor(Qt::ClosedHandCursor);
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove && m_isPanning)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint delta = mouseEvent->pos() - m_lastMousePos;
        m_lastMousePos = mouseEvent->pos();

        // Scroll the chart
        m_chartView->chart()->scroll(-delta.x(), delta.y());
        return true;
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_isPanning)
        {
            m_isPanning = false;
            m_chartView->setCursor(Qt::ArrowCursor);
            return true;
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
    
    // Timer will be created when embeddedWidget is created
}

QtPlotterFbImpl::~QtPlotterFbImpl()
{
    // Timer and embedded widget will be deleted automatically when parent is deleted
    // (timer is a child of embeddedWidget, so it will be cleaned up automatically)
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

void QtPlotterFbImpl::removed()
{
    // Embedded widget will be deleted automatically when parent is deleted
    FunctionBlockImpl::removed();
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

    // Plot window functionality removed - widget is now only available as embedded tab

    readProperties();
}

void QtPlotterFbImpl::propertyChanged()
{
    readProperties();

    // Update chart properties
    if (chart)
    {
        chart->legend()->setVisible(showLegend);
    }

    if (axisX)
    {
        axisX->setGridLineVisible(showGrid);
    }

    if (axisY)
    {
        axisY->setGridLineVisible(showGrid);
    }
}

void QtPlotterFbImpl::readProperties()
{
    duration = objPtr.getPropertyValue("Duration");
    showLegend = objPtr.getPropertyValue("ShowLegend");
    autoScale = objPtr.getPropertyValue("AutoScale");
    showGrid = objPtr.getPropertyValue("ShowGrid");

    LOG_T("Properties: Duration={}, ShowLegend={}, AutoScale={}, ShowGrid={}",
          duration, showLegend, autoScale, showGrid)
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

    subscribeToSignalCoreEvent(inputPort.getSignal());
    updateInputPorts();
    LOG_T("Connected to port {}", inputPort.getLocalId());
}

void QtPlotterFbImpl::onDisconnected(const daq::InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();

    signalContexts.erase(inputPort);
    removeInputPort(inputPort);
    LOG_T("Disconnected from port {}", inputPort.getLocalId());
}

// openPlotWindow() and closePlotWindow() removed - only embedded widget is used now

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
        QLineSeries* series = nullptr;
        if (seriesIndex < static_cast<size_t>(seriesList.size()))
        {
            series = qobject_cast<QLineSeries*>(seriesList[seriesIndex]);
        }
        else
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
                        // Update series name if caption has changed
                        if (series && series->name() != QString::fromStdString(sigCtx.caption))
                        {
                            series->setName(QString::fromStdString(sigCtx.caption));
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
            axisY->setTickCount(3);
        }
    }
}

void QtPlotterFbImpl::subscribeToSignalCoreEvent(const daq::SignalPtr& signal)
{
    signal.getOnComponentCoreEvent() += daq::event(this, &QtPlotterFbImpl::processCoreEvent);
}

void QtPlotterFbImpl::processCoreEvent(daq::ComponentPtr& component, daq::CoreEventArgsPtr& args)
{
    if (args.getEventId() == static_cast<daq::Int>(daq::CoreEventId::AttributeChanged))
    {
        // Handle attribute changes if needed
    }
}

ErrCode QtPlotterFbImpl::getWidget(struct QWidget** widget)
{
    if (widget == nullptr)
        return OPENDAQ_ERR_ARGUMENT_NULL;

    // Get the embedded widget (for tabs) or create it if needed
    QWidget* qtWidget = getQtWidget();
    if (!qtWidget)
        return OPENDAQ_ERR_NOTFOUND;

    *widget = qtWidget;
    return OPENDAQ_SUCCESS;
}

QWidget* QtPlotterFbImpl::getQtWidget()
{
    // Create embedded widget if it doesn't exist
    if (!embeddedWidget)
    {
        // Ensure chart is created
        if (!chart)
        {
            // Initialize chart if not already done
            chart = new QChart();
            chart->setTitle("Signal Plotter");
            chart->setAnimationOptions(QChart::NoAnimation);
            
            // Setup dark theme
            chart->setBackgroundBrush(QBrush(QColor(43, 43, 43)));
            chart->setTitleBrush(QBrush(QColor(200, 200, 200)));
            chart->legend()->setLabelColor(QColor(200, 200, 200));
            chart->legend()->setVisible(showLegend);
            
            // Create axes
            axisX = new QDateTimeAxis();
            axisX->setTitleText("Time");
            axisX->setTickCount(3);
            axisX->setFormat("yyyy-MM-dd HH:mm:ss.zzz");
            axisX->setLabelsVisible(true);
            axisX->setLabelsColor(QColor(150, 150, 150));
            axisX->setGridLineColor(QColor(100, 100, 100));
            axisX->setGridLineVisible(showGrid);
            axisX->setTitleBrush(QBrush(QColor(150, 150, 150)));
            QDateTime now = QDateTime::currentDateTime();
            axisX->setRange(now.addSecs(-1), now);
            chart->addAxis(axisX, Qt::AlignBottom);
            
            axisY = new QValueAxis();
            axisY->setTitleText("Value");
            axisY->setTickCount(3);
            axisY->setLabelsColor(QColor(150, 150, 150));
            axisY->setGridLineColor(QColor(100, 100, 100));
            axisY->setGridLineVisible(showGrid);
            axisY->setTitleBrush(QBrush(QColor(150, 150, 150)));
            chart->addAxis(axisY, Qt::AlignLeft);
        }
        
        // Create widget for embedding
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        
        // Create toolbar
        auto* toolbarWidget = new QWidget(widget);
        auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
        toolbarLayout->setContentsMargins(5, 5, 5, 5);
        
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
        
        toolbarLayout->addWidget(zoomInBtn);
        toolbarLayout->addWidget(zoomOutBtn);
        toolbarLayout->addWidget(resetZoomBtn);
        toolbarLayout->addWidget(freezeBtn);
        toolbarLayout->addStretch();
        
        layout->addWidget(toolbarWidget);
        
        // Create chart view (reuse the same chart)
        auto* embeddedChartView = new QChartView(chart, widget);
        embeddedChartView->setRenderHint(QPainter::Antialiasing);
        embeddedChartView->setRubberBand(QChartView::NoRubberBand);
        embeddedChartView->setDragMode(QGraphicsView::NoDrag);
        
        // Install event filter
        embeddedChartView->viewport()->installEventFilter(new ChartEventFilter(embeddedChartView, this));
        
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
        
        layout->addWidget(embeddedChartView);
        
        embeddedWidget = widget;
        
        // Create update timer with embeddedWidget as parent
        if (!updateTimer)
        {
            updateTimer = new QTimer(embeddedWidget);
            QObject::connect(updateTimer, &QTimer::timeout, [this]()
            {
                // Update plot if embeddedWidget exists
                if (embeddedWidget)
                {
                    updatePlot();
                }
            });
            updateTimer->start(40);  // Redraw at ~20 FPS
        }
    }
    
    return embeddedWidget;
}

}  // namespace QtPlotter

END_NAMESPACE_OPENDAQ_QT_MODULE
