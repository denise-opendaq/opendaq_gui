#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

#include <opendaq/function_block_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class ComponentTreeWidget;
class InputPortSignalSelector;

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

private Q_SLOTS:
    void updateInputPorts();

private:
    daq::FunctionBlockPtr functionBlock;
    daq::FolderPtr inputPortsFolder; // Folder containing input ports ("IP")
    ComponentTreeWidget* componentTree;
    QVBoxLayout* mainLayout;
    QMap<QString, InputPortSignalSelector*> inputPortSelectors; // Map by global ID
};

