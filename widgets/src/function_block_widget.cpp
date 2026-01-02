#include "widgets/function_block_widget.h"
#include "widgets/input_port_signal_selector.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QMetaObject>
#include <QSignalBlocker>
#include <QMap>
#include <QSet>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>
#include <opendaq/recorder_ptr.h>

// FunctionBlockWidget implementation
FunctionBlockWidget::FunctionBlockWidget(const daq::FunctionBlockPtr& functionBlock, ComponentTreeWidget* componentTree, QWidget* parent)
    : QWidget(parent)
    , functionBlock(functionBlock)
    , inputPortsFolder(functionBlock.getItem("IP"))
    , componentTree(componentTree)
    , mainLayout(nullptr)
    , inputPortsLayout(nullptr)
    , inputPortSelectors()
    , recordingToggle(nullptr)
    , recorderLayout(nullptr)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    mainLayout = layout;

    *AppContext::DaqEvent() += daq::event(this, &FunctionBlockWidget::onCoreEvent);

    // Create separate container for input ports
    inputPortsLayout = new QVBoxLayout();
    inputPortsLayout->setSpacing(10);
    inputPortsLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(inputPortsLayout);

    // Setup recorder controls first (if supported)
    setupRecorderControls();

    setupInputPorts();
}

FunctionBlockWidget::~FunctionBlockWidget()
{
    *AppContext::DaqEvent() -= daq::event(this, &FunctionBlockWidget::onCoreEvent);
}

void FunctionBlockWidget::setupInputPorts()
{
    // Clear existing widgets
    QList<InputPortSignalSelector*> selectorsToRemove = inputPortSelectors.values();
    for (auto* selector : selectorsToRemove)
    {
        inputPortsLayout->removeWidget(selector);
        selector->deleteLater();
    }
    inputPortSelectors.clear();

    // Remove stretch if it exists
    QLayoutItem* item;
    while ((item = inputPortsLayout->takeAt(inputPortsLayout->count() - 1)) != nullptr)
    {
        if (item->spacerItem())
        {
            delete item;
            break;
        }
        delete item;
    }

    updateInputPorts();
}

void FunctionBlockWidget::updateInputPorts()
{
    if (!functionBlock.assigned())
    {
        auto label = new QLabel("Function block not assigned", this);
        inputPortsLayout->addWidget(label);
        return;
    }

    try
    {
        // Get all input ports from the function block
        auto inputPorts = functionBlock.getInputPorts();

        if (!inputPorts.assigned() || inputPorts.getCount() == 0)
        {
            // Check if we already have a "No input ports" label
            bool hasNoPortsLabel = false;
            for (int i = 0; i < inputPortsLayout->count(); ++i)
            {
                QLayoutItem* item = inputPortsLayout->itemAt(i);
                if (item && item->widget())
                {
                    auto label = qobject_cast<QLabel*>(item->widget());
                    if (label && label->text() == "No input ports available")
                    {
                        hasNoPortsLabel = true;
                        break;
                    }
                }
            }

            if (!hasNoPortsLabel)
            {
                auto label = new QLabel("No input ports available", this);
                inputPortsLayout->addWidget(label);
            }
            inputPortsLayout->addStretch();
            return;
        }

        // Remove "No input ports" label if it exists
        for (int i = inputPortsLayout->count() - 1; i >= 0; --i)
        {
            QLayoutItem* item = inputPortsLayout->itemAt(i);
            if (item && item->widget())
            {
                auto label = qobject_cast<QLabel*>(item->widget());
                if (label && (label->text() == "No input ports available" || label->text() == "Function block not assigned"))
                {
                    inputPortsLayout->removeWidget(label);
                    label->deleteLater();
                }
            }
        }

        // Build set of current input port global IDs
        QSet<QString> currentPortIds;
        for (const auto & inputPort : inputPorts)
        {
            try
            {
                QString globalId = QString::fromStdString(inputPort.getGlobalId());
                currentPortIds.insert(globalId);
            } 
            catch (const std::exception&)
            {
                // Skip invalid ports
            }
        }

        // Remove selectors for ports that no longer exist
        QList<QString> keysToRemove;
        for (auto it = inputPortSelectors.begin(); it != inputPortSelectors.end(); ++it)
        {
            if (!currentPortIds.contains(it.key()))
            {
                inputPortsLayout->removeWidget(it.value());
                it.value()->deleteLater();
                keysToRemove.append(it.key());
            }
        }

        for (const QString& key : keysToRemove)
            inputPortSelectors.remove(key);

        // Add selectors for new ports
        for (const auto & inputPort : inputPorts)
        {
            try
            {
                QString globalId = QString::fromStdString(inputPort.getGlobalId().toStdString());

                // Check if selector already exists for this port
                if (!inputPortSelectors.contains(globalId))
                {
                    auto selector = new InputPortSignalSelector(inputPort, componentTree, this);
                    inputPortSelectors[globalId] = selector;

                    // Remove stretch temporarily to add widget before it
                    QLayoutItem* stretchItem = nullptr;
                    for (int i = inputPortsLayout->count() - 1; i >= 0; --i)
                    {
                        QLayoutItem* item = inputPortsLayout->itemAt(i);
                        if (item && item->spacerItem())
                        {
                            stretchItem = item;
                            inputPortsLayout->removeItem(stretchItem);
                            break;
                        }
                    }

                    // Add new selector
                    inputPortsLayout->addWidget(selector);

                    // Re-add stretch at the end
                    if (stretchItem)
                        inputPortsLayout->addItem(stretchItem);
                    else
                        inputPortsLayout->addStretch();
                }
            }
            catch (const std::exception& e)
            {
                const auto loggerComponent = AppContext::LoggerComponent();
                LOG_W("Failed to process input port: {}", e.what());
            }
        }
    }
    catch (const std::exception& e)
    {
        auto label = new QLabel(QString("Error getting input ports: %1").arg(e.what()), this);
        inputPortsLayout->addWidget(label);
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error getting input ports: {}", e.what());
    }
}

