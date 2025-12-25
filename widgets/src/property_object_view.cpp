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
    connect(this, &QTreeWidget::itemDoubleClicked, this, &PropertyObjectView::onItemDoubleClicked);
    connect(this, &QWidget::customContextMenuRequested, this, &PropertyObjectView::onContextMenu);

    refresh();
    if (owner.assigned())
        owner.getOnComponentCoreEvent() += daq::event(this, &PropertyObjectView::componentCoreEventCallback);
}

PropertyObjectView::~PropertyObjectView()
{
    if (owner.assigned())
        owner.getOnComponentCoreEvent() -= daq::event(this, &PropertyObjectView::componentCoreEventCallback);
}

void PropertyObjectView::refresh()
{
    QSignalBlocker b(this);

    clear();
    items.clear();
    propertyObjectToLogic.clear();

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

void PropertyObjectView::componentCoreEventCallback(daq::ComponentPtr& component, daq::CoreEventArgsPtr& eventArgs)
{
    if (component != owner)
        return;

    std::string path = eventArgs.getParameters()["Path"];
    
    if (auto objPath = root.asPtr<daq::IPropertyObjectInternal>(true).getPath(); objPath.assigned() && objPath.getLength())
    {
        if (path.find(objPath.toStdString()) != 0)
            return;
        if (path.length() == objPath.getLength())
            path = "";
        else
            path = path.substr(objPath.getLength() + 1);
    }

    const auto eventId = static_cast<daq::CoreEventId>(eventArgs.getEventId());
    if (eventId != daq::CoreEventId::PropertyValueChanged)
        return;

    daq::StringPtr propertyName = eventArgs.getParameters()["Name"];

    // Get the property object that owns this property
    auto obj = root;
    if (!path.empty())
    {    
        if (!obj.hasProperty(path))
            return;
        obj = obj.getPropertyValue(path);
    }

    // Find the ObjectPropertyItem using the map
    auto it = propertyObjectToLogic.find(obj);
    if (it == propertyObjectToLogic.end())
        return;

    ObjectPropertyItem* parentLogic = it->second;
    QSignalBlocker blocker(this);

    // Find the tree item for this logic
    QTreeWidgetItem* parentItem = nullptr;
    if (parentLogic)
    {
        QTreeWidgetItemIterator iter(this);
        while (*iter)
        {
            if (getLogic(*iter) == parentLogic)
            {
                parentItem = *iter;
                break;
            }
            ++iter;
        }
    }

    // Get the list of items to search
    int childCount = parentItem ? parentItem->childCount() : topLevelItemCount();

    // Find the property item
    for (int i = 0; i < childCount; ++i)
    {
        QTreeWidgetItem* child = parentItem ? parentItem->child(i) : topLevelItem(i);
        auto* logic = getLogic(child);
        if (logic && logic->getName() == propertyName)
        {
            // Update the value display
            child->setText(1, logic->showValue());

            // If it has a subtree and is expanded, reload it
            if (logic->hasSubtree() && child->isExpanded())
            {
                PropertySubtreeBuilder builder(*this);
                logic->build_subtree(builder, child);
            }
            break;
        }
    }
}

BasePropertyItem* PropertyObjectView::store(std::unique_ptr<BasePropertyItem> item)
{
    items.emplace_back(std::move(item));
    return items.back().get();
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
        Q_EMIT propertyChanged(QString::fromStdString(logic->getName()), logic->showValue());
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

    if (logic)
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

// ============================================================================
// PropertySubtreeBuilder implementation
// ============================================================================

QTreeWidgetItem* PropertySubtreeBuilder::addItem(QTreeWidgetItem* parent,
                                                  std::unique_ptr<BasePropertyItem> item)
{
    auto* logic = view.store(std::move(item));

    auto* it = new QTreeWidgetItem();

    it->setText(0, QString::fromStdString(logic->getName()));
    it->setText(1, logic->showValue());

    // Editable only if allowed by logic AND not a container (containers edit their children, not the header)
    // Column edit is controlled by PropertyObjectView::edit override.
    if (!logic->hasSubtree() && (logic->isKeyEditable() || logic->isValueEditable()))
    {
        it->setFlags(it->flags() |= Qt::ItemIsEditable);
    }
    else if (!logic->isValueEditable())
    {
        // Set inactive color only for truly non-editable items (not containers)
        QPalette palette = view.palette();
        it->setForeground(0, palette.color(QPalette::Disabled, QPalette::Text));
        it->setForeground(1, palette.color(QPalette::Disabled, QPalette::Text));
    }

    if (logic->hasSubtree())
    {
        it->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

        // Dummy child so expand arrow appears before lazy-load
        it->addChild(new QTreeWidgetItem());
    }

    PropertyObjectView::setLogic(it, logic);

    if (parent)
        parent->addChild(it);
    else
        view.addTopLevelItem(it);

    return it;
}

void PropertySubtreeBuilder::buildFromPropertyObject(QTreeWidgetItem* parent, const daq::PropertyObjectPtr& obj)
{
    if (!obj.assigned())
        return;

    for (const auto& prop: obj.getAllProperties())
        addItem(parent, createPropertyItem(obj, prop));
}
