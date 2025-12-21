#include "component/base_tree_element.h"
#include "component/icon_provider.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>

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

BaseTreeElement* BaseTreeElement::addChild(BaseTreeElement* child)
{
    children[child->localId] = child;
    child->init(this);
    return child;
}

void BaseTreeElement::removeChild(BaseTreeElement* child)
{
    if (children.contains(child->localId))
    {
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

QMenu* BaseTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = new QMenu(parent);
    // Override in derived classes to add menu items
    return menu;
}

