#include "widgets/function_block_widget.h"
#include "widgets/input_port_folder_selector.h"
#include "context/AppContext.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QSignalBlocker>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>
#include <opendaq/recorder_ptr.h>

// FunctionBlockWidget implementation
FunctionBlockWidget::FunctionBlockWidget(const daq::FunctionBlockPtr& functionBlock, ComponentTreeWidget* componentTree, QWidget* parent)
    : QWidget(parent)
    , functionBlock(functionBlock)
    , componentTree(componentTree)
    , mainLayout(nullptr)
    , inputPortFolderSelector(nullptr)
    , recordingToggle(nullptr)
    , recorderLayout(nullptr)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    mainLayout = layout;

    // Setup recorder controls first (if supported)
    setupRecorderControls();

    // Create input port folder selector
    inputPortFolderSelector = new InputPortFolderSelector(functionBlock.getItem("IP"), componentTree, this);
    mainLayout->addWidget(inputPortFolderSelector);
}

FunctionBlockWidget::~FunctionBlockWidget()
{
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


