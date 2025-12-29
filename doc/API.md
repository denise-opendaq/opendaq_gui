# API Documentation

## Core Classes

### AppContext

Singleton class providing global application state.

**Location**: `context/include/context/AppContext.h`

**Key Methods**:
- `static AppContext* instance()` - Get singleton instance
- `daq::InstancePtr daqInstance()` - Get OpenDAQ instance
- `void setDaqInstance(const daq::InstancePtr& instance)` - Set OpenDAQ instance
- `UpdateScheduler* updateScheduler()` - Get update scheduler
- `daq::LoggerComponentPtr getLoggerComponent()` - Get logger for logging

**Usage**:
```cpp
// two ways to get instance
auto instance = AppContext::Instance()->daqInstance(); 
auto instance = AppContext::Daq(); 

auto loggerComponent = AppContext::LoggerComponent()
LOG_W("Message"); // uses Logger Component
```

### ComponentWidget

Widget for displaying and editing component properties and status.

**Location**: `widgets/include/widgets/component_widget.h`

**Key Features**:
- Property editing with bidirectional sync
- Status display from StatusContainer
- Async event handling

**Constructor**:
```cpp
ComponentWidget(const daq::ComponentPtr& component, QWidget* parent = nullptr)
```

**Internal Methods**:
- `void setupUI()` - Initialize UI
- `void createComponentPropertyObject()` - Create property wrapper
- `void setupPropertyHandlers()` - Setup property change handlers
- `void updateStatuses()` - Update status display
- `void onCoreEvent(...)` - Handle component events

### PropertyObjectView

Generic property editor widget.

**Location**: `widgets/include/widgets/property_object_view.h`

**Constructor**:
```cpp
PropertyObjectView(
    const daq::PropertyObjectPtr& propertyObject,
    QWidget* parent = nullptr,
    const daq::ComponentPtr& component = nullptr
)
```

**Features**:
- Tree view of properties
- Inline editing
- Support for nested properties
- Read-only property indication

### ComponentTreeWidget

Tree widget displaying OpenDAQ component hierarchy.

**Location**: `component/include/component/component_tree_widget.h`

**Key Methods**:
- `void loadInstance(const daq::InstancePtr& instance)` - Load OpenDAQ instance
- `BaseTreeElement* getSelectedElement()` - Get selected element
- `void setShowHidden(bool show)` - Show/hide invisible components

**Signals**:
- `void componentSelected(BaseTreeElement* element)` - Emitted when component selected

### BaseTreeElement

Base class for all tree elements.

**Location**: `component/include/component/base_tree_element.h`

**Key Methods**:
- `virtual void onSelected(QWidget* mainContent)` - Called when selected
- `virtual QStringList getAvailableTabNames() const` - Get available tabs
- `virtual void openTab(const QString& tabName, QWidget* mainContent)` - Open tab
- `virtual QMenu* onCreateRightClickMenu(QWidget* parent)` - Create context menu

**Derived Classes**:
- `ComponentTreeElement` - Generic component
- `DeviceTreeElement` - Device component
- `FunctionBlockTreeElement` - Function block
- `SignalTreeElement` - Signal
- `InputPortTreeElement` - Input port

### DetachableTabWidget

VSCode-like tab widget with drag-and-drop support.

**Location**: `DetachableTabWidget.h`

**Key Features**:
- Drag tabs to split view
- Drag tabs to detach window
- Tab reordering
- Tab closing

**Signals**:
- `void tabDetached(QWidget* widget, const QString& title, const QPoint& globalPos)`
- `void tabSplitRequested(int index, Qt::Orientation orientation, DropZone zone)`
- `void tabMoveCompleted(DetachableTabWidget* sourceWidget)`

## Widget Classes

### SignalValueWidget

Displays real-time signal values.

**Location**: `widgets/include/widgets/signal_value_widget.h`

**Constructor**:
```cpp
SignalValueWidget(const daq::SignalPtr& signal, QWidget* parent = nullptr)
```

**Features**:
- Automatic updates via UpdateScheduler
- Unit display
- Time-domain signal support
- Last value display

### InputPortWidget

Widget for managing input port connections.

**Location**: `widgets/include/widgets/input_port_widget.h`

**Features**:
- Signal connection/disconnection
- Signal selector
- Connection status display

