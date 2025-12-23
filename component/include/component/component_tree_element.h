#pragma once
#include "DetachableTabWidget.h"
#include "base_tree_element.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include <opendaq/opendaq.h>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>

class ComponentTreeElement : public BaseTreeElement
{
    Q_OBJECT

public:
    ComponentTreeElement(QTreeWidget* tree, const daq::ComponentPtr& daqComponent, QObject* parent = nullptr);
    ~ComponentTreeElement() override;

    void init(BaseTreeElement* parent = nullptr) override;

    // Override visible property
    bool visible() const override;

    // Handle core events from openDAQ component
    virtual void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

    // Handle attribute changes
    void onChangedAttribute(const QString& attributeName, const daq::ObjectPtr<daq::IBaseObject>& value);

    // Override onSelected to show component properties
    void onSelected(QWidget* mainContent) override;

    // Override to return available tabs
    QStringList getAvailableTabNames() const override;
    void openTab(const QString& tabName, QWidget* mainContent) override;

    void addTab(DetachableTabWidget* tabWidget, QWidget* tab, const QString & tabName);

    // Get the underlying openDAQ component
    daq::ComponentPtr getDaqComponent() const;

protected:
    daq::ComponentPtr daqComponent;
};
