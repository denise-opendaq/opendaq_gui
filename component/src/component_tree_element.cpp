#include "component/component_tree_element.h"
#include "context/AppContext.h"
#include "DetachableTabWidget.h"
#include "widgets/property_object_view.h"
#include "widgets/component_widget.h"
#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

ComponentTreeElement::ComponentTreeElement(QTreeWidget* tree, const daq::ComponentPtr& daqComponent, QObject* parent)
    : BaseTreeElement(tree, parent)
    , daqComponent(daqComponent)
{
    this->localId = QString::fromStdString(daqComponent.getLocalId().toStdString());
    this->globalId = QString::fromStdString(daqComponent.getGlobalId().toStdString());
    this->name = QString::fromStdString(daqComponent.getName().toStdString());
    this->type = "Component";
}

void ComponentTreeElement::init(BaseTreeElement* parent)
{
    BaseTreeElement::init(parent);

    // Subscribe to component core events
    try
    {
        *AppContext::DaqEvent() += daq::event(this, &ComponentTreeElement::onCoreEvent);
    }
    catch (const std::exception& e)
    {
        const auto context = daqComponent.getContext();
        const auto loggerComponent = context.getLogger().getOrAddComponent("openDAQ GUI");
        LOG_W("Failed to subscribe to component events: {}", e.what());
    }
}

ComponentTreeElement::~ComponentTreeElement()
{
    // Unsubscribe from events
    try
    {
        if (daqComponent.assigned())
            *AppContext::DaqEvent() -= daq::event(this, &ComponentTreeElement::onCoreEvent);
    }
    catch (const std::exception& e)
    {
        const auto context = daqComponent.getContext();
        const auto loggerComponent = context.getLogger().getOrAddComponent("openDAQ GUI");
        LOG_W("Failed to unsubscribe from component events: {}", e.what());
    }
}

bool ComponentTreeElement::visible() const
{
    try
    {
        // Check if component is visible
        bool componentVisible = daqComponent.getVisible();

        // If component is not visible and we're not showing hidden, hide it
        if (!componentVisible && !AppContext::Instance()->showInvisibleComponents())
            return false;

        // Check if we're filtering by component type
        QSet<QString> allowedTypes = AppContext::Instance()->showComponentTypes();
        if (allowedTypes.isEmpty())
            return true;

        return allowedTypes.contains(type);
    }
    catch (const std::exception&)
    {
        return true;
    }
}

void ComponentTreeElement::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    try
    {
        auto eventId = static_cast<daq::CoreEventId>(args.getEventId());
        if (eventId == daq::CoreEventId::AttributeChanged)
        {
            auto params = args.getParameters();
            auto attributeName = params.get("AttributeName");

            if (params.hasKey(attributeName))
            {
                auto attributeValue = params.get(attributeName);
                onChangedAttribute(
                    QString::fromStdString(attributeName),
                    attributeValue
                );
            }
        }
    }
    catch (const std::exception& e)
    {
        const auto context = daqComponent.getContext();
        const auto loggerComponent = context.getLogger().getOrAddComponent("openDAQ GUI");
        LOG_W("Error handling core event: {}", e.what());
    }
}

void ComponentTreeElement::onChangedAttribute(const QString& attributeName, const daq::ObjectPtr<daq::IBaseObject>& value)
{
    if (attributeName == "Name")
    {
        try
        {
            QString newName = QString::fromStdString(value.toString());
            setName(newName);
        }
        catch (const std::exception& e)
        {
            const auto context = daqComponent.getContext();
            const auto loggerComponent = context.getLogger().getOrAddComponent("openDAQ GUI");
            LOG_W("Error updating name: {}", e.what());
        }
    }
}

daq::ComponentPtr ComponentTreeElement::getDaqComponent() const
{
    return daqComponent;
}

QStringList ComponentTreeElement::getAvailableTabNames() const
{
    QStringList tabs;
    tabs << (getName() + " Component");
    tabs << (getName() + " Properties");
    return tabs;
}

void ComponentTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
    if (!tabWidget)
        return;

    QString componentTabName = getName() + " Component";
    QString propertiesTabName = getName() + " Properties";

    if (tabName == componentTabName)
    {
        auto componentWidget = new ComponentWidget(daqComponent);
        addTab(tabWidget, componentWidget, tabName);
    }
    else if (tabName == propertiesTabName)
    {
        auto propertyView = new PropertyObjectView(daqComponent, tabWidget, daqComponent);
        addTab(tabWidget, propertyView, tabName);
    }
}

QMenu* ComponentTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = BaseTreeElement::onCreateRightClickMenu(parent);

    QAction* beginUpdateAction = menu->addAction("Begin Update");
    connect(beginUpdateAction, &QAction::triggered, this, &ComponentTreeElement::onBeginUpdate);

    QAction* endUpdateAction = menu->addAction("End Update");
    connect(endUpdateAction, &QAction::triggered, this, &ComponentTreeElement::onEndUpdate);
    
    menu->addSeparator();
    
    return menu;
}

void ComponentTreeElement::onBeginUpdate()
{
    try
    {
        daqComponent.beginUpdate();
    }
    catch (const std::exception& e)
    {
        const auto context = daqComponent.getContext();
        const auto loggerComponent = context.getLogger().getOrAddComponent("openDAQ GUI");
        LOG_E("Failed to begin update: {}", e.what());
    }
}

void ComponentTreeElement::onEndUpdate()
{
    try
    {
        daqComponent.endUpdate();
    }
    catch (const std::exception& e)
    {
        const auto context = daqComponent.getContext();
        const auto loggerComponent = context.getLogger().getOrAddComponent("openDAQ GUI");
        LOG_E("Failed to end update: {}", e.what());
    }
}
