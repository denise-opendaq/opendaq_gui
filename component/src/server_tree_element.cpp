#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include "component/server_tree_element.h"
#include "component/servers_folder_tree_element.h"
#include "context/AppContext.h"
#include <opendaq/instance_ptr.h>
#include <opendaq/server_ptr.h>

ServerTreeElement::ServerTreeElement(QTreeWidget* tree, const daq::ServerPtr& daqServer, LayoutManager* layoutManager, QObject* parent)
    : FolderTreeElement(tree, daqServer, layoutManager, parent)
    , discoveryIsEnabled(false)
{
    this->type = "Server";
    this->iconName = "server";
}

QMenu* ServerTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

    QAction* firstAction = menu->actions().isEmpty() ? nullptr : menu->actions().first();

    if (discoveryIsEnabled)
    {
        QAction* disableDiscoveryAction = new QAction("Disable Discovery", menu);
        connect(disableDiscoveryAction, &QAction::triggered, this, &ServerTreeElement::onDisableDiscovery);
        menu->insertAction(firstAction, disableDiscoveryAction);
    }
    else
    {
        QAction* enableDiscoveryAction = new QAction("Enable Discovery", menu);
        connect(enableDiscoveryAction, &QAction::triggered, this, &ServerTreeElement::onEnableDiscovery);
        menu->insertAction(firstAction, enableDiscoveryAction);
    }

    if (!parentElement)
        return menu;

    auto parentComponentElement = dynamic_cast<ServersFolderTreeElement*>(parentElement);
    if (!parentComponentElement || !parentComponentElement->isLocalDeviceFolder())
        return menu;

    menu->insertSeparator(firstAction);

    QAction* removeAction = new QAction("Remove", menu);
    connect(removeAction, &QAction::triggered, this, &ServerTreeElement::onRemove);
    menu->insertAction(firstAction, removeAction);

    menu->insertSeparator(firstAction);

    return menu;
}

void ServerTreeElement::onEnableDiscovery()
{
    try
    {
        daqComponent.asPtr<daq::IServer>(true).enableDiscovery();
        discoveryIsEnabled = true;
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(nullptr, "Error",
            QString("Failed to enable discovery: %1").arg(e.what()));
    }
}

void ServerTreeElement::onDisableDiscovery()
{
    try
    {
        daqComponent.asPtr<daq::IServer>(true).disableDiscovery();
        discoveryIsEnabled = false;
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(nullptr, "Error",
            QString("Failed to disable discovery: %1").arg(e.what()));
    }
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
        AppContext::Instance()->daqInstance().removeServer(daqComponent);
    }
    catch (const std::exception& e)
    {
        QMessageBox::critical(nullptr, "Error",
            QString("Failed to remove server: %1").arg(e.what()));
    }
}

