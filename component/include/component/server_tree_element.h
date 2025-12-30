#pragma once
#include "folder_tree_element.h"
#include <QSplitter>
#include <opendaq/server_ptr.h>

class ServerTreeElement : public FolderTreeElement
{
    Q_OBJECT

public:
    ServerTreeElement(QTreeWidget* tree, const daq::ServerPtr& daqServer, QObject* parent = nullptr);

    // Override context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

private Q_SLOTS:
    void onRemove();
};

