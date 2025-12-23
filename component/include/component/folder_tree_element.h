#pragma once
#include "component_tree_element.h"
#include <opendaq/opendaq.h>

// Forward declaration for factory function
BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QObject* parent);

class FolderTreeElement : public ComponentTreeElement
{
    Q_OBJECT

public:
    FolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent = nullptr);

    void init(BaseTreeElement* parent = nullptr) override;

    // Override visible: hide empty folders
    bool visible() const override;

    // Handle core events - override to handle ComponentAdded/ComponentRemoved
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args) override;

    // Get standard folder name based on component name
    QString getStandardFolderName(const QString& componentName) const;

public Q_SLOTS:
    // Refresh folder contents from openDAQ structure
    void refresh();
};
