#include "component/function_block_tree_element.h"
#include <QMenu>
#include <QAction>
#include <QDebug>

FunctionBlockTreeElement::FunctionBlockTreeElement(QTreeWidget* tree, const daq::FunctionBlockPtr& daqFunctionBlock, QObject* parent)
    : FolderTreeElement(tree, daqFunctionBlock, parent)
{
    this->type = "FunctionBlock";
    this->iconName = "function_block";
}

QMenu* FunctionBlockTreeElement::onCreateRightClickMenu(QWidget* parent)
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

void FunctionBlockTreeElement::onRemove()
{
    // Remove this function block from parent
    if (parentElement)
    {
        parentElement->removeChild(this);
    }
}

void FunctionBlockTreeElement::onAddFunctionBlock()
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

