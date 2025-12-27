#include "component/function_block_tree_element.h"
#include "dialogs/add_function_block_dialog.h"
#include "widgets/function_block_widget.h"
#include "component/component_tree_widget.h"
#include "DetachableTabWidget.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>

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
    auto parentDeviceElement = dynamic_cast<ComponentTreeElement*>(parentElement->getParent());
    if (!parentDeviceElement)
        return;

    auto parentComponent = parentDeviceElement->getDaqComponent();
    if (const auto parentFb = parentComponent.asPtrOrNull<daq::IFunctionBlock>(); parentFb.assigned())
        parentFb.removeFunctionBlock(daqComponent);
    else if (const auto parentDevice = parentComponent.asPtrOrNull<daq::IDevice>(); parentDevice.assigned())
        parentDevice.removeFunctionBlock(daqComponent);
    else
        return;
}

void FunctionBlockTreeElement::onAddFunctionBlock()
{
    auto functionBlock = daqComponent.asPtr<daq::IFunctionBlock>();

    AddFunctionBlockDialog dialog(functionBlock, nullptr);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString functionBlockType = dialog.getFunctionBlockType();
        if (functionBlockType.isEmpty())
        {
            return;
        }

        try
        {
            // Get config if available (will be nullptr if not set)
            daq::PropertyObjectPtr config = dialog.getConfig();
            
            // Add function block using IFunctionBlock interface
            daq::FunctionBlockPtr newFunctionBlock = functionBlock.addFunctionBlock(functionBlockType.toStdString(), config);
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(nullptr, "Error",
                QString("Failed to add function block '%1': %2").arg(functionBlockType, e.what()));
        }
    }
}

QStringList FunctionBlockTreeElement::getAvailableTabNames() const
{
    QStringList tabs = Super::getAvailableTabNames();
    tabs << "Input Ports";
    return tabs;
}

void FunctionBlockTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    if (tabName == "Input Ports") 
    {
        auto tabWidget = dynamic_cast<DetachableTabWidget*>(mainContent);
        if (tabWidget) 
        {
            auto functionBlock = getFunctionBlock();
            auto componentTree = qobject_cast<ComponentTreeWidget*>(tree);
            auto functionBlockWidget = new FunctionBlockWidget(functionBlock, componentTree);
            addTab(tabWidget, functionBlockWidget, tabName);
        }
    } 
    else 
    {
        Super::openTab(tabName, mainContent);
    }
}

daq::FunctionBlockPtr FunctionBlockTreeElement::getFunctionBlock() const
{
    return daqComponent.asPtr<daq::IFunctionBlock>();
}

