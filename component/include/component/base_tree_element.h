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

class QTabWidget;

class BaseTreeElement;

class BaseTreeElement : public QObject
{
    Q_OBJECT

public:
    BaseTreeElement(QTreeWidget* tree, QTabWidget* contentTabs, QObject* parent = nullptr);
    virtual ~BaseTreeElement();

    virtual void init(BaseTreeElement* parent = nullptr);

    virtual bool visible() const;

    void setName(const QString& newName);

    void updateIcon();

    virtual void showFiltered(QTreeWidgetItem* parentTreeItem = nullptr);

    BaseTreeElement* addChild(std::unique_ptr<BaseTreeElement> child);

    void removeChild(BaseTreeElement* child);

    // Close all tabs associated with this component
    void closeTabs();

    BaseTreeElement* getChild(const QString& path);

    // Called when this element is selected in the tree
    virtual void onSelected();

    // Get list of available tab names for this component
    virtual QStringList getAvailableTabNames() const;

    // Open a specific tab by name (subclasses construct the widget and call addTab)
    virtual void openTab(const QString& tabName);

    // Add a tab into the shared content tab widget. Sets the
    // "componentGlobalId" property on the widget so it can be matched later
    // (e.g. for cleanup when the component is removed).
    void addTab(QWidget* tab, const QString& tabName);

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
    QTabWidget* getContentTabs() const { return contentTabs; }

protected:
    QTreeWidget* tree;
    QTreeWidgetItem* treeItem;
    BaseTreeElement* parentElement;
    std::map<QString, std::unique_ptr<BaseTreeElement>> children;
    QPointer<QTabWidget> contentTabs;

    QString localId;
    QString globalId;
    QString name;
    QString type;
    QString iconName;
};
