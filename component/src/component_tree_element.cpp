#include "component/component_tree_element.h"
#include "context/AppContext.h"
#include "DetachableTabWidget.h"
#include "widgets/property_object_view.h"
#include <opendaq/opendaq.h>

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
        daqComponent.getOnComponentCoreEvent() += daq::event(this, &ComponentTreeElement::onCoreEvent);
    }
    catch (const std::exception& e)
    {
        qWarning() << "Failed to subscribe to component events:" << e.what();
    }
}

ComponentTreeElement::~ComponentTreeElement()
{
    // Unsubscribe from events
    try
    {
        if (daqComponent.assigned())
        {
            daqComponent.getOnComponentCoreEvent() -= daq::event(this, &ComponentTreeElement::onCoreEvent);
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Failed to unsubscribe from component events:" << e.what();
    }
}

bool ComponentTreeElement::visible() const
{
    try
    {
        // Check if component is visible
        bool componentVisible = daqComponent.getVisible();

        // If component is not visible and we're not showing hidden, hide it
        if (!componentVisible && !AppContext::instance()->showInvisibleComponents())
            return false;

        // Check if we're filtering by component type
        QSet<QString> allowedTypes = AppContext::instance()->showComponentTypes();
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
        auto eventName = args.getEventName();

        if (eventName == "AttributeChanged")
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
        qWarning() << "Error handling core event:" << e.what();
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
            qWarning() << "Error updating name:" << e.what();
        }
    }
}

void ComponentTreeElement::onSelected(QWidget* mainContent)
{
    // Open all available tabs by calling openTab for each
    QStringList availableTabs = getAvailableTabNames();
    for (const QString& tabName : availableTabs) {
        openTab(tabName, mainContent);
    }
}

void ComponentTreeElement::addTab(DetachableTabWidget* tabWidget, QWidget* tab, const QString & tabName)
{
    for (int i = 0; i < tabWidget->count(); ++i)
    {
        if (tabWidget->tabText(i) == tabName)
        {
            tabWidget->setCurrentIndex(i);
            return;
        }
    }

    int index = tabWidget->addTab(tab, tabName);
    // Store component globalId as property on the tab widget so we can find and close tabs when component is removed
    tab->setProperty("componentGlobalId", globalId);
    tabWidget->setCurrentIndex(index);
}

daq::ComponentPtr ComponentTreeElement::getDaqComponent() const
{
    return daqComponent;
}

QStringList ComponentTreeElement::getAvailableTabNames() const
{
    QStringList tabs;
    tabs << (getName() + " Properties");
    return tabs;
}

void ComponentTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
    if (!tabWidget)
        return;

    QString expectedName = getName() + " Properties";
    if (tabName == expectedName) {
        auto propertyView = new PropertyObjectView(daqComponent, nullptr, daqComponent);
        addTab(tabWidget, propertyView, tabName);
    }
}