void FunctionBlockWidget::setupRecorderControls()
{
    if (!functionBlock.supportsInterface<daq::IRecorder>())
        return;

    // Create group box for recorder controls
    auto* groupBox = new QGroupBox("Recording", this);

    // Create horizontal layout for recorder controls
    recorderLayout = new QHBoxLayout(groupBox);
    recorderLayout->setSpacing(10);
    recorderLayout->setContentsMargins(10, 10, 10, 10);

    // Add label
    auto* label = new QLabel("Recording", groupBox);

    // Create recording toggle checkbox (without text, just the indicator)
    recordingToggle = new QCheckBox(groupBox);

    // Create container widget for toggle to match combo box width
    auto* toggleContainer = new QWidget(groupBox);
    toggleContainer->setMinimumWidth(300);
    toggleContainer->setMaximumWidth(300);
    auto* toggleLayout = new QHBoxLayout(toggleContainer);
    toggleLayout->setContentsMargins(0, 0, 0, 0);
    toggleLayout->addStretch();
    toggleLayout->addWidget(recordingToggle);

    // Add widgets to layout: label on left, toggle container on right
    recorderLayout->addWidget(label);
    recorderLayout->addWidget(toggleContainer);

    // Create a separate widget container for recorder
    auto* recorderContainer = new QWidget(this);
    auto* containerLayout = new QVBoxLayout(recorderContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(groupBox);

    // Add recorder container to main layout (before inputPortsLayout)
    mainLayout->insertWidget(0, recorderContainer);

    // Connect toggle signal
    connect(recordingToggle, &QCheckBox::toggled, this, &FunctionBlockWidget::onRecordingToggled);

    // Initial status update
    updateRecordingStatus();
}

void FunctionBlockWidget::onRecordingToggled(bool checked)
{
    auto recorder = functionBlock.asPtrOrNull<daq::IRecorder>(true);
    if (!recorder.assigned())
        return;

    try
    {
        if (checked)
        {
            recorder.startRecording();
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_I("Started recording");
        }
        else
        {
            recorder.stopRecording();
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_I("Stopped recording");
        }
    }
    catch (const std::exception& e)
    {
        // Revert checkbox state on error
        QSignalBlocker blocker(recordingToggle);
        recordingToggle->setChecked(!checked);

        QMessageBox::critical(this, "Recording Error", QString("Failed to toggle recording: %1").arg(e.what()));
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_E("Failed to toggle recording: {}", e.what());
    }
}

void FunctionBlockWidget::updateRecordingStatus()
{
    auto recorder = functionBlock.asPtrOrNull<daq::IRecorder>(true);
    if (!recorder.assigned() || !recordingToggle)
        return;

    try
    {
        bool isRecording = recorder.getIsRecording();

        // Block signals to avoid triggering onRecordingToggled
        QSignalBlocker blocker(recordingToggle);
        recordingToggle->setChecked(isRecording);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Failed to get recording status: {}", e.what());
    }
}

void FunctionBlockWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    // Events come from input ports folder
    if (sender != inputPortsFolder)
        return;

    try
    {
        auto eventId = static_cast<daq::CoreEventId>(args.getEventId());
        // Events from input ports folder indicate changes in the folder structure
        if (eventId == daq::CoreEventId::ComponentAdded || eventId == daq::CoreEventId::ComponentRemoved)
            QMetaObject::invokeMethod(this, "updateInputPorts", Qt::QueuedConnection);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error handling function block core event: {}", e.what());
    }
}

