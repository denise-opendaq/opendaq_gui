#pragma once
#include <QTreeWidget>
#include "component_factory.h"
#include "base_tree_element.h"
#include "context/AppContext.h"
#include <opendaq/opendaq.h>

// Main widget that wraps QTreeWidget for openDAQ components
class ComponentTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    ComponentTreeWidget(QWidget* parent = nullptr);
    ~ComponentTreeWidget() override;

    // Load openDAQ instance into the tree
    void loadInstance(const daq::InstancePtr& instance);

    // Get the selected BaseTreeElement
    BaseTreeElement* getSelectedElement() const;

    // Find element by globalId
    BaseTreeElement* findElementByGlobalId(const QString& globalId) const;

    // Set whether to show hidden components
    void setShowHidden(bool show);

    // Set component type filter
    void setComponentTypeFilter(const QSet<QString>& types);

    // Refresh visibility based on current filters
    void refreshVisibility();

Q_SIGNALS:
    // Emitted when a component is selected in the tree
    void componentSelected(BaseTreeElement* element);

private:
    void setupUI();

private Q_SLOTS:
    void onSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);

private:
    BaseTreeElement* rootElement;
};
