#pragma once

#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include "DetachableTabWidget.h"
#include "base_tree_element.h"

#include <opendaq/component_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

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

    // Override to return available tabs
    QStringList getAvailableTabNames() const override;
    void openTab(const QString& tabName, QWidget* mainContent) override;

    // Get the underlying openDAQ component
    daq::ComponentPtr getDaqComponent() const;

    // Override to add context menu items
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

protected Q_SLOTS:
    void onBeginUpdate();
    void onEndUpdate();

protected:
    daq::ComponentPtr daqComponent;
};
