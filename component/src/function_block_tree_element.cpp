#include "component/function_block_tree_element.h"
#include "dialogs/add_function_block_dialog.h"
#include "widgets/function_block_widget.h"
#include "component/component_tree_widget.h"
#include <qt_widget_interface/qt_widget_interface.h>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

FunctionBlockTreeElement::FunctionBlockTreeElement(QTreeWidget* tree, const daq::FunctionBlockPtr& daqFunctionBlock, LayoutManager* layoutManager, QObject* parent)
    : FolderTreeElement(tree, daqFunctionBlock, layoutManager, parent)
{
    this->type = "FunctionBlock";
    this->iconName = "function_block";
}

QMenu* FunctionBlockTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = FolderTreeElement::onCreateRightClickMenu(parent);

    // Get the first action to insert our items before parent items
    QAction* firstAction = menu->actions().isEmpty() ? nullptr : menu->actions().first();

    QAction* removeAction = new QAction("Remove", menu);
    connect(removeAction, &QAction::triggered, this, &FunctionBlockTreeElement::onRemove);
    menu->insertAction(firstAction, removeAction);

    QAction* addFBAction = new QAction("Add Function Block", menu);
    connect(addFBAction, &QAction::triggered, this, &FunctionBlockTreeElement::onAddFunctionBlock);
    menu->insertAction(firstAction, addFBAction);

    menu->insertSeparator(firstAction);

    return menu;
}

void FunctionBlockTreeElement::onRemove()
{
    auto parentDeviceElement = dynamic_cast<ComponentTreeElement*>(parentElement->getParent());
    if (!parentDeviceElement)
        return;

    auto parentComponent = parentDeviceElement->getDaqComponent();
    if (const auto parentFb = parentComponent.asPtrOrNull<daq::IFunctionBlock>(true); parentFb.assigned())
        parentFb.removeFunctionBlock(daqComponent);
    else if (const auto parentDevice = parentComponent.asPtrOrNull<daq::IDevice>(true); parentDevice.assigned())
        parentDevice.removeFunctionBlock(daqComponent);
    else
        return;
}

void FunctionBlockTreeElement::onAddFunctionBlock()
{
    auto functionBlock = daqComponent.asPtr<daq::IFunctionBlock>(true);

    AddFunctionBlockDialog dialog(functionBlock, nullptr);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString functionBlockType = dialog.getFunctionBlockType();
        if (functionBlockType.isEmpty())
            return;

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
    tabs << "Signal Selector";    
    return tabs;
}

void FunctionBlockTreeElement::openTab(const QString& tabName)
{
    if (!layoutManager)
        return;
        
    if (tabName == "Signal Selector")
    {
        auto componentTree = qobject_cast<ComponentTreeWidget*>(tree);
        auto functionBlockWidget = new FunctionBlockWidget(daqComponent, componentTree);
        if (daqComponent.supportsInterface<IQTWidget>())
            addTab(functionBlockWidget, tabName, LayoutZone::Bottom, "Properties");
        else
            addTab(functionBlockWidget, tabName, LayoutZone::Left);
    }
    else
    {
        Super::openTab(tabName);
    }
}

daq::FunctionBlockPtr FunctionBlockTreeElement::getFunctionBlock() const
{
    return daqComponent.asPtr<daq::IFunctionBlock>();
}

