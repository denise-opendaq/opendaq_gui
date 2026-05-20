#pragma once

#include <memory>
#include <unordered_map>

#include <opendaq/component_ptr.h>

#include "property/object_property_item.h"

class PropertyInspector;
class QLineEdit;
class QString;

// Custom hash for PropertyObjectPtr — part of the public propertyObjectToLogic type.
struct PropertyObjectPtrHash
{
    std::size_t operator()(const daq::PropertyObjectPtr& ptr) const noexcept
    {
        return std::hash<void*>{}(ptr.getObject());
    }
};

// Key and hash for the private items map: (owner pointer, property name)
struct PropertyKey
{
    void* ownerPtr;
    std::string name;

    PropertyKey(const daq::PropertyObjectPtr& owner, const std::string& propName)
        : ownerPtr(owner.getObject()), name(propName)
    {}

    bool operator==(const PropertyKey& other) const
    {
        return ownerPtr == other.ownerPtr && name == other.name;
    }
};

struct PropertyKeyHash
{
    std::size_t operator()(const PropertyKey& key) const noexcept
    {
        std::size_t h1 = std::hash<void*>{}(key.ownerPtr);
        std::size_t h2 = std::hash<std::string>{}(key.name);
        return h1 ^ (h2 << 1);
    }
};

// ============================================================================
// PropertyObjectView — QWidget containing tree + inspector in a splitter
// ============================================================================

class PropertyObjectView : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyObjectView(const daq::PropertyObjectPtr& root,
                                QWidget* parent = nullptr,
                                const daq::ComponentPtr& owner = nullptr);
    virtual ~PropertyObjectView();

Q_SIGNALS:
    void propertySelected(BasePropertyItem* item);

public Q_SLOTS:
    void refresh();

public:
    // Build tree items for a property object into the given parent widget item
    void buildFromPropertyObject(QTreeWidgetItem* parent, const daq::PropertyObjectPtr& obj);

    // QTreeWidget forwarding methods used by property items
    QRect visualItemRect(QTreeWidgetItem* item) const;
    int columnViewportPosition(int column) const;
    int currentColumn() const;
    QWidget* viewport() const;

    std::unordered_map<daq::PropertyObjectPtr, ObjectPropertyItem*, PropertyObjectPtrHash> propertyObjectToLogic;

    static BasePropertyItem* getLogic(QTreeWidgetItem* it);
    static void setLogic(QTreeWidgetItem* it, BasePropertyItem* logic);

private:
    BasePropertyItem* store(std::unique_ptr<BasePropertyItem> item);
    void addTopLevelItem(QTreeWidgetItem* item);
    QTreeWidgetItem* addItem(QTreeWidgetItem* parent, std::unique_ptr<BasePropertyItem> item);

    void handleEditError(QTreeWidgetItem* item, int column, BasePropertyItem* logic, const char* errorMsg);
    BasePropertyItem* getLogicWithParent(QTreeWidgetItem* item) const;
    daq::PropertyObjectPtr getChildObject(std::string path);
    void removeChildProperty(QTreeWidgetItem* parentWidget, const std::string& propName);
    void applyExpandState();
    void applyFilter();
    bool updateItemFilter(QTreeWidgetItem* item, const QString& text);
    void componentCoreEventCallback(daq::ComponentPtr& component, daq::CoreEventArgsPtr& eventArgs);

private Q_SLOTS:
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onSearchTextChanged(const QString& text);

private:
    daq::ComponentPtr owner;
    daq::PropertyObjectPtr root;
    std::string rootPath;
    std::unordered_map<PropertyKey, std::unique_ptr<BasePropertyItem>, PropertyKeyHash> items;

    QTreeWidget* tree = nullptr;
    QLineEdit* searchEdit = nullptr;
    PropertyInspector* m_inspector = nullptr;
};
