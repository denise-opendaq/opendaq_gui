#pragma once
#include "folder_tree_element.h"
#include <opendaq/device_ptr.h>

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

    // Override to add device-specific context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args) override;

public Q_SLOTS:
    void onAddDevice();
    void onAddFunctionBlock();
    void onShowDeviceInfo();

private Q_SLOTS:
    void onRemoveDevice();
};
