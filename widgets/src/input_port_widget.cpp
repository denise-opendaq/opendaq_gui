#include "widgets/input_port_widget.h"
#include "widgets/input_port_signal_selector.h"
#include "context/AppContext.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMetaObject>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

InputPortWidget::InputPortWidget(const daq::InputPortPtr& inputPort, ComponentTreeWidget* componentTree, QWidget* parent)
    : SignalValueWidget(inputPort.assigned() ? inputPort.getSignal() : daq::SignalPtr(), parent)
    , inputPort(inputPort)
    , componentTree(componentTree)
    , signalSelector(nullptr)
{
    // SignalValueWidget constructor creates QHBoxLayout with value and info groups
    // We need to wrap it in QVBoxLayout and add signal selection at the top
    auto oldLayout = qobject_cast<QHBoxLayout*>(layout());
    QList<QWidget*> widgets;
    
    if (oldLayout) {
        // Extract all widgets from old layout
        while (oldLayout->count() > 0) {
            QLayoutItem* item = oldLayout->takeAt(0);
            if (item->widget()) {
                widgets.append(item->widget());
            }
            delete item;
        }
        delete oldLayout;
    }

    // Create new vertical layout
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Create signal selector widget (without group box, we'll add our own)
    signalSelector = new InputPortSignalSelector(inputPort, componentTree, this);
    signalSelector->setShowGroupBox(false);
    
    // Wrap signal selector in a group box with title "Select Signal"
    auto selectionGroup = new QGroupBox("Select Signal", this);
    auto selectionLayout = new QVBoxLayout(selectionGroup);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    selectionLayout->addWidget(signalSelector);
    mainLayout->addWidget(selectionGroup);
    
    // Re-add the signal value display widgets from SignalValueWidget
    if (!widgets.isEmpty()) {
        auto valueLayout = new QHBoxLayout();
        valueLayout->setSpacing(10);
        for (auto* widget : widgets) {
            valueLayout->addWidget(widget, 1);
        }
        mainLayout->addLayout(valueLayout);
    }
    
    mainLayout->addStretch();
    setLayout(mainLayout);

    // SignalValueWidget constructor always registers us, but we may not have a signal
    // Unregister first, then we'll register in updateSignal if needed
    AppContext::instance()->updateScheduler()->unregisterUpdatable(this);
    
    // Initial update - this will register us if signal is assigned
    updateSignal();
}

InputPortWidget::~InputPortWidget()
{
    // Cleanup is handled by Qt's parent-child relationship
}

void InputPortWidget::updateSignal()
{
    if (!inputPort.assigned()) {
        return;
    }

    try {
        auto newSignal = inputPort.getSignal();
        
        // Check if signal changed
        bool signalChanged = true;
        if (signal.assigned() && newSignal.assigned()) {
            try {
                if (signal.getGlobalId() == newSignal.getGlobalId()) {
                    signalChanged = false;
                }
            } catch (const std::exception&) {
                // If we can't compare, assume changed
            }
        } else if (!signal.assigned() && !newSignal.assigned()) {
            signalChanged = false;
        }

        if (signalChanged) {
            // Unregister old signal if it was assigned
            AppContext::instance()->updateScheduler()->unregisterUpdatable(this);
            
            // Update the signal
            signal = newSignal;
            
            // Register new signal if assigned
            if (signal.assigned()) {
                AppContext::instance()->updateScheduler()->registerUpdatable(this);
                onScheduledUpdate();
            } else {
                // Clear the display when disconnected
                if (valueLabel) {
                    valueLabel->setText("No signal connected");
                }
                if (signalNameLabel) {
                    signalNameLabel->setText("Signal: Not connected");
                }
                if (signalUnitLabel) {
                    signalUnitLabel->setText("");
                }
                if (signalTypeLabel) {
                    signalTypeLabel->setText("");
                }
                if (signalOriginLabel) {
                    signalOriginLabel->setText("");
                }
            }
        }
    } catch (const std::exception& e) {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error updating signal: {}", e.what());
    }
}

