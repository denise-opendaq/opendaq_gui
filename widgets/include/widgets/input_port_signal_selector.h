#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>

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

