#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

#include <opendaq/folder_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class ComponentTreeWidget;
class InputPortSignalSelector;

// Widget for managing all input ports in a folder
class InputPortFolderSelector : public QWidget
{
    Q_OBJECT

public:
    explicit InputPortFolderSelector(const daq::FolderPtr& inputPortsFolder, ComponentTreeWidget* componentTree, QWidget* parent = nullptr);
    ~InputPortFolderSelector() override;

private:
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);
    void setupInputPorts();

private Q_SLOTS:
    void updateInputPorts();

private:
    daq::FolderPtr inputPortsFolder;
    ComponentTreeWidget* componentTree;
    QVBoxLayout* inputPortsLayout;
    QMap<QString, InputPortSignalSelector*> inputPortSelectors; // Map by global ID
};

