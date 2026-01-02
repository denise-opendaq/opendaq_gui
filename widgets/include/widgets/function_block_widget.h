#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

#include <opendaq/function_block_ptr.h>
#include <opendaq/recorder_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class ComponentTreeWidget;
class InputPortSignalSelector;
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
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);
    void setupInputPorts();
    void setupRecorderControls();

private Q_SLOTS:
    void updateInputPorts();
    void onRecordingToggled(bool checked);
    void updateRecordingStatus();

private:
    daq::FunctionBlockPtr functionBlock;
    daq::FolderPtr inputPortsFolder; // Folder containing input ports ("IP")
    ComponentTreeWidget* componentTree;
    QVBoxLayout* mainLayout;
    QVBoxLayout* inputPortsLayout;  // Separate layout for input ports
    QMap<QString, InputPortSignalSelector*> inputPortSelectors; // Map by global ID

    // Recorder controls
    QCheckBox* recordingToggle;
    QHBoxLayout* recorderLayout;
};

