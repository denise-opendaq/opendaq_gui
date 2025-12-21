#pragma once
#include "folder_tree_element.h"
#include <QSplitter>
#include <opendaq/opendaq.h>

class FunctionBlockTreeElement : public FolderTreeElement
{
    Q_OBJECT

public:
    FunctionBlockTreeElement(QTreeWidget* tree, const daq::FunctionBlockPtr& daqFunctionBlock, QObject* parent = nullptr);

    // Override context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

private Q_SLOTS:
    void onRemove();
    void onAddFunctionBlock();
};
