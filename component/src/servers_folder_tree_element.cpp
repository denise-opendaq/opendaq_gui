#include "component/servers_folder_tree_element.h"
#include "dialogs/add_server_dialog.h"
#include "context/AppContext.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>

ServersFolderTreeElement::ServersFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent)
    : FolderTreeElement(tree, daqFolder, parent)
{
    this->type = "ServersFolder";
    this->iconName = "folder";
}

bool ServersFolderTreeElement::isLocalDeviceFolder() const
{
    if (!parentElement)
        return false;

    auto parentComponentElement = dynamic_cast<ComponentTreeElement*>(parentElement);
    if (!parentComponentElement)
        return false;

    if (parentComponentElement->getDaqComponent() != AppContext::instance()->daqInstance().getRootDevice())
        return false;

    return true;
}

bool ServersFolderTreeElement::visible() const
{
    if (isLocalDeviceFolder())
        return true;

    if (children.empty())
        return false;

    return ComponentTreeElement::visible();
}

QMenu* ServersFolderTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

    if (!isLocalDeviceFolder())
        return menu;

    menu->addSeparator();

    QAction* addServerAction = menu->addAction("Add Server");
    connect(addServerAction, &QAction::triggered, this, &ServersFolderTreeElement::onAddServer);
    return menu;
}

void ServersFolderTreeElement::onAddServer()
{
    // Get the instance from AppContext
    auto instance = AppContext::instance()->daqInstance();
    if (!instance.assigned())
    {
        QMessageBox::critical(nullptr, "Error", "No openDAQ instance available.");
        return;
    }

    AddServerDialog dialog(nullptr);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString serverType = dialog.getServerType();
        if (serverType.isEmpty())
            return;

        try
        {
            // Get config if available (will be nullptr if not set)
            daq::PropertyObjectPtr config = dialog.getConfig();
            
            // Add server using instance interface
            daq::ServerPtr newServer = instance.addServer(serverType.toStdString(), config);
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(nullptr, "Error",
                QString("Failed to add server '%1': %2").arg(serverType, e.what()));
        }
    }
}

