#include "widgets/property_object_view.h"
#include "property/property_factory.h"
#include "context/AppContext.h"
#include <QHeaderView>
#include <QSignalBlocker>
#include <QTreeWidgetItemIterator>
#include <QMessageBox>

// ============================================================================
// PropertyObjectView implementation
// ============================================================================

PropertyObjectView::PropertyObjectView(const daq::PropertyObjectPtr& root, 
                                       QWidget* parent,
                                       const daq::ComponentPtr& owner)
    : QTreeWidget(parent)
    , owner(owner)
    , root(root)
{
    if (const auto internal = root.asPtr<daq::IPropertyObjectInternal>(true); internal.assigned())
    {
        if (const auto path = internal.getPath(); path.assigned())
            rootPath = path.toStdString();
    }

    setColumnCount(2);
    setHeaderLabels({ "Property name", "Value" });
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(QHeaderView::Stretch);
    setColumnWidth(0, width() / 2);
    setColumnWidth(1, width() / 2);

    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setExpandsOnDoubleClick(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, &QTreeWidget::itemChanged, this, &PropertyObjectView::onItemChanged);
    connect(this, &QTreeWidget::itemExpanded, this, &PropertyObjectView::onItemExpanded);
    connect(this, &QTreeWidget::itemCollapsed, this, &PropertyObjectView::onItemCollapsed);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &PropertyObjectView::onItemDoubleClicked);
    connect(this, &QWidget::customContextMenuRequested, this, &PropertyObjectView::onContextMenu);

    refresh();
    if (owner.assigned())
        owner.getOnComponentCoreEvent() += daq::event(this, &PropertyObjectView::componentCoreEventCallback);

    // Connect to AppContext to refresh when showInvisible changes
    connect(AppContext::instance(), &AppContext::showInvisibleChanged, this, &PropertyObjectView::refresh);
}

PropertyObjectView::~PropertyObjectView()
{
    if (owner.assigned())
        owner.getOnComponentCoreEvent() -= daq::event(this, &PropertyObjectView::componentCoreEventCallback);
}

void PropertyObjectView::refresh()
{
    QSignalBlocker b(this);

    // Register root with nullptr to handle top-level properties
    propertyObjectToLogic[root] = nullptr;

    PropertySubtreeBuilder builder(*this);
    builder.buildFromPropertyObject(nullptr, root);
}

bool PropertyObjectView::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
    auto* item = itemFromIndex(index);
    if (!item)
        return false;

    auto* logic = getLogic(item);
    if (!logic)
    {
        // For items without logic (like list/dict children), check parent
        auto* parent = item->parent();
        if (!parent)
            return false;

        logic = getLogic(parent);
        if (!logic)
            return false;
    }

    // Check editability based on column
    if (index.column() == 0)
        return logic->isKeyEditable() && QTreeWidget::edit(index, trigger, event);
    else if (index.column() == 1)
        return logic->isValueEditable() && QTreeWidget::edit(index, trigger, event);

    return false;
}

daq::PropertyObjectPtr PropertyObjectView::getChildObject(std::string path)
{
    if (!rootPath.empty())
    {
        if (path.find(rootPath) != 0)
            return nullptr;
        if (path.length() == rootPath.length())
            path = "";
        else
            path = path.substr(rootPath.length() + 1);
    }

    if (!path.empty())
        return root;

    if (root.hasProperty(path))
        return root.getPropertyValue(path);

    return nullptr;
}

void PropertyObjectView::componentCoreEventCallback(daq::ComponentPtr& component, daq::CoreEventArgsPtr& eventArgs)
{
    if (component != owner)
        return;

    const auto eventId = static_cast<daq::CoreEventId>(eventArgs.getEventId());
    if (eventId == daq::CoreEventId::PropertyValueChanged || eventId == daq::CoreEventId::PropertyAdded)
    {
        const auto obj = getChildObject(eventArgs.getParameters()["Path"]);
        if (obj.assigned())
            onPropertyValueChanged(obj, true);
    }
    else if (eventId == daq::CoreEventId::PropertyRemoved)
    {
        const auto obj = getChildObject(eventArgs.getParameters()["Path"]);
        if (!obj.assigned())
            return;

        // Find the parent ObjectPropertyItem
        auto parentIt = propertyObjectToLogic.find(obj);
        if (parentIt == propertyObjectToLogic.end())
            return;

        const daq::StringPtr propName = eventArgs.getParameters().get("Name");
        ObjectPropertyItem* objLogic = parentIt->second;
        removeChildProperty(objLogic ? objLogic->getWidgetItem() : nullptr, propName);
    }
}

void PropertyObjectView::onPropertyValueChanged(const daq::PropertyObjectPtr& obj, bool force)
{
    // Skip if owner is assigned and force is false (event will trigger update automatically)
    if (!force && owner.assigned())
        return;

    // Find the ObjectPropertyItem for this property object using the map
    // obj is the property object that contains the changed property
    auto it = propertyObjectToLogic.find(obj);
    if (it == propertyObjectToLogic.end())
        return;

    ObjectPropertyItem* objLogic = it->second;
    QSignalBlocker blocker(this);

    if (objLogic)
    {
        PropertySubtreeBuilder builder(*this);
        objLogic->refresh(builder);
    }
    else
    {
        refresh();
    }
}

BasePropertyItem* PropertyObjectView::store(std::unique_ptr<BasePropertyItem> item)
{
    BasePropertyItem* itemPtr = item.get();
    items.insert_or_assign(itemPtr->getProperty(), std::move(item));
    return itemPtr;
}

