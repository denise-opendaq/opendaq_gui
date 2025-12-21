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
        {
            return false;
        }

        // Check if we're filtering by component type
        QStringList allowedTypes = AppContext::instance()->showComponentTypes();
        if (!allowedTypes.isEmpty())
        {
            // If type filter is set, check if this component's type is allowed
            if (!allowedTypes.contains(type))
            {
                return false;
            }
        }

        return true;
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
            auto attributeName = params.get("AttributeName").toString();

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
    auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
    QString tabName = getName() + " Properties";
    auto propertyView = new PropertyObjectView(daqComponent.asPtr<daq::IPropertyObject>());
    addTab(tabWidget, propertyView, tabName);
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

    tabWidget->addTab(tab, tabName);
    tabWidget->setCurrentIndex(tabWidget->count() - 1);
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
        auto propertyView = new PropertyObjectView(daqComponent.asPtr<daq::IPropertyObject>());
        addTab(tabWidget, propertyView, tabName);
    }
}

