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
    if (eventId == daq::CoreEventId::PropertyValueChanged)
    {
        daq::StringPtr propertyName = eventArgs.getParameters()["Name"];

        // Get the property object that owns this property
        auto obj = root;
        if (!path.empty())
        {
            if (obj.hasProperty(path))
                obj = obj.getPropertyValue(path);
            else
                return;
        }

        onPropertyValueChanged(obj, true);  // force=true because this is from event callback
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

    // Find the tree item that represents this property object
    QTreeWidgetItem* objItem = nullptr;
    if (objLogic)
    {
        QTreeWidgetItemIterator iter(this);
        while (*iter)
        {
            if (getLogic(*iter) == objLogic)
            {
                objItem = *iter;
                break;
            }
            ++iter;
        }
    }

    // Property changes can affect visibility of other properties in the same object
    // Rebuild all properties (children) of this object to reflect visibility changes
    PropertySubtreeBuilder builder(*this);

    if (objItem && objLogic)
    {
        // Rebuild all properties from this property object
        objLogic->build_subtree(builder, objItem, true);
    }
    else
    {
        // This is a top-level object (root), rebuild everything
        clear();
        items.clear();
        propertyObjectToLogic.clear();
        propertyObjectToLogic[root] = nullptr;
        builder.buildFromPropertyObject(nullptr, root);
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

    bool showInvisible = AppContext::instance()->showInvisibleComponents();
    const auto props = showInvisible ? obj.getAllProperties() :  obj.getVisibleProperties();
    for (const auto& prop: props)
    {
        // Skip invisible properties if showInvisible is false
        if (!showInvisible && !prop.getVisible())
            continue;

        addItem(parent, createPropertyItem(obj, prop));
    }
}
