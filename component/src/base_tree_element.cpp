#include "component/base_tree_element.h"
#include "context/icon_provider.h"
#include "context/AppContext.h"
#include "DetachableTabWidget.h"
#include "DetachedWindow.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

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
        // Check if icon has available sizes (more reliable than isNull())
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
    // Find all DetachableTabWidget instances and replace content of tabs associated with this component
    QWidgetList widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets)
    {
        if (auto tabWidget = qobject_cast<DetachableTabWidget*>(widget); tabWidget)
        {
            for (int i = tabWidget->count() - 1; i >= 0; --i)
            {
                QWidget* tabPage = tabWidget->widget(i);
                if (tabPage)
                {
                    QVariant tabGlobalId = tabPage->property("componentGlobalId");
                    if (tabGlobalId.isValid() && tabGlobalId.toString() == globalId)
                        tabWidget->removeTab(i);
                }
            }
        }
        else if (auto detachedWindow = qobject_cast<DetachedWindow*>(widget); detachedWindow)
        {
            QWidget* contentWidget = detachedWindow->contentWidget();
            if (contentWidget)
            {
                QVariant contentGlobalId = contentWidget->property("componentGlobalId");
                if (contentGlobalId.isValid() && contentGlobalId.toString() == globalId)
                {
                    // Replace content with "Component removed" message
                    QWidget* removedWidget = new QWidget(detachedWindow);
                    QVBoxLayout* layout = new QVBoxLayout(removedWidget);
                    layout->setContentsMargins(20, 20, 20, 20);

                    QLabel* label = new QLabel(QString("Component '%1' has been removed").arg(name), removedWidget);
                    label->setAlignment(Qt::AlignCenter);
                    label->setStyleSheet("QLabel { font-size: 14pt; color: #666; }");

                    layout->addWidget(label);
                    layout->addStretch();

                    // Replace central widget
                    detachedWindow->setCentralWidget(removedWidget);
                    contentWidget->deleteLater();
                }
            }
        }
    }
}

void BaseTreeElement::removeChild(BaseTreeElement* child)
{
    if (child && children.find(child->localId) != children.end())
    {
        // Close all tabs associated with this component before deleting
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

void BaseTreeElement::onSelected(QWidget* mainContent)
{
    QStringList availableTabs = getAvailableTabNames();
    for (const QString& tabName : availableTabs)
        openTab(tabName, mainContent);
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

void BaseTreeElement::addTab(DetachableTabWidget* tabWidget, QWidget* tab, const QString& tabName)
{
    int index = tabWidget->addTab(tab, name + " - " + tabName);
    tabWidget->setCurrentIndex(index);
    tab->setProperty("componentGlobalId", globalId);
}

QMenu* BaseTreeElement::onCreateRightClickMenu(QWidget* parent)
{
    QMenu* menu = new QMenu(parent);
    // Override in derived classes to add menu items
    return menu;
}

QMap<QString, BaseTreeElement*> BaseTreeElement::getChildren() const
{
    QMap<QString, BaseTreeElement*> result;
    for (const auto& [k, v]: children)
        result[k] = v.get();

    return result;
}

