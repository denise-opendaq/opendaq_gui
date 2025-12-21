#pragma once

#include "property/property_factory.h"
#include "AppContext.h"

#include <memory>
#include <vector>
#include <utility>
#include <unordered_map>

// Custom hash function for PropertyObjectPtr that uses the object's address
struct PropertyObjectPtrHash
{
    std::size_t operator()(const daq::PropertyObjectPtr& ptr) const noexcept
    {
        return std::hash<void*>{}(ptr.getObject());
    }
};

// ============================================================================
// PropertySubtreeBuilder — строит дерево в существующем QTreeWidget
// ============================================================================

class PropertySubtreeBuilder
{
public:
    explicit PropertySubtreeBuilder(PropertyObjectView& view)
        : view(view)
    {}

    QTreeWidgetItem* addItem(QTreeWidgetItem* parent, std::unique_ptr<BasePropertyItem> item);
    void buildFromPropertyObject(QTreeWidgetItem* parent, const daq::PropertyObjectPtr& obj);

    PropertyObjectView& view;
};

// ============================================================================
// PropertyObjectView — UI + wiring + storage for BasePropertyItem objects
// ============================================================================

class PropertyObjectView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PropertyObjectView(const daq::PropertyObjectPtr& root, QWidget* parent = nullptr)
        : QTreeWidget(parent)
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
        auto context = AppContext::instance()->daqInstance().getContext();
        context.getOnCoreEvent() += daq::event(this, &PropertyObjectView::componentCoreEventCallback);
    }

    virtual ~PropertyObjectView()
    {
        auto context = AppContext::instance()->daqInstance().getContext();
        context.getOnCoreEvent() -= daq::event(this, &PropertyObjectView::componentCoreEventCallback);
    }

public Q_SLOTS:
    void refresh()
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

Q_SIGNALS:
    void propertyChanged(const QString& propertyName, const QString& newValue);

protected:
    // Control editing based on column and item logic
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override
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

    void componentCoreEventCallback(daq::ComponentPtr& component, daq::CoreEventArgsPtr& eventArgs)
    {
        if (component != root)
            return;

        if (eventArgs.getEventName() != "PropertyValueChanged")
            return;

        daq::StringPtr path = eventArgs.getParameters()["Path"];
        daq::StringPtr propertyName = eventArgs.getParameters()["Name"];

        // Get the property object that owns this property
        auto obj = root;
        if (path.getLength())
            obj = obj.getPropertyValue(path);

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

public:
    // Used by PropertySubtreeBuilder
    BasePropertyItem* store(std::unique_ptr<BasePropertyItem> item)
    {
        items.emplace_back(std::move(item));
        return items.back().get();
    }

    std::unordered_map<daq::PropertyObjectPtr, ObjectPropertyItem*, PropertyObjectPtrHash> propertyObjectToLogic;

private:
    friend class PropertySubtreeBuilder;

    static BasePropertyItem* getLogic(QTreeWidgetItem* it)
    {
        return reinterpret_cast<BasePropertyItem*>(it->data(0, Qt::UserRole).toULongLong());
    }

    static void setLogic(QTreeWidgetItem* it, BasePropertyItem* logic)
    {
        it->setData(0, Qt::UserRole,
                    QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(logic)));
    }

private Q_SLOTS:
    void onItemExpanded(QTreeWidgetItem* item)
    {
        auto* logic = getLogic(item);
        if (!logic || !logic->hasSubtree())
            return;

        PropertySubtreeBuilder builder(*this);
        logic->build_subtree(builder, item);
    }

    void onItemChanged(QTreeWidgetItem* item, int column)
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

    void handleEditError(QTreeWidgetItem* item, int column, BasePropertyItem* logic, const char* errorMsg)
    {
        QSignalBlocker b(this);
        if (column == 1 && getLogic(item) == logic)
            item->setText(1, logic->showValue());

        QMessageBox::warning(this, "Property Update Error",
                           QString("Failed to update property: %1").arg(errorMsg));
    }

    void onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
    {
        if (auto* logic = getLogic(item))
            logic->handle_double_click(this, item);
    }

    void onContextMenu(const QPoint& pos)
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

private:
    daq::PropertyObjectPtr root;
    std::vector<std::unique_ptr<BasePropertyItem>> items;
};

// ============================================================================
// PropertySubtreeBuilder implementation
// ============================================================================

inline QTreeWidgetItem* PropertySubtreeBuilder::addItem(QTreeWidgetItem* parent,
                                                        std::unique_ptr<BasePropertyItem> item)
{
    auto* logic = view.store(std::move(item));

    auto* it = new QTreeWidgetItem();

    it->setText(0, QString::fromStdString(logic->getName()));
    it->setText(1, logic->showValue());

    // Editable only if allowed by logic; column edit is controlled by PropertyObjectView::edit override.
    if (logic->isKeyEditable() || logic->isValueEditable())
    {
        it->setFlags(it->flags() |= Qt::ItemIsEditable);
    }
    else
    {
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

// ============================================================================
// PropertySubtreeBuilder::buildFromPropertyObject implementation
// ============================================================================

inline void PropertySubtreeBuilder::buildFromPropertyObject(QTreeWidgetItem* parent, const daq::PropertyObjectPtr& obj)
{
    if (!obj.assigned())
        return;

    for (const auto& prop: obj.getAllProperties())
        addItem(parent, createPropertyItem(obj, prop));
}
