#include "widgets/component_widget.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>
#include <set>

// RAII helper to manage updatingFromComponent counter
struct UpdateGuard
{
    UpdateGuard(std::atomic<int>& cnt)
        : cnt(cnt)
    {
        cnt++;
    }

    ~UpdateGuard()
    {
        cnt--;
    }

private:
   std::atomic<int>& cnt;
};

ComponentWidget::ComponentWidget(const daq::ComponentPtr& comp, QWidget* parent)
    : QWidget(parent)
    , component(comp)
    , propertyView(nullptr)
    , componentPropertyObject(nullptr)
    , updatingFromComponent(0)
{
    setupUI();

    if (component.assigned())
    {
       *AppContext::DaqEvent() += daq::event(this, &ComponentWidget::onCoreEvent);
    }

}

ComponentWidget::~ComponentWidget()
{
    // Unregister from core event listener
    *AppContext::DaqEvent() -= daq::event(this, &ComponentWidget::onCoreEvent);
}

void ComponentWidget::setupUI()
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create PropertyObject wrapper for component attributes
    createComponentPropertyObject();
    
    // Use PropertyObjectView to display component attributes
    propertyView = new PropertyObjectView(componentPropertyObject, this, component);
    mainLayout->addWidget(propertyView);
}

void ComponentWidget::createComponentPropertyObject()
{
    if (!component.assigned())
        return;

    try
    {
        // Create PropertyObject for component attributes
        componentPropertyObject = daq::PropertyObject();

        // Get locked attributes
        std::set<std::string> lockedSet;
        for (const auto & attribute : component.getLockedAttributes())
            lockedSet.insert(attribute);

        // Add Name property
        bool nameLocked = lockedSet.find("Name") != lockedSet.end();
        auto nameProp = daq::StringPropertyBuilder("Name", component.getName().toStdString())
            .setReadOnly(nameLocked)
            .build();
        componentPropertyObject.addProperty(nameProp);
        
        // Add Description property
        bool descriptionLocked = lockedSet.find("Description") != lockedSet.end();
        auto descriptionProp = daq::StringPropertyBuilder("Description", component.getDescription().toStdString())
            .setReadOnly(descriptionLocked)
            .build();
        componentPropertyObject.addProperty(descriptionProp);
        
        // Add Active property
        bool activeLocked = lockedSet.find("Active") != lockedSet.end();
        auto activeProp = daq::BoolPropertyBuilder("Active", component.getActive())
            .setReadOnly(activeLocked)
            .build();
        componentPropertyObject.addProperty(activeProp);
        
        // Add Visible property
        bool visibleLocked = lockedSet.find("Visible") != lockedSet.end();
        auto visibleProp = daq::BoolPropertyBuilder("Visible", component.getVisible())
            .setReadOnly(visibleLocked)
            .build();
        componentPropertyObject.addProperty(visibleProp);
        
        // Add Tags property (read-only, as list)
        auto tags = component.getTags();
        auto tagsProp = daq::ListPropertyBuilder("Tags", tags.getList())
            .setReadOnly(true)
            .build();
        componentPropertyObject.addProperty(tagsProp);
        
        // Add Global ID property (read-only)
        auto globalIdProp = daq::StringPropertyBuilder("Global ID", component.getGlobalId().toStdString())
            .setReadOnly(true)
            .build();
        componentPropertyObject.addProperty(globalIdProp);
        
        // Add Local ID property (read-only)
        auto localIdProp = daq::StringPropertyBuilder("Local ID", component.getLocalId().toStdString())
            .setReadOnly(true)
            .build();
        componentPropertyObject.addProperty(localIdProp);

        if (component.supportsInterface<daq::IDevice>())
        {
            auto selection = daq::Dict<daq::IInteger, daq::IString>(
            {
                {static_cast<int>(daq::OperationModeType::Unknown), "Unknown"},
                {static_cast<int>(daq::OperationModeType::Idle), "Idle"},
                {static_cast<int>(daq::OperationModeType::Operation), "Operation"},
                {static_cast<int>(daq::OperationModeType::SafeOperation), "SafeOperation"}, 
            });
            auto opModeProp = daq::SparseSelectionProperty("Operation Mode", selection, static_cast<int>(component.getOperationMode()));
            componentPropertyObject.addProperty(opModeProp);
        }

        // Add Statuses property (read-only, as dict)
        updateStatuses();

        // Set up read/write handlers for each property
        setupPropertyHandlers();

        auto internal = componentPropertyObject.asPtr<daq::IPropertyObjectInternal>(true);
        internal.setPath("daqComponentAttributes");

        internal.setCoreEventTrigger([this](const daq::CoreEventArgsPtr& args) 
        { 
            daq::EventPtr<const daq::ComponentPtr, const daq::CoreEventArgsPtr> coreEvent;
            AppContext::Daq().getContext()->getOnCoreEvent(&coreEvent);
            coreEvent(component, args); 
        });
        internal.enableCoreEventTrigger();

    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error creating component property object: {}", e.what());
    }
}

