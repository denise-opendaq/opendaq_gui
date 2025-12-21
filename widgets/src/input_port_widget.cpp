#include "widgets/input_port_widget.h"
#include "context/AppContext.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>

InputPortWidget::InputPortWidget(const daq::InputPortPtr& inputPort, ComponentTreeWidget* componentTree, QWidget* parent)
    : SignalValueWidget(inputPort.assigned() ? inputPort.getSignal() : daq::SignalPtr(), parent)
    , inputPort(inputPort)
    , componentTree(componentTree)
    , signalComboBox(nullptr)
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

    // Setup signal selection UI (this will add the combo box at the top)
    setupSignalSelection();
    
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
    
    // Subscribe to input port core events
    if (inputPort.assigned()) {
        try {
            inputPort.getOnComponentCoreEvent() += daq::event(this, &InputPortWidget::onCoreEvent);
        } catch (const std::exception& e) {
            qWarning() << "Failed to subscribe to input port events:" << e.what();
        }
    }
    
    // Initial update - this will register us if signal is assigned
    updateSignal();
}

InputPortWidget::~InputPortWidget()
{
    // Unsubscribe from events
    if (inputPort.assigned()) {
        try {
            inputPort.getOnComponentCoreEvent() -= daq::event(this, &InputPortWidget::onCoreEvent);
        } catch (const std::exception& e) {
            qWarning() << "Failed to unsubscribe from input port events:" << e.what();
        }
    }
}

void InputPortWidget::setupSignalSelection()
{
    // Add signal selection group at the top
    auto selectionGroup = new QGroupBox("Select Signal", this);
    auto selectionLayout = new QVBoxLayout(selectionGroup);

    signalComboBox = new QComboBox(selectionGroup);
    signalComboBox->setEditable(false);
    signalComboBox->setMinimumWidth(300);
    connect(signalComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InputPortWidget::onSignalSelected);

    selectionLayout->addWidget(signalComboBox);

    // Get the main layout and add selection group at the top
    auto mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        mainLayout->insertWidget(0, selectionGroup);
    }

    // Populate signals
    populateSignals();
}

void InputPortWidget::populateSignals()
{
    if (!signalComboBox) {
        return;
    }

    // Block signals to prevent onSignalSelected from being called during programmatic updates
    signalComboBox->blockSignals(true);
    
    signalComboBox->clear();

    try {
        auto instance = AppContext::instance()->daqInstance();
        if (!instance.assigned()) {
            signalComboBox->addItem("No instance available");
            signalComboBox->blockSignals(false);
            return;
        }

        // Add "Disconnect" option at index 0
        signalComboBox->addItem("(Disconnect)");

        // Get all signals recursively
        auto allSignals = instance.getSignalsRecursive();
        if (!allSignals.assigned()) {
            signalComboBox->addItem("No signals available");
            signalComboBox->blockSignals(false);
            return;
        }

        // Get current signal path for selection
        QString currentSignalPath;
        if (inputPort.assigned()) {
            auto currentSignal = inputPort.getSignal();
            if (currentSignal.assigned()) {
                currentSignalPath = getSignalPath(currentSignal);
            }
        }

        // Add signals to combo box
        int selectedIndex = 0; // Default to disconnect
        for (size_t i = 0; i < allSignals.getCount(); ++i) {
            try {
                auto signal = allSignals[i];
                if (!signal.assigned()) {
                    continue;
                }

                QString path = getSignalPath(signal);
                signalComboBox->addItem(path);

                // Select current signal if it matches
                if (!currentSignalPath.isEmpty() && path == currentSignalPath) {
                    selectedIndex = signalComboBox->count() - 1;
                }
            } catch (const std::exception&) {
                // Skip invalid signals
            }
        }

        // Set the selected index
        signalComboBox->setCurrentIndex(selectedIndex);

    } catch (const std::exception& e) {
        signalComboBox->addItem(QString("Error: %1").arg(e.what()));
    }
    
    // Unblock signals
    signalComboBox->blockSignals(false);
}

void InputPortWidget::onSignalSelected(int index)
{
    if (index < 0 || index >= signalComboBox->count()) {
        return;
    }

    // Index 0 is "(Disconnect)"
    if (index == 0) {
        // Check if already disconnected
        if (inputPort.assigned()) {
            auto currentSignal = inputPort.getSignal();
            if (!currentSignal.assigned()) {
                return; // Already disconnected
            }
        }
        disconnectSignal();
        return;
    }

    // Get the path from combo box and find the signal using instance.findComponent
    QString path = signalComboBox->itemText(index);
    
    try {
        auto instance = AppContext::instance()->daqInstance();
        if (!instance.assigned()) {
            return;
        }

        // Find component by path
        auto component = instance.findComponent(path.toStdString());
        if (!component.assigned()) {
            return;
        }

        // Try to cast to Signal
        auto signal = component.asPtrOrNull<daq::ISignal>();
        if (!signal.assigned()) {
            return;
        }

        // Check if this signal is already connected
        if (inputPort.assigned()) {
            auto currentSignal = inputPort.getSignal();
            if (currentSignal.assigned()) {
                try {
                    if (currentSignal == signal) {
                        return; // Already connected to this signal
                    }
                } catch (const std::exception&) {
                    // If we can't compare, proceed with connection
                }
            }
        }

        connectSignal(signal);
    } catch (const std::exception& e) {
        qWarning() << "Failed to find signal by path:" << path << "Error:" << e.what();
        // Refresh combo box to restore previous selection
        populateSignals();
    }
}

void InputPortWidget::connectSignal(const daq::SignalPtr& signal)
{
    try {
        if (!inputPort.assigned()) {
            return;
        }

        inputPort.connect(signal);
        updateSignal();
        AppContext::instance()->addLogMessage(QString("Signal '%1' connected to input port '%2'")
            .arg(getSignalPath(signal), QString::fromStdString(inputPort.getName().toStdString())));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to connect signal: %1").arg(e.what()));
        // Refresh combo box to restore previous selection
        populateSignals();
    }
}

void InputPortWidget::disconnectSignal()
{
    try {
        if (!inputPort.assigned()) {
            return;
        }

        inputPort.disconnect();
        updateSignal();
        AppContext::instance()->addLogMessage(QString("Signal disconnected from input port '%1'")
            .arg(QString::fromStdString(inputPort.getName().toStdString())));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to disconnect signal: %1").arg(e.what()));
        // Refresh combo box to restore previous selection
        populateSignals();
    }
}

void InputPortWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != inputPort) {
        return;
    }

    try {
        auto eventName = args.getEventName();
        
        // Check for SignalConnected or SignalDisconnected events
        if (eventName == "SignalConnected" || eventName == "SignalDisconnected") {
            // Core events come from openDAQ thread, need to invoke updateSignal in main Qt thread
            QMetaObject::invokeMethod(this, "updateSignal", Qt::QueuedConnection);
            // Also update combo box selection
            QMetaObject::invokeMethod(this, "populateSignals", Qt::QueuedConnection);
        }
    } catch (const std::exception& e) {
        qWarning() << "Error handling core event:" << e.what();
    }
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
        qWarning() << "Error updating signal:" << e.what();
    }
}

QString InputPortWidget::getSignalPath(const daq::SignalPtr& signal) const
{
    if (!signal.assigned()) {
        return "N/A";
    }

    try {
        // Use getGlobalId() to get the full path
        auto globalId = signal.getGlobalId();
        return QString::fromStdString(globalId.toStdString());
    } catch (const std::exception& e) {
        return QString("Error: %1").arg(e.what());
    }
}
