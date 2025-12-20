#pragma once

#include "property_factory.h"

#include <memory>
#include <vector>
#include <utility>

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

private:
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
    }

public Q_SLOTS:
    void refresh()
    {
        QSignalBlocker b(this);

        clear();
        items.clear();

        PropertySubtreeBuilder builder(*this);
        builder.buildFromPropertyObject(nullptr, root);

        expandAll();
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
            // For items without logic (like dict children), check parent
            auto* parent = item->parent();
            if (parent)
            {
                auto* parentLogic = getLogic(parent);
                if (parentLogic)
                {
                    // Parent will handle editability via handleChildEdit
                    return QTreeWidget::edit(index, trigger, event);
                }
            }
            return false;
        }

        // Check editability based on column
        if (index.column() == 0)
            return logic->isKeyEditable() && QTreeWidget::edit(index, trigger, event);
        else if (index.column() == 1)
            return logic->isValueEditable() && QTreeWidget::edit(index, trigger, event);

        return false;
    }

private:
    friend class PropertySubtreeBuilder;

    BasePropertyItem* store(std::unique_ptr<BasePropertyItem> item)
    {
        items.emplace_back(std::move(item));
        return items.back().get();
    }

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

        if (auto* logic = getLogic(item))
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

    // Editable only if allowed by logic; column-0 edit is blocked by PropertyObjectView::edit override.
    Qt::ItemFlags flags = it->flags();
    if (logic->isEditable())
        flags |= Qt::ItemIsEditable;
    else
        flags &= ~Qt::ItemIsEditable;

    it->setFlags(flags);

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
