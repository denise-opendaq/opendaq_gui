#include "widgets/function_block_widget.h"
#include "widgets/input_port_signal_selector.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QMap>
#include <QSet>

// FunctionBlockWidget implementation
FunctionBlockWidget::FunctionBlockWidget(const daq::FunctionBlockPtr& functionBlock, ComponentTreeWidget* componentTree, QWidget* parent)
    : QWidget(parent)
    , functionBlock(functionBlock)
    , inputPortsFolder(functionBlock.getItem("IP"))
    , componentTree(componentTree)
    , mainLayout(nullptr)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    mainLayout = layout;

    inputPortsFolder.getOnComponentCoreEvent() += daq::event(this, &FunctionBlockWidget::onCoreEvent);
    setupInputPorts();
}

FunctionBlockWidget::~FunctionBlockWidget()
{
    inputPortsFolder.getOnComponentCoreEvent() -= daq::event(this, &FunctionBlockWidget::onCoreEvent);
}

void FunctionBlockWidget::setupInputPorts()
{
    // Clear existing widgets
    QList<InputPortSignalSelector*> selectorsToRemove = inputPortSelectors.values();
    for (auto* selector : selectorsToRemove)
    {
        mainLayout->removeWidget(selector);
        selector->deleteLater();
    }
    inputPortSelectors.clear();

    // Remove stretch if it exists
    QLayoutItem* item;
    while ((item = mainLayout->takeAt(mainLayout->count() - 1)) != nullptr)
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

void FunctionBlockWidget::updateInputPorts()
{
    if (!functionBlock.assigned())
    {
        auto label = new QLabel("Function block not assigned", this);
        mainLayout->addWidget(label);
        return;
    }

    try
    {
        // Get all input ports from the function block
        auto inputPorts = functionBlock.getInputPorts();
        
        if (!inputPorts.assigned() || inputPorts.getCount() == 0)
        {
            // Check if we already have a "No input ports" label
            bool hasNoPortsLabel = false;
            for (int i = 0; i < mainLayout->count(); ++i)
            {
                QLayoutItem* item = mainLayout->itemAt(i);
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
                mainLayout->addWidget(label);
            }
            mainLayout->addStretch();
            return;
        }

        // Remove "No input ports" label if it exists
        for (int i = mainLayout->count() - 1; i >= 0; --i)
        {
            QLayoutItem* item = mainLayout->itemAt(i);
            if (item && item->widget())
            {
                auto label = qobject_cast<QLabel*>(item->widget());
                if (label && (label->text() == "No input ports available" || label->text() == "Function block not assigned"))
                {
                    mainLayout->removeWidget(label);
                    label->deleteLater();
                }
            }
        }

        // Build set of current input port global IDs
        QSet<QString> currentPortIds;
        for (size_t i = 0; i < inputPorts.getCount(); ++i)
        {
            try
            {
                auto inputPort = inputPorts[i];
                if (inputPort.assigned())
                {
                    QString globalId = QString::fromStdString(inputPort.getGlobalId().toStdString());
                    currentPortIds.insert(globalId);
                }
            } 
            catch (const std::exception&)
            {
                // Skip invalid ports
            }
        }

        // Remove selectors for ports that no longer exist
        QList<QString> keysToRemove;
        for (auto it = inputPortSelectors.begin(); it != inputPortSelectors.end(); ++it)
        {
            if (!currentPortIds.contains(it.key()))
            {
                mainLayout->removeWidget(it.value());
                it.value()->deleteLater();
                keysToRemove.append(it.key());
            }
        }

        for (const QString& key : keysToRemove)
            inputPortSelectors.remove(key);

        // Add selectors for new ports
        for (size_t i = 0; i < inputPorts.getCount(); ++i)
        {
            try 
            {
                auto inputPort = inputPorts[i];
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
                    for (int j = mainLayout->count() - 1; j >= 0; --j)
                    {
                        QLayoutItem* item = mainLayout->itemAt(j);
                        if (item && item->spacerItem())
                        {
                            stretchItem = item;
                            mainLayout->removeItem(stretchItem);
                            break;
                        }
                    }
                    
                    // Add new selector
                    mainLayout->addWidget(selector);
                    
                    // Re-add stretch at the end
                    if (stretchItem)
                        mainLayout->addItem(stretchItem);
                    else
                        mainLayout->addStretch();
                }
            }
            catch (const std::exception& e)
            {
                qWarning() << "Failed to process input port" << i << ":" << e.what();
            }
        }
    }
    catch (const std::exception& e)
    {
        auto label = new QLabel(QString("Error getting input ports: %1").arg(e.what()), this);
        mainLayout->addWidget(label);
        qWarning() << "Error getting input ports:" << e.what();
    }
}

void FunctionBlockWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
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
        qWarning() << "Error handling function block core event:" << e.what();
    }
}

