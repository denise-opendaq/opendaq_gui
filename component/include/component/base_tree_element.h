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
#include "context/icon_provider.h"
#include "context/AppContext.h"


// Forward declaration
class BaseTreeElement;

class BaseTreeElement : public QObject
{
    Q_OBJECT

public:
    BaseTreeElement(QTreeWidget* tree, QObject* parent = nullptr);
    virtual ~BaseTreeElement();

    // Initialize the tree element with optional parent
    virtual void init(BaseTreeElement* parent = nullptr);

    // Property: visible (can be overridden)
    virtual bool visible() const;

    // Set name and update tree item
    void setName(const QString& newName);

    // Update icon for this element
    void updateIcon();

    // Show/hide based on filter (placeholder for future filtering logic)
    virtual void showFiltered(QTreeWidgetItem* parentTreeItem = nullptr);

    // Add child element
    BaseTreeElement* addChild(BaseTreeElement* child);

    // Remove child element
    void removeChild(BaseTreeElement* child);

    // Get child by path (similar to Python's get_child)
    BaseTreeElement* getChild(const QString& path);

    // Called when this element is selected in the tree
    virtual void onSelected(QWidget* mainContent);

    // Get list of available tab names for this component
    virtual QStringList getAvailableTabNames() const;

    // Open a specific tab by name
    virtual void openTab(const QString& tabName, QWidget* mainContent);

    // Create right-click context menu
    virtual QMenu* onCreateRightClickMenu(QWidget* parent);

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
