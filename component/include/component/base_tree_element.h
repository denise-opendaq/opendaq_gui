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

class BaseTreeElement;

class BaseTreeElement : public QObject
{
    Q_OBJECT

public:
    BaseTreeElement(QTreeWidget* tree, QWidget* contentHost, QObject* parent = nullptr);
    virtual ~BaseTreeElement();

    virtual void init(BaseTreeElement* parent = nullptr);

    virtual bool visible() const;

    void setName(const QString& newName);

    void updateIcon();

    virtual void showFiltered(QTreeWidgetItem* parentTreeItem = nullptr);

    BaseTreeElement* addChild(std::unique_ptr<BaseTreeElement> child);

    void removeChild(BaseTreeElement* child);

    // Remove the content widget associated with this element (if currently shown).
    void clearContent();

    BaseTreeElement* getChild(const QString& path);

    // Called when this element is selected in the tree
    virtual void onSelected();

    // Build and add the element's content widget into the shared content host.
    // Default implementation does nothing — subclasses override to populate.
    virtual void openTab();

    // Replace the content of the shared host with `widget`. Sets the
    // "componentGlobalId" property on the widget so it can be matched later
    // (e.g. for cleanup when the component is removed).
    void setContent(QWidget* widget);

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
    QWidget* getContentHost() const { return contentHost; }

protected:
    QTreeWidget* tree;
    QTreeWidgetItem* treeItem;
    BaseTreeElement* parentElement;
    std::map<QString, std::unique_ptr<BaseTreeElement>> children;
    QPointer<QWidget> contentHost;

    QString localId;
    QString globalId;
    QString name;
    QString type;
    QString iconName;
};
