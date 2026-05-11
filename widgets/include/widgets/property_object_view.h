#pragma once

#include <memory>
#include <unordered_map>

#include <opendaq/component_ptr.h>

#include "property/object_property_item.h"

class PropertyInspector;
class QLineEdit;
class QString;

// Custom hash function for PropertyObjectPtr that uses the object's address
struct PropertyObjectPtrHash
{
    std::size_t operator()(const daq::PropertyObjectPtr& ptr) const noexcept
    {
        return std::hash<void*>{}(ptr.getObject());
    }
};

// Key for items map: (PropertyObject owner, property name)
struct PropertyKey
{
    void* ownerPtr;  // Raw pointer for comparison
    std::string name;

    PropertyKey(const daq::PropertyObjectPtr& owner, const std::string& propName)
        : ownerPtr(owner.getObject()), name(propName)
    {}

    bool operator==(const PropertyKey& other) const
    {
        return ownerPtr == other.ownerPtr && name == other.name;
    }
};

// Hash function for PropertyKey
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
    // Used by PropertySubtreeBuilder
    BasePropertyItem* store(std::unique_ptr<BasePropertyItem> item);
    void addTopLevelItem(QTreeWidgetItem* item);

    // Notify that a property value has changed
    void onPropertyValueChanged(const daq::PropertyObjectPtr& obj, bool force = false);

    // QTreeWidget forwarding methods used by property items
    QRect visualItemRect(QTreeWidgetItem* item) const;
    int columnViewportPosition(int column) const;
    int currentColumn() const;
    QWidget* viewport() const;

    std::unordered_map<daq::PropertyObjectPtr, ObjectPropertyItem*, PropertyObjectPtrHash> propertyObjectToLogic;

    static BasePropertyItem* getLogic(QTreeWidgetItem* it);
    static void setLogic(QTreeWidgetItem* it, BasePropertyItem* logic);

private:
    friend class PropertySubtreeBuilder;

private Q_SLOTS:
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onSearchTextChanged(const QString& text);

private:
    void handleEditError(QTreeWidgetItem* item, int column, BasePropertyItem* logic, const char* errorMsg);
    daq::PropertyObjectPtr getChildObject(std::string path);
    void removeChildProperty(QTreeWidgetItem* parentWidget, const std::string& propName);
    void applyExpandState();
    void applyFilter();
    bool updateItemFilter(QTreeWidgetItem* item, const QString& text);
    void componentCoreEventCallback(daq::ComponentPtr& component, daq::CoreEventArgsPtr& eventArgs);

    daq::ComponentPtr owner;
    daq::PropertyObjectPtr root;
    std::string rootPath;
    std::unordered_map<PropertyKey, std::unique_ptr<BasePropertyItem>, PropertyKeyHash> items;

    QTreeWidget* tree = nullptr;
    QLineEdit* searchEdit = nullptr;
    PropertyInspector* m_inspector = nullptr;
};
