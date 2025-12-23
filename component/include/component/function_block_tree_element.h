#pragma once
#include "folder_tree_element.h"
#include <QSplitter>
#include <opendaq/opendaq.h>

class FunctionBlockTreeElement : public FolderTreeElement
{
    Q_OBJECT

    using Super = FolderTreeElement;

public:
    FunctionBlockTreeElement(QTreeWidget* tree, const daq::FunctionBlockPtr& daqFunctionBlock, QObject* parent = nullptr);

    // Override context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override;

    // Override tab methods to add Input Ports tab
    QStringList getAvailableTabNames() const override;
    void openTab(const QString& tabName, QWidget* mainContent) override;

    // Get the function block
    daq::FunctionBlockPtr getFunctionBlock() const;

public Q_SLOTS:
    void onAddFunctionBlock();

private Q_SLOTS:
    void onRemove();
};
