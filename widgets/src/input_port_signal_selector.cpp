#include "widgets/input_port_signal_selector.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <opendaq/instance_ptr.h>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

InputPortSignalSelector::InputPortSignalSelector(const daq::InputPortPtr& inputPort, ComponentTreeWidget* componentTree, QWidget* parent)
    : QWidget(parent)
    , inputPort(inputPort)
    , componentTree(componentTree)
    , signalComboBox(nullptr)
    , groupBox(nullptr)
    , showGroupBox(true)
{
    setupSignalSelection();

    // Subscribe to input port core events
    if (inputPort.assigned()) 
    {
        try 
        {
            *AppContext::DaqEvent() += daq::event(this, &InputPortSignalSelector::onCoreEvent);
        } 
        catch (const std::exception& e) 
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("Failed to subscribe to input port events: {}", e.what());
        }
    }

    // Initial populate
    populateSignals();
}

InputPortSignalSelector::~InputPortSignalSelector()
{
    // Unsubscribe from events
    if (inputPort.assigned()) 
    {
        try 
        {
            *AppContext::DaqEvent() -= daq::event(this, &InputPortSignalSelector::onCoreEvent);
        } 
        catch (const std::exception& e) 
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("Failed to unsubscribe from input port events: {}", e.what());
        }
    }
}

void InputPortSignalSelector::setShowGroupBox(bool show)
{
    if (showGroupBox == show)
        return;
    
    showGroupBox = show;
    
    if (groupBox)
        groupBox->setVisible(show);
}

void InputPortSignalSelector::setGroupBoxTitle(const QString& title)
{
    if (groupBox)
        groupBox->setTitle(title);
}

void InputPortSignalSelector::setupSignalSelection()
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget* containerWidget = this;
    
    // Create group box if needed
    if (showGroupBox) 
    {
        QString portName = inputPort.assigned() ? QString::fromStdString(inputPort.getName().toStdString()) : "Input Port";
        groupBox = new QGroupBox(portName, this);
        containerWidget = groupBox;
        mainLayout->addWidget(groupBox);
    }

    // Create layout for combo box
    QVBoxLayout* containerLayout;
    if (showGroupBox) 
    {
        containerLayout = new QVBoxLayout(containerWidget);
        containerLayout->setSpacing(10);
        containerLayout->setContentsMargins(10, 10, 10, 10);
    }
    else
    {
        containerLayout = mainLayout;
    }

    signalComboBox = new QComboBox(containerWidget);
    signalComboBox->setEditable(false);
    signalComboBox->setMinimumWidth(300);
    connect(signalComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InputPortSignalSelector::onSignalSelected);

    containerLayout->addWidget(signalComboBox);
    
    if (!showGroupBox)
        mainLayout->addStretch();
}

void InputPortSignalSelector::populateSignals()
{
    if (!signalComboBox)
        return;

    // Block signals to prevent onSignalSelected from being called during programmatic updates
    signalComboBox->blockSignals(true);
    
    signalComboBox->clear();

    try 
    {
        auto instance = AppContext::Instance()->daqInstance();
        if (!instance.assigned()) 
        {
            signalComboBox->addItem("No instance available");
            signalComboBox->blockSignals(false);
            return;
        }

        // Add "Disconnect" option at index 0
        signalComboBox->addItem("(Disconnect)");

        // Get all signals recursively
        auto allSignals = instance.getSignalsRecursive();
        if (!allSignals.assigned()) 
        {
            signalComboBox->addItem("No signals available");
            signalComboBox->blockSignals(false);
            return;
        }

        // Get current signal path for selection
        QString currentSignalPath;
        if (inputPort.assigned()) 
        {
            auto currentSignal = inputPort.getSignal();
            if (currentSignal.assigned())
                currentSignalPath = getSignalPath(currentSignal);
        }

        // Add signals to combo box
        for (const auto & signal : allSignals) 
        {
            try 
            {
                QString path = getSignalPath(signal);
                signalComboBox->addItem(path);
                
                // Select current signal if it matches
                if (!currentSignalPath.isEmpty() && path == currentSignalPath)
                    signalComboBox->setCurrentIndex(signalComboBox->count() - 1);
            }
            catch (const std::exception&) 
            {
                // Skip invalid signals
            }
        }
        
        // If no signal is selected and no current signal, select "(Disconnect)"
        if (signalComboBox->currentIndex() < 0 && currentSignalPath.isEmpty())
            signalComboBox->setCurrentIndex(0);
    } 
    catch (const std::exception& e) 
    {
        signalComboBox->addItem(QString("Error: %1").arg(e.what()));
    }
    
    // Unblock signals
    signalComboBox->blockSignals(false);
}

