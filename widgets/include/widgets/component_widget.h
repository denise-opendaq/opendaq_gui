#pragma once

#include <QWidget>
#include <QVBoxLayout>

#include <opendaq/opendaq.h>

class PropertyObjectView;

class ComponentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ComponentWidget(const daq::ComponentPtr& component, QWidget* parent = nullptr);
    ~ComponentWidget() override;

private:
    void setupUI();
    void createComponentPropertyObject();
    void setupPropertyHandlers();
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

    daq::ComponentPtr component;
    PropertyObjectView* propertyView;
    daq::PropertyObjectPtr componentPropertyObject;
    std::atomic<int> updatingFromComponent; // Counter to prevent recursive updates
    daq::EventPtr<const daq::ComponentPtr, const daq::CoreEventArgsPtr> coreEvent;
};

