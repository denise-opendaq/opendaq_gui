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
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_factory.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QTimer>
#include <opendaq_qt_module/qcustomplot_wrapper.h>

BEGIN_NAMESPACE_OPENDAQ_QT_MODULE

namespace QtPlotter
{

QtPlotterFbImpl::QtPlotterFbImpl(const daq::ContextPtr& ctx,
                                 const daq::ComponentPtr& parent,
                                 const daq::StringPtr& localId,
                                 const daq::PropertyObjectPtr& config)
    : FunctionBlock(CreateType(), ctx, parent, localId)
    , inputPortCount(0)
    , duration(1.0)
    , freeze(false)
    , showLegend(true)
    , autoScale(true)
    , plotWindow(nullptr)
    , customPlot(nullptr)
{
    initProperties();
    updateInputPorts();
    openPlotWindow();
}

QtPlotterFbImpl::~QtPlotterFbImpl()
{
    closePlotWindow();
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
    closePlotWindow();
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

    const auto freezeProp = daq::BoolProperty("Freeze", false);
    objPtr.addProperty(freezeProp);
    objPtr.getOnPropertyValueWrite("Freeze") += onPropertyValueWrite;

    const auto showLegendProp = daq::BoolProperty("ShowLegend", true);
    objPtr.addProperty(showLegendProp);
    objPtr.getOnPropertyValueWrite("ShowLegend") += onPropertyValueWrite;

    const auto autoScaleProp = daq::BoolProperty("AutoScale", true);
    objPtr.addProperty(autoScaleProp);
    objPtr.getOnPropertyValueWrite("AutoScale") += onPropertyValueWrite;

    readProperties();
}

void QtPlotterFbImpl::propertyChanged()
{
    readProperties();
}

void QtPlotterFbImpl::readProperties()
{
    duration = objPtr.getPropertyValue("Duration");
    freeze = objPtr.getPropertyValue("Freeze");
    showLegend = objPtr.getPropertyValue("ShowLegend");
    autoScale = objPtr.getPropertyValue("AutoScale");

    LOG_T("Properties: Duration={}, Freeze={}, ShowLegend={}, AutoScale={}",
          duration, freeze, showLegend, autoScale)
}

void QtPlotterFbImpl::updateInputPorts()
{
    // Remove input ports that are no longer connected
    for (auto it = signalContexts.begin(); it != signalContexts.end();)
    {
        const auto& port = it->first;
        if (!port.getSignal().assigned())
        {
            removeInputPort(port);
            it = signalContexts.erase(it);
        }
        else
            ++it;
    }

    // Add a new input port for future connections
    const auto inputPort = createAndAddInputPort(
        fmt::format("Input{}", inputPortCount++),
        daq::PacketReadyNotification::SameThread);

    signalContexts.emplace(inputPort, inputPort);
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

    updateInputPorts();
    LOG_T("Disconnected from port {}", inputPort.getLocalId());
}

void QtPlotterFbImpl::openPlotWindow()
{
    if (plotWindow)
        return;

    // Create main window
    auto* window = new QMainWindow();
    window->setWindowTitle("Qt Signal Plotter");
    window->resize(1024, 768);

    // Create central widget
    auto* centralWidget = new QWidget(window);
    auto* layout = new QVBoxLayout(centralWidget);

    // Create QCustomPlot
    customPlot = new QCustomPlot(centralWidget);
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // Setup axes
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setLabel("Value");
    customPlot->xAxis->grid()->setVisible(true);
    customPlot->yAxis->grid()->setVisible(true);

    // Setup dark theme
    customPlot->setBackground(QColor(43, 43, 43));
    customPlot->xAxis->setBasePen(QPen(QColor(150, 150, 150)));
    customPlot->yAxis->setBasePen(QPen(QColor(150, 150, 150)));
    customPlot->xAxis->setTickPen(QPen(QColor(150, 150, 150)));
    customPlot->yAxis->setTickPen(QPen(QColor(150, 150, 150)));
    customPlot->xAxis->setSubTickPen(QPen(QColor(150, 150, 150)));
    customPlot->yAxis->setSubTickPen(QPen(QColor(150, 150, 150)));
    customPlot->xAxis->setTickLabelColor(QColor(150, 150, 150));
    customPlot->yAxis->setTickLabelColor(QColor(150, 150, 150));
    customPlot->xAxis->setLabelColor(QColor(150, 150, 150));
    customPlot->yAxis->setLabelColor(QColor(150, 150, 150));
    customPlot->xAxis->grid()->setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));
    customPlot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));

    // Setup legend if enabled
    if (showLegend)
    {
        customPlot->legend->setVisible(true);
        customPlot->legend->setBrush(QColor(60, 60, 60, 200));
        customPlot->legend->setTextColor(QColor(200, 200, 200));
        customPlot->legend->setBorderPen(QPen(QColor(150, 150, 150)));
        customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
    }

    layout->addWidget(customPlot);

    // Add control buttons
    auto* controlLayout = new QHBoxLayout();

    auto* closeBtn = new QPushButton("Close", centralWidget);
    QObject::connect(closeBtn, &QPushButton::clicked, [this]()
    {
        closePlotWindow();
    });
    controlLayout->addWidget(closeBtn);

    auto* resetZoomBtn = new QPushButton("Reset Zoom", centralWidget);
    QObject::connect(resetZoomBtn, &QPushButton::clicked, [this]()
    {
        if (customPlot && autoScale)
        {
            customPlot->rescaleAxes();
            customPlot->replot();
        }
    });
    controlLayout->addWidget(resetZoomBtn);

    controlLayout->addStretch();
    layout->addLayout(controlLayout);

    window->setCentralWidget(centralWidget);
    plotWindow = window;
    window->show();

    // Setup periodic redraw timer (packets are processed in onPacketReceived)
    auto* updateTimer = new QTimer(window);
    QObject::connect(updateTimer, &QTimer::timeout, [this]()
    {
        if (plotWindow)
        {
            updatePlot();
        }
    });
    updateTimer->start(50);  // Redraw at ~20 FPS

    LOG_I("Plot window opened");
}

