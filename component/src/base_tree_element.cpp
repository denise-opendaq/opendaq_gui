#include "component/base_tree_element.h"
#include "context/icon_provider.h"
#include "context/AppContext.h"
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

BaseTreeElement::BaseTreeElement(QTreeWidget* tree, QTabWidget* contentTabs, QObject* parent)
    : QObject(parent)
    , tree(tree)
    , treeItem(nullptr)
    , parentElement(nullptr)
    , contentTabs(contentTabs)
    , localId("/")
    , globalId("/")
    , name(localId)
    , type("PropertyObject")
    , iconName("")
{
    if (!contentTabs)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("BaseTreeElement: contentTabs is null in constructor for '{}'", name.toStdString());
    }
}

BaseTreeElement::~BaseTreeElement()
{
    children.clear();

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

    treeItem = new QTreeWidgetItem(parentTreeItem);
    treeItem->setText(0, name);

    treeItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(this)));

    treeItem->setExpanded(true);

    if (!parentTreeItem && tree)
        tree->addTopLevelItem(treeItem);

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
        treeItem->setText(0, name);
}

void BaseTreeElement::updateIcon()
{
    if (treeItem && !iconName.isEmpty())
    {
        QIcon icon = IconProvider::instance().icon(iconName);
        if (icon.availableSizes().size() > 0)
        {
            treeItem->setIcon(0, icon);
        }
        else
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("Failed to load icon: {} for element: {}", iconName.toStdString(), name.toStdString());
        }
    }
}

void BaseTreeElement::showFiltered(QTreeWidgetItem* parentTreeItem)
{
    bool isVisible = visible();

    if (treeItem)
    {
        treeItem->setHidden(!isVisible);
        if (isVisible && parentTreeItem)
        {
            QTreeWidgetItem* currentParent = treeItem->parent();
            if (currentParent != parentTreeItem)
            {
                if (currentParent)
                {
                    int index = currentParent->indexOfChild(treeItem);
                    if (index >= 0)
                        currentParent->takeChild(index);
                }
                else if (tree)
                {
                    int index = tree->indexOfTopLevelItem(treeItem);
                    if (index >= 0)
                        tree->takeTopLevelItem(index);
                }
                parentTreeItem->addChild(treeItem);
            }
            treeItem->setExpanded(true);
        }
    }

    QTreeWidgetItem* childParent = isVisible ? treeItem : parentTreeItem;
    for (const auto& [_, child] : children)
        child->showFiltered(childParent);
}

BaseTreeElement* BaseTreeElement::addChild(std::unique_ptr<BaseTreeElement> child)
{
    BaseTreeElement* rawPtr = child.get();
    child->init(this);
    children[child->localId] = std::move(child);
    return rawPtr;
}

void BaseTreeElement::closeTabs()
{
    if (!contentTabs)
        return;

    for (int i = contentTabs->count() - 1; i >= 0; --i)
    {
        QWidget* tabPage = contentTabs->widget(i);
        if (!tabPage)
            continue;

        const QVariant tabGlobalId = tabPage->property("componentGlobalId");
        if (tabGlobalId.isValid() && tabGlobalId.toString() == globalId)
        {
            contentTabs->removeTab(i);
            tabPage->deleteLater();
        }
    }
}

void BaseTreeElement::removeChild(BaseTreeElement* child)
{
    if (child && children.find(child->localId) != children.end())
    {
        child->closeTabs();
        children.erase(child->localId);
    }
}

BaseTreeElement* BaseTreeElement::getChild(const QString& path)
{
    if (path.isEmpty())
        return this;

    QString localPath = path;

    if (localPath.startsWith("/"))
    {
        if (localPath.startsWith("/" + localId))
        {
            if (localPath.length() == localId.length() + 1)
                return this;
            localPath = localPath.mid(localId.length() + 2);
        }
        else
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_W("No child found at path: {}", path.toStdString());
            return nullptr;
        }
    }

    QStringList parts = localPath.split("/", Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return this;

    QString firstPart = parts[0];
    if (children.find(firstPart) == children.end())
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("No child found with id: {}", firstPart.toStdString());
        return nullptr;
    }

    if (parts.size() == 1)
    {
        auto it = children.find(firstPart);
        return (it != children.end()) ? it->second.get() : nullptr;
    }
    else
    {
        QString remainingPath = parts.mid(1).join("/");
        auto it = children.find(firstPart);
        return (it != children.end()) ? it->second->getChild(remainingPath) : nullptr;
    }
}

void BaseTreeElement::onSelected()
{
    const QStringList availableTabs = getAvailableTabNames();
    for (const QString& tabName : availableTabs)
        openTab(tabName);
}

QStringList BaseTreeElement::getAvailableTabNames() const
{
    return QStringList();
}

void BaseTreeElement::openTab(const QString& tabName)
{
    Q_UNUSED(tabName);
}

void BaseTreeElement::addTab(QWidget* tab, const QString& tabName)
{
    if (!tab || !contentTabs)
        return;

    const QString fullTabName = name + " - " + tabName;
    const int idx = contentTabs->addTab(tab, fullTabName);
    contentTabs->setCurrentIndex(idx);

    tab->setProperty("componentGlobalId", globalId);
}

QMenu* BaseTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = new QMenu(parent);
    return menu;
}

QMap<QString, BaseTreeElement*> BaseTreeElement::getChildren() const
{
    QMap<QString, BaseTreeElement*> result;
    for (const auto& [k, v] : children)
        result[k] = v.get();

    return result;
}