void ComponentWidget::setupPropertyHandlers()
{
    if (!componentPropertyObject.assigned() || !component.assigned())
        return;

    try
    {
        // Helper lambda to setup property handler with guard
        auto setupHandler = [this](const QString& propName, auto setter) 
        {
            componentPropertyObject.getOnPropertyValueWrite(propName.toStdString()) += 
                [this, setter](daq::PropertyObjectPtr&, daq::PropertyValueEventArgsPtr& args)
                {
                    if (updatingFromComponent == 0)
                    {
                        UpdateGuard guard(updatingFromComponent);
                        setter(args.getValue());
                    }
                };
        };

        // Name property write handler - sync changes back to component
        setupHandler("Name", [this](const daq::BaseObjectPtr& value) 
        {
            component.setName(value.asPtr<daq::IString>(true));
        });

        // Description property write handler
        setupHandler("Description", [this](const daq::BaseObjectPtr& value) 
        {
            component.setDescription(value.asPtr<daq::IString>());
        });

        // Active property write handler
        setupHandler("Active", [this](const daq::BaseObjectPtr& value)
        {
            component.setActive(static_cast<bool>(value));
        });

        // Visible property write handler
        setupHandler("Visible", [this](const daq::BaseObjectPtr& value)
        {
            component.setVisible(static_cast<bool>(value));
        });

        // Operation mode property write handler
        if (componentPropertyObject.hasProperty("Operation Mode"))
        {
            setupHandler("Operation Mode", [this](const daq::BaseObjectPtr& value)
            {
                if (const auto device = component.asPtrOrNull<daq::IDevice>(true); device.assigned())
                    device.setOperationMode(static_cast<daq::OperationModeType>(static_cast<int>(value)));
            });
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error setting up property handlers: {}", e.what());
    }
}

void ComponentWidget::updateStatuses()
{
    if (!component.assigned() || !componentPropertyObject.assigned())
        return;

    try
    {
        daq::ComponentStatusContainerPtr statusContainer = component.getStatusContainer();
        if (!statusContainer.assigned())
            return;

        // Get all statuses
        auto statuses = statusContainer.getStatuses();
        if (!statuses.assigned())
            return;

        // Create a dict with status names and their string values
        daq::DictPtr<daq::IString, daq::IString> statusDict = daq::Dict<daq::IString, daq::IString>();
        for (const auto& [name, statusEnum] : statuses)
        {
            QString statusValue = QString::fromStdString(statusEnum.getValue().toStdString());
            QString statusMessage;
            try
            {
                daq::StringPtr messagePtr = statusContainer.getStatusMessage(name);
                if (messagePtr.assigned() && messagePtr.getLength())
                {
                    statusMessage = QString::fromStdString(messagePtr.toStdString());
                    statusValue += QString(" (%1)").arg(statusMessage);
                }
            }
            catch (const std::exception& e)
            {
                // If message is not available, just use the value
                const auto loggerComponent = AppContext::LoggerComponent();
                LOG_D("Could not get status message for '{}': {}", name.toStdString(), e.what());
            }
            statusDict.set(name, daq::String(statusValue.toStdString()));
        }

        // Check if Statuses property already exists
        if (componentPropertyObject.hasProperty("Statuses"))
        {
            UpdateGuard guard(updatingFromComponent);
            auto protectedObj = componentPropertyObject.asPtr<daq::IPropertyObjectProtected>(true);
            protectedObj.setProtectedPropertyValue("Statuses", statusDict);
        }
        else
        {
            // Add Statuses property (read-only, as dict)
            auto statusesProp = daq::DictPropertyBuilder("Statuses", statusDict)
                .setReadOnly(true)
                .build();
            componentPropertyObject.addProperty(statusesProp);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error updating statuses: {}", e.what());
    }
}

void ComponentWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != component)
        return;

    // Ignore events that we triggered ourselves
    if (updatingFromComponent > 0)
        return;

    try
    {
        const auto eventId = static_cast<daq::CoreEventId>(args.getEventId());

        if (eventId == daq::CoreEventId::AttributeChanged)
        {
            const daq::StringPtr attributeName = args.getParameters().get("AttributeName");
            const daq::BaseObjectPtr attributeValue = args.getParameters().get(attributeName);
            handleAttributeChangedAsync(attributeName, attributeValue);
        }
        else if (eventId == daq::CoreEventId::TagsChanged)
        {
            daq::TagsPtr tags = args.getParameters()["Tags"];
            handleTagsChangedAsync(tags);
        }
        else if (eventId == daq::CoreEventId::StatusChanged)
        {
            handleStatusChangedAsync();
        }
        else if (eventId == daq::CoreEventId::DeviceOperationModeChanged)
        {
            handleAttributeChangedAsync("Operation Mode", args.getParameters().get("OperationMode"));
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error handling core event: {}", e.what());
    }
}

void ComponentWidget::handleAttributeChangedAsync(const daq::StringPtr& attributeName, const daq::BaseObjectPtr& value)
{
    if (updatingFromComponent > 0 || !componentPropertyObject.assigned())
        return;

    try
    {
        if (componentPropertyObject.hasProperty(attributeName))
        {
            UpdateGuard guard(updatingFromComponent);
            auto protectedObj = componentPropertyObject.asPtr<daq::IPropertyObjectProtected>(true);
            protectedObj.setProtectedPropertyValue(attributeName, value);
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error handling attribute changed async: {}", e.what());
    }
}

void ComponentWidget::handleTagsChangedAsync(const daq::TagsPtr& tags)
{
    if (updatingFromComponent > 0 || !componentPropertyObject.assigned())
        return;

    try
    {
        UpdateGuard guard(updatingFromComponent);
        auto protectedObj = componentPropertyObject.asPtr<daq::IPropertyObjectProtected>(true);
        protectedObj.setProtectedPropertyValue("Tags", tags.getList());
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error handling tags changed async: {}", e.what());
    }
}

void ComponentWidget::handleStatusChangedAsync()
{
    if (updatingFromComponent > 0)
        return;

    updateStatuses();
}