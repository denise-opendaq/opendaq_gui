#pragma once
#include <QtWidgets>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFrame>
#include <QMenu>
#include <QString>
#include <QMap>
#include <QIcon>
#include <QPointer>
#include <memory>
#include <map>

#include "LayoutManager.h"

// Forward declaration
class BaseTreeElement;

class BaseTreeElement : public QObject
{
    Q_OBJECT

public:
    BaseTreeElement(QTreeWidget* tree, LayoutManager* layoutManager, QObject* parent = nullptr);
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

    // Add child element (takes ownership)
    BaseTreeElement* addChild(std::unique_ptr<BaseTreeElement> child);

    // Remove child element (releases ownership)
    void removeChild(BaseTreeElement* child);

    // Close all tabs associated with this component
    void closeTabs();

    // Get child by path (similar to Python's get_child)
    BaseTreeElement* getChild(const QString& path);

    // Called when this element is selected in the tree
    virtual void onSelected();

    // Get list of available tab names for this component
    virtual QStringList getAvailableTabNames() const;

    // Open a specific tab by name
    virtual void openTab(const QString& tabName);

    // Helper methods to add tabs (relativeToTabName = short name of tab to place relative to, e.g. "Attributes")
    void addTab(QWidget* tab, const QString& tabName, LayoutZone zone = LayoutZone::Default,
                const QString& relativeToTabName = QString());

    // Create right-click context menu
    virtual QMenu* onCreateRightClickMenu(QWidget* parent);

    // Getters
    QString getLocalId() const { return localId; }
    QString getGlobalId() const { return globalId; }
    QString getName() const { return name; }
    QString getType() const { return type; }
    QTreeWidgetItem* getTreeItem() const { return treeItem; }
    BaseTreeElement* getParent() const { return parentElement; }
    QMap<QString, BaseTreeElement*> getChildren() const;
    LayoutManager* getLayoutManager() const { return layoutManager; }

protected:
    QTreeWidget* tree;
    QTreeWidgetItem* treeItem;
    BaseTreeElement* parentElement;
    std::map<QString, std::unique_ptr<BaseTreeElement>> children;
    QPointer<LayoutManager> layoutManager;

    QString localId;
    QString globalId;
    QString name;
    QString type;
    QString iconName;
};