void InputPortSignalSelector::onSignalSelected(int index)
{
    if (index < 0 || index >= signalComboBox->count())
        return;

    // Index 0 is "(Disconnect)"
    if (index == 0) 
    {
        // Check if already disconnected
        if (inputPort.assigned()) 
        {
            auto currentSignal = inputPort.getSignal();
            if (!currentSignal.assigned())
                return; // Already disconnected
        }
        disconnectSignal();
        return;
    }

    // Get the path from combo box and find the signal using instance.findComponent
    QString path = signalComboBox->itemText(index);
    
    try 
    {
        auto instance = AppContext::Instance()->daqInstance();
        if (!instance.assigned())
            return;

        // Find component by path
        auto component = instance.findComponent(path.toStdString());
        if (!component.assigned())
            return;

        // Try to cast to Signal
        auto signal = component.asPtrOrNull<daq::ISignal>(true);
        if (!signal.assigned())
            return;

        // Check if this signal is already connected
        if (inputPort.assigned()) 
        {
            auto currentSignal = inputPort.getSignal();
            if (currentSignal.assigned()) 
            {
                try 
                {
                    if (currentSignal == signal)
                        return; // Already connected to this signal
                } 
                catch (const std::exception&) 
                {
                    // If we can't compare, proceed with connection
                }
            }
        }

        connectSignal(signal);
    } 
    catch (const std::exception& e) 
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Failed to find signal by path: {} Error: {}", path.toStdString(), e.what());
        // Refresh combo box to restore previous selection
        populateSignals();
    }
}

void InputPortSignalSelector::connectSignal(const daq::SignalPtr& signal)
{
    try 
    {
        if (!inputPort.assigned())
            return;

        inputPort.connect(signal);
        populateSignals(); // Refresh to update selection
    } 
    catch (const std::exception& e) 
    {
        QMessageBox::critical(this, "Error", QString("Failed to connect signal: %1").arg(e.what()));
        // Refresh combo box to restore previous selection
        populateSignals();
    }
}

void InputPortSignalSelector::disconnectSignal()
{
    try 
    {
        if (!inputPort.assigned())
            return;

        inputPort.disconnect();
        populateSignals(); // Refresh to update selection
    } 
    catch (const std::exception& e)
    {
        QMessageBox::critical(this, "Error", QString("Failed to disconnect signal: %1").arg(e.what()));
        // Refresh combo box to restore previous selection
        populateSignals();
    }
}

void InputPortSignalSelector::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != inputPort)
        return;

    try 
    {
        auto eventId = static_cast<daq::CoreEventId>(args.getEventId());
        // Check for SignalConnected or SignalDisconnected events
        if (eventId == daq::CoreEventId::SignalConnected || eventId == daq::CoreEventId::SignalDisconnected)
        {
            // Core events come from openDAQ thread, need to invoke populateSignals in main Qt thread
            QMetaObject::invokeMethod(this, "populateSignals", Qt::QueuedConnection);
        }
    } 
    catch (const std::exception& e) 
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error handling core event: {}", e.what());
    }
}

QString InputPortSignalSelector::getSignalPath(const daq::SignalPtr& signal) const
{
    if (!signal.assigned())
        return "N/A";

    try 
    {
        // Use getGlobalId() to get the full path
        auto globalId = signal.getGlobalId();
        return QString::fromStdString(globalId.toStdString());
    } 
    catch (const std::exception& e) 
    {
        return QString("Error: %1").arg(e.what());
    }
}

