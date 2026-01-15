#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

#include <opendaq/function_block_ptr.h>
#include <opendaq/recorder_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class ComponentTreeWidget;
class InputPortFolderSelector;
class QHBoxLayout;
class QCheckBox;

// Main widget for function block with all input ports
class FunctionBlockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionBlockWidget(const daq::FunctionBlockPtr& functionBlock, ComponentTreeWidget* componentTree, QWidget* parent = nullptr);
    ~FunctionBlockWidget() override;

private:
    void setupRecorderControls();

private Q_SLOTS:
    void onRecordingToggled(bool checked);
    void updateRecordingStatus();

private:
    daq::FunctionBlockPtr functionBlock;
    ComponentTreeWidget* componentTree;
    QVBoxLayout* mainLayout;
    InputPortFolderSelector* inputPortFolderSelector;

    // Recorder controls
    QCheckBox* recordingToggle;
    QHBoxLayout* recorderLayout;
};

