#pragma once
#include "folder_tree_element.h"
#include <opendaq/folder_ptr.h>

class InputPortFolderTreeElement : public FolderTreeElement
{
    Q_OBJECT

public:
    InputPortFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, QWidget* contentHost, QObject* parent = nullptr);

    void openTab() override;
};