BasePropertyItem* PropertyObjectView::getLogic(QTreeWidgetItem* it)
{
    return reinterpret_cast<BasePropertyItem*>(it->data(0, Qt::UserRole).toULongLong());
}

void PropertyObjectView::setLogic(QTreeWidgetItem* it, BasePropertyItem* logic)
{
    it->setData(0, Qt::UserRole,
                QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(logic)));
}

void PropertyObjectView::onItemExpanded(QTreeWidgetItem* item)
{
    auto* logic = getLogic(item);
    if (!logic || !logic->hasSubtree())
        return;

    PropertySubtreeBuilder builder(*this);
    logic->build_subtree(builder, item);
    logic->setExpanded(true);
}

void PropertyObjectView::onItemCollapsed(QTreeWidgetItem* item)
{
    auto* logic = getLogic(item);
    if (logic)
        logic->setExpanded(false);
}

void PropertyObjectView::onItemChanged(QTreeWidgetItem* item, int column)
{
    // Find the logic - could be on the item itself or on its parent (for dict children)
    BasePropertyItem* logic = getLogic(item);
    if (!logic && item->parent())
        logic = getLogic(item->parent());

    if (!logic)
        return;

    // Check editability
    if ((column == 0 && !logic->isKeyEditable()) || (column == 1 && !logic->isValueEditable()))
        return;

    try
    {
        logic->commitEdit(item, column);
    }
    catch (const std::exception& e)
    {
        handleEditError(item, column, logic, e.what());
    }
    catch (...)
    {
        handleEditError(item, column, logic, "unknown error");
    }
}

void PropertyObjectView::handleEditError(QTreeWidgetItem* item, int column, BasePropertyItem* logic, const char* errorMsg)
{
    QSignalBlocker b(this);
    if (column == 1 && getLogic(item) == logic)
        item->setText(1, logic->showValue());

    QMessageBox::warning(this, "Property Update Error",
                       QString("Failed to update property: %1").arg(errorMsg));
}

void PropertyObjectView::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    auto* logic = getLogic(item);
    if (!logic)
    {
        // For items without logic (like list/dict/struct children), check parent
        auto* parent = item->parent();
        if (parent)
            logic = getLogic(parent);
    }

    if (logic && !logic->isReadOnly())
        logic->handle_double_click(this, item);
}

void PropertyObjectView::onContextMenu(const QPoint& pos)
{
    auto* item = itemAt(pos);
    if (!item)
        return;

    auto* logic = getLogic(item);
    if (!logic)
    {
        // For items without logic (like list/dict children), check parent
        auto* parent = item->parent();
        if (parent)
            logic = getLogic(parent);
    }

    if (logic)
        logic->handle_right_click(this, item, viewport()->mapToGlobal(pos));
}

void PropertyObjectView::removeChildProperty(QTreeWidgetItem* parentWidget, const std::string& propName)
{
    int childCount = parentWidget ? parentWidget->childCount() : topLevelItemCount();

    for (int i = 0; i < childCount; ++i)
    {
        QTreeWidgetItem* childWidget = parentWidget ? parentWidget->child(i) : topLevelItem(i);
        auto* childLogic = getLogic(childWidget);

        if (childLogic && childLogic->getName() == propName)
        {
            // Clear widget (will remove from tree and delete)
            childLogic->setWidgetItem(nullptr);

            // Remove from items map
            items.erase(childLogic->getProperty());
            break;
        }
    }
}

// ============================================================================
// PropertySubtreeBuilder implementation
// ============================================================================

QTreeWidgetItem* PropertySubtreeBuilder::addItem(QTreeWidgetItem* parent,
                                                 std::unique_ptr<BasePropertyItem> item)
{
    auto* logic = view.store(std::move(item));

    if (!logic->isVisible() && !AppContext::instance()->showInvisibleComponents())
    {
        logic->setWidgetItem(nullptr);
        return nullptr;
    }

    // Create new widget and set it (takes ownership via unique_ptr)
    QTreeWidgetItem* it = logic->getWidgetItem();
    if (!it)
    {
        it = new QTreeWidgetItem();
        logic->setWidgetItem(it);

        if (parent)
            parent->addChild(it);
        else
            view.addTopLevelItem(it);
        PropertyObjectView::setLogic(it, logic);
    }

    if (logic->isReadOnly())
    {
        QPalette palette = view.palette();
        it->setForeground(0, palette.color(QPalette::Disabled, QPalette::Text));
        it->setForeground(1, palette.color(QPalette::Disabled, QPalette::Text));
    }
    else if (!logic->hasSubtree())
    {
        it->setFlags(it->flags() |= Qt::ItemIsEditable);
    }

    if (logic->hasSubtree())
        it->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

    PropertySubtreeBuilder builder(*this);
    logic->refresh(builder);
    if (logic->hasSubtree() && logic->isExpanded())
        view.onItemExpanded(it);

    return it;
}

void PropertySubtreeBuilder::buildFromPropertyObject(QTreeWidgetItem* parent, const daq::PropertyObjectPtr& obj)
{
    if (!obj.assigned())
        return;

    for (const auto& prop: obj.getAllProperties())
    {
        if (auto it = view.items.find(prop); it != view.items.end())
            addItem(parent, std::move(it->second));
        else
            addItem(parent, createPropertyItem(obj, prop));
    }
}
