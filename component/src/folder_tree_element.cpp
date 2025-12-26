#include "component/folder_tree_element.h"
#include "component/component_factory.h"
#include <opendaq/custom_log.h>
#include <QSet>
#include <QMetaObject>

FolderTreeElement::FolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent)
    : ComponentTreeElement(tree, daqFolder, parent)
{
    this->type = "Folder";
    this->iconName = "folder";
    this->name = getStandardFolderName(this->name);
}

void FolderTreeElement::init(BaseTreeElement* parent)
{
    ComponentTreeElement::init(parent);

    // Add all items from the folder as children
    try
    {
        auto folder = daqComponent.asPtr<daq::IFolder>();
        auto items = folder.getItems();

        for (size_t i = 0; i < items.getCount(); ++i)
        {
            auto item = items[i];
            auto childElement = createTreeElement(tree, item, this);
            if (childElement)
            {
                addChild(childElement);
            }
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error initializing folder children: {}", e.what());
    }
}

bool FolderTreeElement::visible() const
{
    if (children.isEmpty())
    {
        return false;
    }
    return ComponentTreeElement::visible();
}

void FolderTreeElement::refresh()
{
    try
    {
        auto folder = daqComponent.asPtr<daq::IFolder>();
        auto items = folder.getItems();

        // Add new items that don't exist yet
        for (size_t i = 0; i < items.getCount(); ++i)
        {
            auto item = items[i];
            QString itemLocalId = QString::fromStdString(item.getLocalId());
            
            if (!children.contains(itemLocalId))
            {
                // This is a new item, add it
                auto childElement = createTreeElement(tree, item, this);
                if (childElement)
                {
                    addChild(childElement);
                }
            }
        }

        // Remove items that no longer exist in openDAQ structure
        QList<BaseTreeElement*> toRemove;
        for (auto* child : children.values())
        {
            QString childLocalId = child->getLocalId();
            bool found = false;
            
            for (size_t i = 0; i < items.getCount(); ++i)
            {
                auto item = items[i];
                QString itemLocalId = QString::fromStdString(item.getLocalId());
                if (itemLocalId == childLocalId)
                {
                    found = true;
                    break;
                }
            }
            
            if (!found)
            {
                toRemove.append(child);
            }
        }
        
        for (auto* child : toRemove)
        {
            removeChild(child);
        }
        // Update visibility after refresh
        showFiltered();
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error refreshing folder children: {}", e.what());
    }
}

void FolderTreeElement::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    // First call parent implementation to handle AttributeChanged events
    ComponentTreeElement::onCoreEvent(sender, args);

    try
    {
        auto eventId = static_cast<daq::CoreEventId>(args.getEventId());
        if (eventId == daq::CoreEventId::ComponentAdded || eventId == daq::CoreEventId::ComponentRemoved)
        {
            // Refresh the folder to update the tree structure
            QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error handling folder core event: {}", e.what());
    }
}

QString FolderTreeElement::getStandardFolderName(const QString& componentName) const
{
    if (componentName == "Sig")
        return "Signals";
    if (componentName == "FB")
        return "Function blocks";
    if (componentName == "Dev")
        return "Devices";
    if (componentName == "IP")
        return "Input ports";
    if (componentName == "IO")
        return "Inputs/Outputs";
    if (componentName == "Srv")
        return "Servers";

    return componentName;
}

