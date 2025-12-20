#pragma once
#include <QtWidgets>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFrame>
#include <QMenu>
#include <QString>
#include <QMap>
#include <QIcon>
#include <memory>
#include "icon_provider.h"
#include <AppContext.h>


// Forward declaration
class BaseTreeElement;

class BaseTreeElement : public QObject
{
    Q_OBJECT

public:
    BaseTreeElement(QTreeWidget* tree, QObject* parent = nullptr)
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

    virtual ~BaseTreeElement()
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

    // Initialize the tree element with optional parent
    virtual void init(BaseTreeElement* parent = nullptr)
    {
        parentElement = parent;
        QTreeWidgetItem* parentTreeItem = parentElement ? parentElement->treeItem : nullptr;

        // Create tree item
        if (!iconName.isEmpty())
        {
            // QIcon icon = AppContext::instance()->icon(iconName);
            treeItem = new QTreeWidgetItem(parentTreeItem);
            treeItem->setText(0, name);
            // treeItem->setIcon(0, icon);
        }
        else
        {
            treeItem = new QTreeWidgetItem(parentTreeItem);
            treeItem->setText(0, name);
        }

        // Store pointer to this element in tree item
        treeItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(this)));

        // Expand by default
        treeItem->setExpanded(true);

        // Add to tree if no parent
        if (!parentTreeItem && tree)
        {
            tree->addTopLevelItem(treeItem);
        }
    }

    // Property: visible (can be overridden)
    virtual bool visible() const
    {
        return true;
    }

    // Set name and update tree item
    void setName(const QString& newName)
    {
        name = newName;
        if (treeItem)
        {
            treeItem->setText(0, name);
        }
    }

    // Show/hide based on filter (placeholder for future filtering logic)
    virtual void showFiltered(QTreeWidgetItem* parentTreeItem = nullptr)
    {
        bool isVisible = visible();

        // Show or hide this element
        if (isVisible)
        {
            if (treeItem)
            {
                treeItem->setHidden(false);
            }
        }
        else
        {
            if (treeItem)
            {
                treeItem->setHidden(true);
            }
        }

        // Recursively apply to children
        QTreeWidgetItem* childParent = isVisible ? treeItem : parentTreeItem;
        for (auto* child : children.values())
        {
            child->showFiltered(childParent);
        }
    }

    // Add child element
    BaseTreeElement* addChild(BaseTreeElement* child)
    {
        children[child->localId] = child;
        child->init(this);
        return child;
    }

    // Remove child element
    void removeChild(BaseTreeElement* child)
    {
        if (children.contains(child->localId))
        {
            children.remove(child->localId);
            delete child;
        }
    }

    // Get child by path (similar to Python's get_child)
    BaseTreeElement* getChild(const QString& path)
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

    // Called when this element is selected in the tree
    virtual void onSelected(QWidget* mainContent)
    {
    }

    // Create right-click context menu
    virtual QMenu* onCreateRightClickMenu(QWidget* parent)
    {
        QMenu* menu = new QMenu(parent);
        // Override in derived classes to add menu items
        return menu;
    }

    // Getters
    QString getLocalId() const { return localId; }
    QString getGlobalId() const { return globalId; }
    QString getName() const { return name; }
    QString getType() const { return type; }
    QTreeWidgetItem* getTreeItem() const { return treeItem; }
    BaseTreeElement* getParent() const { return parentElement; }
    QMap<QString, BaseTreeElement*> getChildren() const { return children; }

protected:
    QTreeWidget* tree;
    QTreeWidgetItem* treeItem;
    BaseTreeElement* parentElement;
    QMap<QString, BaseTreeElement*> children;

    QString localId;
    QString globalId;
    QString name;
    QString type;
    QString iconName;
};
