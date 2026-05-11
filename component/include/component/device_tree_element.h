#pragma once
#include "folder_tree_element.h"
#include <opendaq/device_ptr.h>

// Example derived class for Device elements
class DeviceTreeElement : public FolderTreeElement
{
    Q_OBJECT
    using Super = FolderTreeElement;
    

public:
    DeviceTreeElement(QTreeWidget* tree, const daq::DevicePtr& daqDevice, LayoutManager* layoutManager, QObject* parent = nullptr);

    void init(BaseTreeElement* parent = nullptr) override;

    bool visible() const override;

    // Override to add device-specific context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args) override;

    QStringList getAvailableTabNames() const override;
    void openTab(const QString& tabName) override;

public Q_SLOTS:
    void onAddDevice();
    void onAddFunctionBlock();
    void onShowDeviceInfo();
    void onSaveConfiguration();
    void onLoadConfiguration();

private Q_SLOTS:
    void onRemoveDevice();

private:
    QString connectionStatus;
    void updateDeviceLabel();
    static QString operationModeToString(daq::OperationModeType mode);
};
