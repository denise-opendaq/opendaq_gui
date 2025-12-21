#pragma once

#include "property/property_factory.h"
#include "context/AppContext.h"

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
    explicit PropertyObjectView(const daq::PropertyObjectPtr& root, QWidget* parent = nullptr);
    virtual ~PropertyObjectView();

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void propertyChanged(const QString& propertyName, const QString& newValue);

protected:
    // Control editing based on column and item logic
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;

    void componentCoreEventCallback(daq::ComponentPtr& component, daq::CoreEventArgsPtr& eventArgs);

public:
    // Used by PropertySubtreeBuilder
    BasePropertyItem* store(std::unique_ptr<BasePropertyItem> item);

    std::unordered_map<daq::PropertyObjectPtr, ObjectPropertyItem*, PropertyObjectPtrHash> propertyObjectToLogic;

private:
    friend class PropertySubtreeBuilder;

    static BasePropertyItem* getLogic(QTreeWidgetItem* it);
    static void setLogic(QTreeWidgetItem* it, BasePropertyItem* logic);

private Q_SLOTS:
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    void handleEditError(QTreeWidgetItem* item, int column, BasePropertyItem* logic, const char* errorMsg);

private:
    daq::PropertyObjectPtr root;
    std::vector<std::unique_ptr<BasePropertyItem>> items;
};
