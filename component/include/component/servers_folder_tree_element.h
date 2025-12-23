#pragma once
#include "folder_tree_element.h"
#include <QSplitter>
#include <opendaq/opendaq.h>

class ServersFolderTreeElement : public FolderTreeElement
{
    Q_OBJECT

public:
    ServersFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent = nullptr);
    bool isLocalDeviceFolder() const;

    // Override context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

private Q_SLOTS:
    void onAddServer();
};

