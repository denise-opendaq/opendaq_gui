#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>

#include <opendaq/input_port_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class ComponentTreeWidget;

// Widget for selecting a signal for an input port (without value display)
class InputPortSignalSelector : public QWidget
{
    Q_OBJECT

public:
    explicit InputPortSignalSelector(const daq::InputPortPtr& inputPort, ComponentTreeWidget* componentTree, QWidget* parent = nullptr);
    ~InputPortSignalSelector() override;

    // Set whether to show a group box with port name (default: true)
    void setShowGroupBox(bool show);
    
    // Set group box title (if showGroupBox is true)
    void setGroupBoxTitle(const QString& title);

private:
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);
    void setupSignalSelection();
    QString getSignalPath(const daq::SignalPtr& signal) const;

private Q_SLOTS:
    void populateSignals();
    void onSignalSelected(int index);
    void connectSignal(const daq::SignalPtr& signal);
    void disconnectSignal();

private:
    daq::InputPortPtr inputPort;
    ComponentTreeWidget* componentTree;
    QComboBox* signalComboBox;
    QGroupBox* groupBox;
    bool showGroupBox;
};

