#pragma once

#include "widgets/signal_value_widget.h"
#include <QComboBox>

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

class InputPortWidget : public SignalValueWidget
{
    Q_OBJECT

public:
    explicit InputPortWidget(const daq::InputPortPtr& inputPort, ComponentTreeWidget* componentTree, QWidget* parent = nullptr);
    ~InputPortWidget() override;

private:
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);
    void setupSignalSelection();
    QString getSignalPath(const daq::SignalPtr& signal) const;

private Q_SLOTS:
    void updateSignal();
    void populateSignals();
    void onSignalSelected(int index);
    void connectSignal(const daq::SignalPtr& signal);
    void disconnectSignal();

private:
    daq::InputPortPtr inputPort;
    ComponentTreeWidget* componentTree;
    QComboBox* signalComboBox;
};
