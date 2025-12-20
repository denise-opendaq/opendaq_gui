#pragma once
#include "folder_tree_element.h"
#include <QSplitter>
#include <opendaq/opendaq.h>

class FunctionBlockTreeElement : public FolderTreeElement
{
    Q_OBJECT

public:
    FunctionBlockTreeElement(QTreeWidget* tree, const daq::FunctionBlockPtr& daqFunctionBlock, QObject* parent = nullptr)
        : FolderTreeElement(tree, daqFunctionBlock, parent)
    {
        this->type = "FunctionBlock";
        this->iconName = "function_block";
    }

    // Override context menu
    QMenu* onCreateRightClickMenu(QWidget* parent) override
    {
        QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

        menu->addSeparator();

        // Add Remove action
        QAction* removeAction = menu->addAction("Remove");
        connect(removeAction, &QAction::triggered, this, &FunctionBlockTreeElement::onRemove);

        // Add Function Block action
        QAction* addFBAction = menu->addAction("Add Function Block");
        connect(addFBAction, &QAction::triggered, this, &FunctionBlockTreeElement::onAddFunctionBlock);

        return menu;
    }

private Q_SLOTS:
    void onRemove()
    {
        // Remove this function block from parent
        if (parentElement)
        {
            parentElement->removeChild(this);
        }
    }

    void onAddFunctionBlock()
    {
        // TODO: Implement adding a new function block
        // This would typically show a dialog to select function block type
        // and then create it via openDAQ API
        qDebug() << "Add function block requested for:" << name;

        // Example placeholder:
        // auto functionBlockType = showFunctionBlockTypeDialog();
        // if (!functionBlockType.isEmpty()) {
        //     try {
        //         auto fb = daqComponent.addFunctionBlock(functionBlockType.toStdString());
        //         auto childElement = new FunctionBlockTreeElement(tree, fb, this);
        //         addChild(childElement);
        //     } catch (const std::exception& e) {
        //         QMessageBox::warning(nullptr, "Error",
        //             QString("Failed to add function block: %1").arg(e.what()));
        //     }
        // }
    }
};
