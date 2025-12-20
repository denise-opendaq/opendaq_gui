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

    // Override onSelected to display function block with horizontal split
    void onSelected(QWidget* mainContent) override
    {
        // Clear previous content
        QLayout* layout = mainContent->layout();
        if (layout)
        {
            QLayoutItem* item;
            while ((item = layout->takeAt(0)) != nullptr)
            {
                delete item->widget();
                delete item;
            }
        }
        else
        {
            layout = new QVBoxLayout(mainContent);
            layout->setContentsMargins(0, 0, 0, 0);
            mainContent->setLayout(layout);
        }

        // Create horizontal splitter
        QSplitter* splitter = new QSplitter(Qt::Horizontal, mainContent);

        // Left panel - Properties (use parent's onSelected)
        QWidget* leftPanel = new QWidget(splitter);
        leftPanel->setMinimumWidth(200);
        FolderTreeElement::onSelected(leftPanel);

        // Right panel - Empty for now (can be used for function block specific content)
        QWidget* rightPanel = new QWidget(splitter);
        rightPanel->setMinimumWidth(200);

        // Add placeholder label to right panel
        QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
        QLabel* placeholderLabel = new QLabel("Function Block Output Area");
        placeholderLabel->setAlignment(Qt::AlignCenter);
        placeholderLabel->setStyleSheet("color: #888;");
        rightLayout->addWidget(placeholderLabel);
        rightLayout->addStretch();

        // Set splitter proportions (left panel slightly larger)
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);

        layout->addWidget(splitter);
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
