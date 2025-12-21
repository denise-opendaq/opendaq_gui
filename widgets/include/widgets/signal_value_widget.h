#pragma once

#include <QWidget>
#include <QLabel>
#include <QString>

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

#include "context/UpdateScheduler.h"

class SignalValueWidget : public QWidget, public IUpdatable
{
    Q_OBJECT

public:
    explicit SignalValueWidget(const daq::SignalPtr& signal, QWidget* parent = nullptr);
    ~SignalValueWidget() override;

    // IUpdatable interface
    void onScheduledUpdate() override;

private:
    void setupUI();
    void updateSignalInfo();
    bool isTimeDomainSignal();
    QString getLastValue();

    daq::SignalPtr signal;
    QLabel* valueLabel;
    QLabel* signalNameLabel;
    QLabel* signalUnitLabel;
    QLabel* signalTypeLabel;
    QLabel* signalOriginLabel;
};
