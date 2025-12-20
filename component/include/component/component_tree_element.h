#pragma once
#include "base_tree_element.h"
#include "property/property_object_view.h"
#include "AppContext.h"
#include <opendaq/opendaq.h>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>

class ComponentTreeElement : public BaseTreeElement
{
    Q_OBJECT

public:
    ComponentTreeElement(QTreeWidget* tree, const daq::ComponentPtr& daqComponent, QObject* parent = nullptr)
        : BaseTreeElement(tree, parent)
        , daqComponent(daqComponent)
    {
        this->localId = QString::fromStdString(daqComponent.getLocalId().toStdString());
        this->globalId = QString::fromStdString(daqComponent.getGlobalId().toStdString());
        this->name = QString::fromStdString(daqComponent.getName().toStdString());
        this->type = "Component";
    }

    void init(BaseTreeElement* parent = nullptr) override
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

    ~ComponentTreeElement() override
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

    // Override visible property
    bool visible() const override
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

    // Handle core events from openDAQ component
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
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

    // Handle attribute changes
    void onChangedAttribute(const QString& attributeName, const daq::ObjectPtr<daq::IBaseObject>& value)
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

    // Override onSelected to show component properties
    void onSelected(QWidget* mainContent) override
    {
        // Clear previous content
        QLayout* layout = mainContent->layout();
        if (layout)
        {
            QLayoutItem* item;
            while ((item = layout->takeAt(0)) != nullptr)
            {
                delete item->widget();
                delete item;
            }
        }
        else
        {
            layout = new QVBoxLayout(mainContent);
            mainContent->setLayout(layout);
        }

        // Create Properties section
        QGroupBox* propertiesGroup = new QGroupBox("Properties");
        QVBoxLayout* propertiesLayout = new QVBoxLayout(propertiesGroup);

        // Add PropertyObjectView
        auto propertyView = new PropertyObjectView(daqComponent.asPtr<daq::IPropertyObject>());
        propertiesLayout->addWidget(propertyView);

        layout->addWidget(propertiesGroup);

        // Add stretch to push content to top
        static_cast<QVBoxLayout*>(layout)->addStretch();
    }

    // Get the underlying openDAQ component
    daq::ComponentPtr getDaqComponent() const
    {
        return daqComponent;
    }

protected:
    daq::ComponentPtr daqComponent;
};
