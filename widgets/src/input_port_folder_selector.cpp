#include "widgets/input_port_folder_selector.h"
#include "widgets/input_port_signal_selector.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QMap>
#include <QSet>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

InputPortFolderSelector::InputPortFolderSelector(const daq::FolderPtr& inputPortsFolder, ComponentTreeWidget* componentTree, QWidget* parent)
    : QWidget(parent)
    , inputPortsFolder(inputPortsFolder)
    , componentTree(componentTree)
    , inputPortsLayout(nullptr)
    , inputPortSelectors()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // Create separate container for input ports
    inputPortsLayout = new QVBoxLayout();
    inputPortsLayout->setSpacing(10);
    inputPortsLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(inputPortsLayout);

    // Subscribe to folder events
    if (inputPortsFolder.assigned())
    {
        try
        {
            *AppContext::DaqEvent() += daq::event(this, &InputPortFolderSelector::onCoreEvent);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("Failed to subscribe to input ports folder events: {}", e.what());
        }
    }

    setupInputPorts();
}

InputPortFolderSelector::~InputPortFolderSelector()
{
    // Unsubscribe from events
    if (inputPortsFolder.assigned())
    {
        try
        {
            *AppContext::DaqEvent() -= daq::event(this, &InputPortFolderSelector::onCoreEvent);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("Failed to unsubscribe from input ports folder events: {}", e.what());
        }
    }
}

void InputPortFolderSelector::setupInputPorts()
{
    // Clear existing widgets
    QList<InputPortSignalSelector*> selectorsToRemove = inputPortSelectors.values();
    for (auto* selector : selectorsToRemove)
    {
        inputPortsLayout->removeWidget(selector);
        selector->deleteLater();
    }
    inputPortSelectors.clear();

    // Remove stretch if it exists
    QLayoutItem* item;
    while ((item = inputPortsLayout->takeAt(inputPortsLayout->count() - 1)) != nullptr)
    {
        if (item->spacerItem())
        {
            delete item;
            break;
        }
        delete item;
    }

    updateInputPorts();
}

void InputPortFolderSelector::updateInputPorts()
{
    if (!inputPortsFolder.assigned())
    {
        auto label = new QLabel("Input ports folder not assigned", this);
        inputPortsLayout->addWidget(label);
        return;
    }

    try
    {
        // Get all input ports from the folder
        auto inputPorts = inputPortsFolder.getItems();

        if (!inputPorts.assigned() || inputPorts.getCount() == 0)
        {
            // Check if we already have a "No input ports" label
            bool hasNoPortsLabel = false;
            for (int i = 0; i < inputPortsLayout->count(); ++i)
            {
                QLayoutItem* item = inputPortsLayout->itemAt(i);
                if (item && item->widget())
                {
                    auto label = qobject_cast<QLabel*>(item->widget());
                    if (label && label->text() == "No input ports available")
                    {
                        hasNoPortsLabel = true;
                        break;
                    }
                }
            }

            if (!hasNoPortsLabel)
            {
                auto label = new QLabel("No input ports available", this);
                inputPortsLayout->addWidget(label);
            }
            inputPortsLayout->addStretch();
            return;
        }

        // Remove "No input ports" label if it exists
        for (int i = inputPortsLayout->count() - 1; i >= 0; --i)
        {
            QLayoutItem* item = inputPortsLayout->itemAt(i);
            if (item && item->widget())
            {
                auto label = qobject_cast<QLabel*>(item->widget());
                if (label && (label->text() == "No input ports available" || label->text() == "Input ports folder not assigned"))
                {
                    inputPortsLayout->removeWidget(label);
                    label->deleteLater();
                }
            }
        }

        // Build set of current input port global IDs
        QSet<QString> currentPortIds;
        for (const auto& item : inputPorts)
        {
            try
            {
                auto inputPort = item.asPtrOrNull<daq::IInputPort>(true);
                if (!inputPort.assigned())
                    continue;

                QString globalId = QString::fromStdString(inputPort.getGlobalId());
                currentPortIds.insert(globalId);
            }
            catch (const std::exception&)
            {
                // Skip invalid items
            }
        }

        // Remove selectors for ports that no longer exist
        QList<QString> keysToRemove;
        for (auto it = inputPortSelectors.begin(); it != inputPortSelectors.end(); ++it)
        {
            if (!currentPortIds.contains(it.key()))
            {
                inputPortsLayout->removeWidget(it.value());
                it.value()->deleteLater();
                keysToRemove.append(it.key());
            }
        }

        for (const QString& key : keysToRemove)
            inputPortSelectors.remove(key);

        // Add selectors for new ports
        for (const auto& item : inputPorts)
        {
            try
            {
                auto inputPort = item.asPtrOrNull<daq::IInputPort>(true);
                if (!inputPort.assigned())
                    continue;

                QString globalId = QString::fromStdString(inputPort.getGlobalId().toStdString());

                // Check if selector already exists for this port
                if (!inputPortSelectors.contains(globalId))
                {
                    auto selector = new InputPortSignalSelector(inputPort, componentTree, this);
                    inputPortSelectors[globalId] = selector;

                    // Remove stretch temporarily to add widget before it
                    QLayoutItem* stretchItem = nullptr;
                    for (int i = inputPortsLayout->count() - 1; i >= 0; --i)
                    {
                        QLayoutItem* item = inputPortsLayout->itemAt(i);
                        if (item && item->spacerItem())
                        {
                            stretchItem = item;
                            inputPortsLayout->removeItem(stretchItem);
                            break;
                        }
                    }

                    // Add new selector
                    inputPortsLayout->addWidget(selector);

                    // Re-add stretch at the end
                    if (stretchItem)
                        inputPortsLayout->addItem(stretchItem);
                    else
                        inputPortsLayout->addStretch();
                }
            }
            catch (const std::exception& e)
            {
                const auto loggerComponent = AppContext::LoggerComponent();
                LOG_W("Failed to process input port: {}", e.what());
            }
        }
    }
    catch (const std::exception& e)
    {
        auto label = new QLabel(QString("Error getting input ports: %1").arg(e.what()), this);
        inputPortsLayout->addWidget(label);
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error getting input ports: {}", e.what());
    }
}

void InputPortFolderSelector::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    // Events come from input ports folder
    if (sender != inputPortsFolder)
        return;

    try
    {
        auto eventId = static_cast<daq::CoreEventId>(args.getEventId());
        // Events from input ports folder indicate changes in the folder structure
        if (eventId == daq::CoreEventId::ComponentAdded || eventId == daq::CoreEventId::ComponentRemoved)
            QMetaObject::invokeMethod(this, "updateInputPorts", Qt::QueuedConnection);
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error handling input ports folder core event: {}", e.what());
    }
}

