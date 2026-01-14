#pragma once
#include "folder_tree_element.h"
#include <QSplitter>
#include <opendaq/folder_ptr.h>

class FunctionBlocksFolderTreeElement : public FolderTreeElement
{
    Q_OBJECT

public:
    FunctionBlocksFolderTreeElement(QTreeWidget* tree, const daq::FolderPtr& daqFolder, LayoutManager* layoutManager, QObject* parent = nullptr);

    // Override context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

private Q_SLOTS:
    void onAddFunctionBlock();
};

