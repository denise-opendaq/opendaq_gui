#pragma once
#include "folder_tree_element.h"

// Example derived class for Device elements
class DeviceTreeElement : public FolderTreeElement
{
    Q_OBJECT
    using Super = FolderTreeElement;
    

public:
    DeviceTreeElement(QTreeWidget* tree, const daq::DevicePtr& daqDevice, QObject* parent = nullptr);

    bool visible() const override;

    // Override onSelected to show device-specific content
    void onSelected(QWidget* mainContent) override;
    QStringList getAvailableTabNames() const override;
    void openTab(const QString& tabName, QWidget* mainContent) override;

    // Override to add device-specific context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;
};
