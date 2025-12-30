#pragma once

#include <QWidget>
#include <QLabel>
#include <QString>

#include "context/UpdateScheduler.h"

#include <opendaq/signal_ptr.h>

class SignalValueWidget : public QWidget, public IUpdatable
{
    Q_OBJECT

public:
    explicit SignalValueWidget(const daq::SignalPtr& signal, QWidget* parent = nullptr);
    ~SignalValueWidget() override;

    // IUpdatable interface
    void onScheduledUpdate() override;

protected:
    daq::SignalPtr signal;
    QLabel* valueLabel;
    QLabel* signalNameLabel;
    QLabel* signalUnitLabel;
    QLabel* signalTypeLabel;
    QLabel* signalOriginLabel;

private:
    void setupUI();
    void updateSignalInfo();
    bool isTimeDomainSignal();
    QString getLastValue();
};
