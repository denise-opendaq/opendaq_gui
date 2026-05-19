#pragma once
#include "component_tree_element.h"
#include <opendaq/folder_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class QTabWidget;
BaseTreeElement* createTreeElement(QTreeWidget* tree, const daq::ComponentPtr& component, QTabWidget* contentTabs, QObject* parent);

class FolderTreeElement : public ComponentTreeElement
{
    Q_OBJECT

public:
    FolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QTabWidget* contentTabs, QObject* parent = nullptr);

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
