#include "component/component_tree_element.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include "widgets/property_object_view.h"
#include "widgets/component_widget.h"
#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>
#include <qt_widget_interface/qt_widget_interface.h>

ComponentTreeElement::ComponentTreeElement(QTreeWidget* tree, const daq::ComponentPtr& daqComponent, QWidget* contentHost, QObject* parent)
    : BaseTreeElement(tree, contentHost, parent)
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
    applyActiveStyle();

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

        return true;
    }
    catch (const std::exception&)
    {
        return true;
    }
}

void ComponentTreeElement::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != daqComponent)
        return;

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
    else if (attributeName == "Active")
    {
        applyActiveStyle();
    }
}

void ComponentTreeElement::applyActiveStyle()
{
    if (!treeItem)
        return;

    bool active = true;
    try
    {
        if (daqComponent.assigned())
            active = daqComponent.getActive();
    }
    catch (...)
    {
        active = true;
    }

    const QPalette pal = tree ? tree->palette() : QApplication::palette();
    const QColor normal = pal.color(QPalette::Normal, QPalette::Text);
    const QColor disabled = pal.color(QPalette::Disabled, QPalette::Text);

    treeItem->setForeground(0, QBrush(active ? normal : disabled));
}

daq::ComponentPtr ComponentTreeElement::getDaqComponent() const
{
    return daqComponent;
}

void ComponentTreeElement::openTab()
{
    if (!contentHost)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("ComponentTreeElement::openTab: contentHost is null for component '{}'", name.toStdString());
        return;
    }

    auto* componentWidget = ComponentWidget::Сreate(daqComponent, nullptr, iconName);
    setContent(componentWidget);
}

QMenu* ComponentTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = BaseTreeElement::onCreateRightClickMenu(parent);

    QAction* beginUpdateAction = menu->addAction("Begin Update");
    connect(beginUpdateAction, &QAction::triggered, this, &ComponentTreeElement::onBeginUpdate);

    QAction* endUpdateAction = menu->addAction("End Update");
    connect(endUpdateAction, &QAction::triggered, this, &ComponentTreeElement::onEndUpdate);

    menu->addSeparator();

    bool active = true;
    bool parentActive = true;
    try
    {
        if (daqComponent.assigned())
        {
            parentActive = daqComponent.getParentActive();
            active = daqComponent.getActive();
        }
    }
    catch (...) {}

    if (parentActive)
    {
        QAction* changeActiveAction = menu->addAction(active ? "Set Inactive" : "Set Active");
        connect(changeActiveAction, &QAction::triggered, this, &ComponentTreeElement::onChangeActive);

        menu->addSeparator();
    }

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

void ComponentTreeElement::onChangeActive()
{
    try
    {
        daqComponent.setActive(!daqComponent.getActive());
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_E("Failed to change active state: {}", e.what());
    }
}
