#include "component/component_tree_element.h"
#include "context/AppContext.h"
#include "widgets/property_object_view.h"
#include "widgets/component_widget.h"
#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>
#include <qt_widget_interface/qt_widget_interface.h>

ComponentTreeElement::ComponentTreeElement(QTreeWidget* tree, const daq::ComponentPtr& daqComponent, LayoutManager* layoutManager, QObject* parent)
    : BaseTreeElement(tree, layoutManager, parent)
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
        const auto loggerComponent = AppContext::LoggerComponent();
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
        const auto loggerComponent = AppContext::LoggerComponent();
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
        const auto loggerComponent = AppContext::LoggerComponent();
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
            const auto loggerComponent = AppContext::LoggerComponent();
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
    tabs << "Attributes";
    tabs << "Properties";
    if (daqComponent.supportsInterface<IQTWidget>())
       tabs << "QTWidget";
    return tabs;
}

void ComponentTreeElement::openTab(const QString& tabName)
{
    if (!layoutManager)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("ComponentTreeElement::openTab: layoutManager is null for component '{}'", name.toStdString());
        return;
    }
        
    if (tabName == "Attributes")
    {
        auto* componentWidget = new ComponentWidget(daqComponent);
        addTab(componentWidget, tabName);
    }
    else if (tabName == "Properties")
    {
        auto* propertyView = new PropertyObjectView(daqComponent, nullptr, daqComponent);
        addTab(propertyView, tabName, LayoutZone::Bottom, "Attributes");
    }
    else if (tabName == "QTWidget")
    {
        auto widgetComponent = daqComponent.asPtrOrNull<IQTWidget>(true);
        if (widgetComponent.assigned())
        {
            QWidget* qtWidget;
            widgetComponent->getWidget(&qtWidget);
            if (qtWidget)
            {
                // QTWidget opens in left zone
                addTab(qtWidget, tabName, LayoutZone::Left);
            }
        }
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
        const auto loggerComponent = AppContext::LoggerComponent();
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
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_E("Failed to end update: {}", e.what());
    }
}