### FunctionBlockWidget

Widget for function block configuration.

**Location**: `widgets/include/widgets/function_block_widget.h`

## Dialog Classes

### AddDeviceDialog

Dialog for adding new devices.

**Location**: `dialogs/include/dialogs/add_device_dialog.h`

**Methods**:
- `QString getConnectionString()` - Get entered connection string
- `daq::PropertyObjectPtr getConfig()` - Get device configuration

### AddFunctionBlockDialog

Dialog for adding function blocks.

**Location**: `dialogs/include/dialogs/add_function_block_dialog.h`

**Methods**:
- `QString getFunctionBlockType()` - Get selected function block type
- `daq::PropertyObjectPtr getConfig()` - Get function block configuration

## Utility Classes

### UpdateScheduler

Schedules periodic updates for widgets.

**Location**: `context/include/context/UpdateScheduler.h`

**Interface**: `IUpdatable`
```cpp
class IUpdatable {
public:
    virtual void onScheduledUpdate() = 0;
};
```

**Methods**:
- `void registerUpdatable(IUpdatable* updatable)` - Register widget
- `void unregisterUpdatable(IUpdatable* updatable)` - Unregister widget

### IconProvider

Provides icons for components.

**Location**: `context/include/context/icon_provider.h`

**Methods**:
- `QIcon getIcon(const QString& iconName)` - Get icon by name

## Property System

### Property Items

Located in `property/include/property/`

**Types**:
- `StringPropertyItem` - String properties
- `BoolPropertyItem` - Boolean properties
- `IntPropertyItem` - Integer properties
- `FloatPropertyItem` - Float properties
- `ListPropertyItem` - List properties
- `DictPropertyItem` - Dictionary properties
- `EnumPropertyItem` - Enumeration properties
- `ObjectPropertyItem` - Object properties

**Base Class**: `BasePropertyItem`

**Key Methods**:
- `virtual QString showValue()` - Display value as string
- `virtual void setValue(const QString& value)` - Set value from string
- `virtual bool isReadOnly()` - Check if read-only
- `virtual bool hasSubtree()` - Check if has nested properties

## Event Handling

### Component Event Subscription

```cpp
// Subscribe
*AppContext::DaqEvent() += daq::event(this, &Widget::onCoreEvent);

// Handler signature
void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

// Unsubscribe (in destructor)
*AppContext::DaqEvent() -= daq::event(this, &Widget::onCoreEvent);
```

### Property Change Handling

```cpp
// Subscribe to property write
propertyObject.getOnPropertyValueWrite("PropertyName") += 
    [this](daq::PropertyObjectPtr& obj, daq::PropertyValueEventArgsPtr& args) {
        // Handle property change
    };
```

### Async Event Handling Pattern

```cpp
void Widget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args) {
    QPointer<Widget> weakThis(this);
    QTimer::singleShot(0, this, [weakThis, ...]() {
        if (!weakThis) return;
        // Handle event asynchronously
    });
}
```

## Constants and Enums

### View Types

Defined in `MainWindow`:
- System Overview
- Signals
- Channels
- Function blocks
- Full Topology

### Drop Zones

Defined in `DetachableTabWidget`:
- `DropZone::Left`
- `DropZone::Right`
- `DropZone::Top`
- `DropZone::Bottom`
- `DropZone::Center`

## Best Practices

### 1. Always Use Weak References in Async Callbacks

```cpp
QPointer<Widget> weakThis(this);
QTimer::singleShot(0, this, [weakThis]() {
    if (!weakThis) return;
    // Safe to use weakThis
});
```

### 2. Unsubscribe from Events in Destructor

```cpp
~Widget() {
    if (component.assigned()) {
        *AppContext::DaqEvent() -= daq::event(this, &Widget::onCoreEvent);
    }
}
```

### 3. Use UpdateGuard to Prevent Recursive Updates

```cpp
UpdateGuard guard(updatingFromComponent);
// Update property object
```

### 4. Handle Exceptions in OpenDAQ Operations

```cpp
try {
    // OpenDAQ operation
} catch (const std::exception& e) {
    const auto logger = AppContext::LoggerComponent();
    LOG_W("Error: {}", e.what());
}
```

### 5. Check Component Assignment

```cpp
if (!component.assigned())
    return;
```

