#include "widgets/component_widget.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include <QMetaObject>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

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
    
    // Subscribe to component core events
    if (component.assigned())
    {
        try
        {
            AppContext::daq().getContext()->getOnCoreEvent(&coreEvent);
            component.getOnComponentCoreEvent() += daq::event(this, &ComponentWidget::onCoreEvent);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = AppContext::getLoggerComponent();
            LOG_W("Failed to subscribe to component events: {}", e.what());
        }
    }
}

ComponentWidget::~ComponentWidget()
{
    // Unsubscribe from events
    if (component.assigned())
    {
        try
        {
            component.getOnComponentCoreEvent() -= daq::event(this, &ComponentWidget::onCoreEvent);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = AppContext::getLoggerComponent();
            LOG_W("Failed to unsubscribe from component events: {}", e.what());
        }
    }
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

        // Set up read/write handlers for each property
        setupPropertyHandlers();

        auto internal = componentPropertyObject.asPtr<daq::IPropertyObjectInternal>(true);
        internal.setPath("daqComponentAttributes");
        internal.setCoreEventTrigger([this](const daq::CoreEventArgsPtr& args) { coreEvent(component, args); });
        internal.enableCoreEventTrigger();

    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error creating component property object: {}", e.what());
    }
}

void ComponentWidget::setupPropertyHandlers()
{
    if (!componentPropertyObject.assigned() || !component.assigned())
        return;

    try
    {
        // Name property write handler - sync changes back to component
        componentPropertyObject.getOnPropertyValueWrite("Name") += [this](daq::PropertyObjectPtr& obj, daq::PropertyValueEventArgsPtr& args) 
        {
            if (updatingFromComponent == 0)
            {
                UpdateGuard guard(updatingFromComponent);
                component.setName(args.getValue().asPtr<daq::IString>());
            }
        };

        // Description property write handler
        componentPropertyObject.getOnPropertyValueWrite("Description") += [this](daq::PropertyObjectPtr& obj, daq::PropertyValueEventArgsPtr& args) 
        {
            if (updatingFromComponent == 0)
            {
                UpdateGuard guard(updatingFromComponent);
                component.setDescription(args.getValue().asPtr<daq::IString>());
            }
        };

        // Active property write handler
        componentPropertyObject.getOnPropertyValueWrite("Active") += [this](daq::PropertyObjectPtr& obj, daq::PropertyValueEventArgsPtr& args) 
        {
            if (updatingFromComponent == 0)
            {
                UpdateGuard guard(updatingFromComponent);
                component.setActive(static_cast<bool>(args.getValue()));
            }
        };

        // Visible property write handler
        componentPropertyObject.getOnPropertyValueWrite("Visible") += [this](daq::PropertyObjectPtr& obj, daq::PropertyValueEventArgsPtr& args) 
        {
            if (updatingFromComponent == 0)
            {
                UpdateGuard guard(updatingFromComponent);
                component.setVisible(static_cast<bool>(args.getValue()));
            }
        };
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error setting up property handlers: {}", e.what());
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
            const auto attributeValue = args.getParameters().get(attributeName);
            const auto currentValue = componentPropertyObject.getPropertyValue(attributeName);

            UpdateGuard guard(updatingFromComponent);
            auto protectedObj = componentPropertyObject.asPtr<daq::IPropertyObjectProtected>(true);
            protectedObj.setProtectedPropertyValue(attributeName, attributeValue);
        }
        else if (eventId == daq::CoreEventId::TagsChanged)
        {
            // Update Tags from component as list
            daq::TagsPtr tags = args.getParameters()["Tags"];

            UpdateGuard guard(updatingFromComponent);
            auto protectedObj = componentPropertyObject.asPtr<daq::IPropertyObjectProtected>(true);
            protectedObj.setProtectedPropertyValue("Tags", tags.getList());
        }
        else 
        {
            return;
        }
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::getLoggerComponent();
        LOG_W("Error handling core event: {}", e.what());
    }
}
