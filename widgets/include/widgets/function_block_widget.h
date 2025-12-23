#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

// Save Qt keywords and undefine them before including openDAQ
#pragma push_macro("signals")
#pragma push_macro("slots")
#pragma push_macro("emit")
#undef signals
#undef slots
#undef emit

#include <opendaq/opendaq.h>

// Restore Qt keywords
#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")

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

