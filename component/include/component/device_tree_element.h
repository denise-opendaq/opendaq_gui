#pragma once
#include "folder_tree_element.h"

// Example derived class for Device elements
class DeviceTreeElement : public FolderTreeElement
{
    Q_OBJECT
    using Super = FolderTreeElement;
    

public:
    DeviceTreeElement(QTreeWidget* tree, const daq::DevicePtr& daqDevice, QObject* parent = nullptr)
        : Super(tree, daqDevice, parent)
    {
        this->type = "Device";
        this->iconName = "device";
        this->name = getStandardFolderName(this->name);
    }

    bool visible() const override
    {
        return true;
    }

    // Override onSelected to show device-specific content
    void onSelected(QWidget* mainContent) override
    {
        Super::onSelected(mainContent);
    
        auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
        QString tabName = getName() + " device info";
        auto propertyView = new PropertyObjectView(daqComponent.asPtr<daq::IDevice>(true).getInfo());
        addTab(tabWidget, propertyView, tabName);
    }

    // Override to add device-specific context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override
    {
        QMenu* menu = new QMenu(parent);

        QAction* refreshAction = menu->addAction(IconProvider::instance().icon("refresh"), "Refresh");
        QAction* settingsAction = menu->addAction(IconProvider::instance().icon("settings"), "Settings");
        menu->addSeparator();
        QAction* removeAction = menu->addAction("Remove Device");

        // Connect actions (you would implement these handlers)
        // connect(refreshAction, &QAction::triggered, this, &DeviceTreeElement::onRefresh);
        // connect(settingsAction, &QAction::triggered, this, &DeviceTreeElement::onSettings);
        // connect(removeAction, &QAction::triggered, this, &DeviceTreeElement::onRemove);

        return menu;
    }
};
