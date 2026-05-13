#pragma once

#include <atomic>

#include "widgets/component_widget.h"
#include <opendaq/signal_ptr.h>

class QCheckBox;
class QLabel;
class QTimer;

class SignalWidget : public ComponentWidget
{
    Q_OBJECT

public:
    explicit SignalWidget(const daq::SignalPtr& signal, QWidget* parent = nullptr);
    ~SignalWidget() override;

private:
    QWidget* buildHeaderCard() override;
    void populateTabs() override;

    void updatePublicToggle();
    void updateLastValue();
    QString formatSignalValue(const daq::SignalPtr& sig) const;

    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

    daq::SignalPtr signal;
    std::atomic<int> updatingFromSignal{0};

    QCheckBox* publicCheckBox   = nullptr;
    QLabel*    lastValueLabel   = nullptr;
    QLabel*    domainValueLabel = nullptr;
    QTimer*    timer            = nullptr;
};
