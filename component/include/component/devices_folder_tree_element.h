#pragma once
#include "folder_tree_element.h"
#include <QSplitter>
#include <opendaq/opendaq.h>

class DevicesFolderTreeElement : public FolderTreeElement
{
    Q_OBJECT

public:
    DevicesFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QObject* parent = nullptr);

    // Override context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

private Q_SLOTS:
    void onAddDevice();
};