void QtPlotterFbImpl::closePlotWindow()
{
    if (plotWindow)
    {
        plotWindow->deleteLater();
        plotWindow = nullptr;
        customPlot = nullptr;
        LOG_I("Plot window closed");
    }
}

void QtPlotterFbImpl::updatePlot()
{
    if (!customPlot || !plotWindow)
        return;

    // For each valid signal, create or update graph
    size_t graphIndex = 0;
    for (auto& [port, sigCtx] : signalContexts)
    {
        if (!port.getSignal().assigned())
            continue;

        // Get or create graph for this signal
        QCPGraph* graph = nullptr;
        if (graphIndex < static_cast<size_t>(customPlot->graphCount()))
            graph = customPlot->graph(static_cast<int>(graphIndex));
        else
        {
            graph = customPlot->addGraph();
            graph->setName(QString::fromStdString(sigCtx.caption));

            // Colors for different signals
            QColor colors[] = {
                QColor(255, 0, 0),      // Red
                QColor(255, 255, 0),    // Yellow
                QColor(0, 255, 0),      // Green
                QColor(255, 0, 255),    // Magenta
                QColor(0, 255, 255),    // Cyan
                QColor(0, 0, 255)       // Blue
            };
            graph->setPen(QPen(colors[graphIndex % 6], 2));
        }

        // Clear old data for simplicity (можно оптимизировать позже)
        // graph->data()->clear();

        // Extract and plot data using TimeReader
        if (sigCtx.timeReader.assigned())
        {
            try
            {
                // sigCtx.timeReader.getAxx
                const size_t maxSamples = 1000;
                double samples[maxSamples];
                std::chrono::system_clock::time_point timeStamps[maxSamples];

                daq::SizeT count = maxSamples;
                sigCtx.timeReader.readWithDomain(samples, timeStamps, &count);

                if (count > 0)
                {
                    // Convert timestamps to relative seconds
                    if (sigCtx.firstSample.time_since_epoch().count() == 0)
                        sigCtx.firstSample = timeStamps[0];
                    for (daq::SizeT i = 0; i < count; ++i)
                    {
                        auto timeSeconds = std::chrono::duration_cast<std::chrono::microseconds>(timeStamps[i] - sigCtx.firstSample).count();
                        graph->addData(timeSeconds, samples[i]);
                    }
                }
            }
            catch (const std::exception& e)
            {
                LOG_W("Error reading data from TimeReader: {}", e.what())
            }
        }

        graphIndex++;
    }

    // Auto-scale if enabled
    if (autoScale)
    {
        customPlot->rescaleAxes();
    }

    customPlot->replot();
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

}  // namespace QtPlotter

END_NAMESPACE_OPENDAQ_QT_MODULE
