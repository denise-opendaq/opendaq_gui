#include "component/device_tree_element.h"
#include "DetachableTabWidget.h"
#include "widgets/property_object_view.h"
#include "component/icon_provider.h"
#include <QMenu>
#include <QAction>

DeviceTreeElement::DeviceTreeElement(QTreeWidget* tree, const daq::DevicePtr& daqDevice, QObject* parent)
    : Super(tree, daqDevice, parent)
{
    this->type = "Device";
    this->iconName = "device";
    this->name = getStandardFolderName(this->name);
}

bool DeviceTreeElement::visible() const
{
    return true;
}

void DeviceTreeElement::onSelected(QWidget* mainContent)
{
    Super::onSelected(mainContent);

    auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
    QString tabName = getName() + " device info";
    auto propertyView = new PropertyObjectView(daqComponent.asPtr<daq::IDevice>(true).getInfo());
    addTab(tabWidget, propertyView, tabName);
}

QMenu* DeviceTreeElement::onCreateRightClickMenu(QWidget* parent)
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

