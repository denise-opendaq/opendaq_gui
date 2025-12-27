#include "component/server_tree_element.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include "component/servers_folder_tree_element.h"

ServerTreeElement::ServerTreeElement(QTreeWidget* tree, const daq::ServerPtr& daqServer, QObject* parent)
    : FolderTreeElement(tree, daqServer, parent)
{
    this->type = "Server";
    this->iconName = "server";
}

QMenu* ServerTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

    if (!parentElement)
        return menu;

    auto parentComponentElement = dynamic_cast<ServersFolderTreeElement*>(parentElement);
    if (!parentComponentElement)
        return menu;

    if (!parentComponentElement->isLocalDeviceFolder())
        return menu;

    menu->addSeparator();
    QAction* removeAction = menu->addAction("Remove");
    connect(removeAction, &QAction::triggered, this, &ServerTreeElement::onRemove);

    return menu;
}

void ServerTreeElement::onRemove()
{
    auto parentElement = getParent();
    if (!parentElement)
        return;

    // Servers are typically managed at the instance level
    // This is a placeholder - actual removal logic depends on openDAQ API
    try
    {
        AppContext::instance()->daqInstance().removeServer(daqComponent);
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(nullptr, "Error",
            QString("Failed to remove server: %1").arg(e.what()));
    }
}

