#include "component/base_tree_element.h"
#include "context/icon_provider.h"
#include "DetachableTabWidget.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

BaseTreeElement::BaseTreeElement(QTreeWidget* tree, QObject* parent)
    : QObject(parent)
    , tree(tree)
    , treeItem(nullptr)
    , parentElement(nullptr)
    , localId("/")
    , globalId("/")
    , name(localId)
    , type("PropertyObject")
    , iconName("")
{
}

BaseTreeElement::~BaseTreeElement()
{
    // Clean up children
    qDeleteAll(children);
    children.clear();

    // Remove tree item
    if (treeItem && tree)
    {
        delete treeItem;
        treeItem = nullptr;
    }
}

void BaseTreeElement::init(BaseTreeElement* parent)
{
    parentElement = parent;
    QTreeWidgetItem* parentTreeItem = parentElement ? parentElement->treeItem : nullptr;

    // Create tree item
    treeItem = new QTreeWidgetItem(parentTreeItem);
    treeItem->setText(0, name);

    // Store pointer to this element in tree item
    treeItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(this)));

    // Expand by default
    treeItem->setExpanded(true);

    // Add to tree if no parent
    if (!parentTreeItem && tree)
    {
        tree->addTopLevelItem(treeItem);
    }

    // Set icon after item is added to tree (ensures iconName is set by derived class constructor)
    updateIcon();
}

bool BaseTreeElement::visible() const
{
    return true;
}

void BaseTreeElement::setName(const QString& newName)
{
    name = newName;
    if (treeItem)
    {
        treeItem->setText(0, name);
    }
}

void BaseTreeElement::updateIcon()
{
    if (treeItem && !iconName.isEmpty())
    {
        QIcon icon = IconProvider::instance().icon(iconName);
        // Check if icon has available sizes (more reliable than isNull())
        if (icon.availableSizes().size() > 0)
        {
            treeItem->setIcon(0, icon);
        }
        else
        {
            qWarning() << "Failed to load icon:" << iconName << "for element:" << name;
        }
    }
}

void BaseTreeElement::showFiltered(QTreeWidgetItem* parentTreeItem)
{
    bool isVisible = visible();

    // Show or hide this element
    if (treeItem)
    {
        treeItem->setHidden(!isVisible);
        if (isVisible && parentTreeItem)
        {
            // Move to correct parent if needed
            QTreeWidgetItem* currentParent = treeItem->parent();
            if (currentParent != parentTreeItem)
            {
                // Remove from current parent
                if (currentParent)
                {
                    int index = currentParent->indexOfChild(treeItem);
                    if (index >= 0)
                        currentParent->takeChild(index);
                }
                else if (tree)
                {
                    // It's a top-level item
                    int index = tree->indexOfTopLevelItem(treeItem);
                    if (index >= 0)
                        tree->takeTopLevelItem(index);
                }
                // Add to new parent
                parentTreeItem->addChild(treeItem);
            }
            // Expand when visible
            treeItem->setExpanded(true);
        }
    }

    // Recursively apply to children
    QTreeWidgetItem* childParent = isVisible ? treeItem : parentTreeItem;
    for (auto* child : children.values())
        child->showFiltered(childParent);
}

BaseTreeElement* BaseTreeElement::addChild(BaseTreeElement* child)
{
    children[child->localId] = child;
    child->init(this);
    return child;
}

void BaseTreeElement::closeTabs()
{
    // Find all DetachableTabWidget instances and replace content of tabs associated with this component
    QWidgetList widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets)
    {
        DetachableTabWidget* tabWidget = qobject_cast<DetachableTabWidget*>(widget);
        if (!tabWidget)
            continue;

        // Check all tabs and replace content for those that belong to this component
        // Iterate backwards to avoid index issues when removing tabs
        for (int i = tabWidget->count() - 1; i >= 0; --i)
        {
            QWidget* tabPage = tabWidget->widget(i);
            if (tabPage)
            {
                QVariant tabGlobalId = tabPage->property("componentGlobalId");
                if (tabGlobalId.isValid() && tabGlobalId.toString() == globalId)
                {
                    // Save tab title before removing
                    QString tabTitle = tabWidget->tabText(i);
                    
                    // Replace tab content with "Component removed" message
                    QWidget* removedWidget = new QWidget();
                    QVBoxLayout* layout = new QVBoxLayout(removedWidget);
                    layout->setContentsMargins(20, 20, 20, 20);
                    
                    QLabel* label = new QLabel(QString("Component '%1' has been removed").arg(name), removedWidget);
                    label->setAlignment(Qt::AlignCenter);
                    label->setStyleSheet("QLabel { font-size: 14pt; color: #666; }");
                    
                    layout->addWidget(label);
                    layout->addStretch();
                    
                    // Remove old widget and insert new one at the same index
                    tabWidget->removeTab(i);
                    tabWidget->insertTab(i, removedWidget, tabTitle);
                    tabWidget->setCurrentIndex(i);
                }
            }
        }
    }
}

void BaseTreeElement::removeChild(BaseTreeElement* child)
{
    if (children.contains(child->localId))
    {
        // Close all tabs associated with this component before deleting
        child->closeTabs();
        
        children.remove(child->localId);
        delete child;
    }
}

BaseTreeElement* BaseTreeElement::getChild(const QString& path)
{
    if (path.isEmpty())
    {
        return this;
    }

    QString localPath = path;

    if (localPath.startsWith("/"))
    {
        if (localPath.startsWith("/" + localId))
        {
            if (localPath.length() == localId.length() + 1)
            {
                return this;
            }
            localPath = localPath.mid(localId.length() + 2);
        }
        else
        {
            qWarning() << "No child found at path:" << path;
            return nullptr;
        }
    }

    QStringList parts = localPath.split("/", Qt::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return this;
    }

    QString firstPart = parts[0];
    if (!children.contains(firstPart))
    {
        qWarning() << "No child found with id:" << firstPart;
        return nullptr;
    }

    if (parts.size() == 1)
    {
        return children[firstPart];
    }
    else
    {
        QString remainingPath = parts.mid(1).join("/");
        return children[firstPart]->getChild(remainingPath);
    }
}

void BaseTreeElement::onSelected(QWidget* mainContent)
{
    Q_UNUSED(mainContent);
}

QStringList BaseTreeElement::getAvailableTabNames() const
{
    return QStringList(); // Base class returns empty list
}

void BaseTreeElement::openTab(const QString& tabName, QWidget* mainContent)
{
    Q_UNUSED(tabName);
    Q_UNUSED(mainContent);
    // Base class does nothing
}

QMenu* BaseTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = new QMenu(parent);
    // Override in derived classes to add menu items
    return menu;
}

