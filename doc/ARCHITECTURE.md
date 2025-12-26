# Architecture Documentation

## System Architecture

### High-Level Overview

```
┌─────────────────────────────────────────────────────────┐
│                    MainWindow                            │
│  ┌──────────────┐  ┌─────────────────────────────────┐ │
│  │ Component    │  │   DetachableTabWidget            │ │
│  │ Tree Widget  │  │   ┌─────────────────────────────┐│ │
│  │              │  │   │ ComponentWidget             ││ │
│  │              │  │   │ ┌─────────────────────────┐││ │
│  │              │  │   │ │ PropertyObjectView       │││ │
│  │              │  │   │ │ Statuses (from Status)   │││ │
│  │              │  │   │ └─────────────────────────┘││ │
│  │              │  │   └─────────────────────────────┘│ │
│  └──────────────┘  └─────────────────────────────────┘ │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │              Log Viewer (QTextEdit)                 │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Component Interaction Flow

```
User Action
    │
    ├─→ ComponentTreeWidget
    │       │
    │       └─→ BaseTreeElement::onSelected()
    │               │
    │               └─→ ComponentTreeElement::openTab()
    │                       │
    │                       └─→ ComponentWidget
    │                               │
    │                               ├─→ PropertyObjectView
    │                               │       └─→ Property editing
    │                               │
    │                               └─→ Status display
    │
    └─→ Property Change
            │
            ├─→ PropertyObjectView::onPropertyChanged()
            │       │
            │       └─→ ComponentWidget::setupPropertyHandlers()
            │               │
            │               └─→ component.setName() / setActive() / etc.
            │                       │
            │                       └─→ [May block for remote components]
            │                               │
            │                               └─→ Component fires AttributeChanged
            │                                       │
            │                                       └─→ ComponentWidget::onCoreEvent()
            │                                               │
            │                                               └─→ QTimer::singleShot() [Async]
            │                                                       │
            │                                                       └─→ handleAttributeChangedAsync()
            │                                                               │
            │                                                               └─→ PropertyObjectView update
```

## Module Dependencies

```
main.cpp
    │
    ├─→ MainWindow
    │       │
    │       ├─→ ComponentTreeWidget
    │       │       │
    │       │       ├─→ BaseTreeElement
    │       │       │       │
    │       │       │       ├─→ ComponentTreeElement
    │       │       │       ├─→ DeviceTreeElement
    │       │       │       ├─→ FunctionBlockTreeElement
    │       │       │       ├─→ SignalTreeElement
    │       │       │       └─→ InputPortTreeElement
    │       │       │
    │       │       └─→ ComponentWidget
    │       │               │
    │       │               ├─→ PropertyObjectView
    │       │               │       │
    │       │               │       └─→ Property items (property/)
    │       │               │
    │       │               └─→ Status display (from StatusContainer)
    │       │
    │       ├─→ DetachableTabWidget
    │       │       │
    │       │       └─→ Various widgets (SignalValueWidget, etc.)
    │       │
    │       └─→ Log Viewer
    │               │
    │               └─→ QtTextEditSink (logger/)
    │
    └─→ AppContext (context/)
            │
            ├─→ UpdateScheduler
            │       │
            │       └─→ SignalValueWidget (periodic updates)
            │
            └─→ IconProvider
```

## Data Flow

### Property Editing Flow

```
1. User edits property in PropertyObjectView
   │
2. PropertyObjectView emits property change
   │
3. ComponentWidget::setupPropertyHandlers() receives event
   │
4. Handler calls component.setName() / setActive() / etc.
   │
5. For remote components: Network call (may block)
   │
6. Component fires AttributeChanged event
   │
7. ComponentWidget::onCoreEvent() receives event
   │
8. Event queued to Qt event loop via QTimer::singleShot()
   │
9. handleAttributeChangedAsync() executes on main thread
   │
10. PropertyObjectView updated via setProtectedPropertyValue()
```

### Status Update Flow

```
1. Component status changes (remote or local)
   │
2. Component fires StatusChanged event
   │
3. ComponentWidget::onCoreEvent() receives event
   │
4. Event queued to Qt event loop via QTimer::singleShot()
   │
5. handleStatusChangedAsync() executes
   │
6. updateStatuses() called
   │
7. StatusContainer queried for all statuses
   │
8. Statuses property updated in componentPropertyObject
   │
9. PropertyObjectView automatically refreshes
```

## Threading Model

### Main Thread (Qt GUI Thread)
- All UI operations
- Widget creation/destruction
- Event handling
- Property updates

### OpenDAQ Threads
- Network operations (may block)
- Event callbacks (may be on different thread)
- Status updates

### Thread Safety Mechanisms

1. **QPointer**: Weak references for async callbacks
   ```cpp
   QPointer<ComponentWidget> weakThis(this);
   QTimer::singleShot(0, this, [weakThis]() {
       if (!weakThis) return; // Safe check
       // Use weakThis
   });
   ```

2. **QTimer::singleShot**: Queue operations to main thread
   - Ensures all UI updates happen on main thread
   - Prevents deadlocks with blocking operations

3. **UpdateGuard**: Prevents recursive updates
   - Atomic counter tracks update depth
   - Prevents infinite loops

## Memory Management

### Qt Object Ownership
- Parent-child relationships for automatic cleanup
- QPointer for weak references
- QObject::deleteLater() for deferred deletion

### OpenDAQ Object Management
- Reference counting (smart pointers)
- Automatic cleanup when last reference released
- Safe to copy in lambdas (reference counting)

## Event System

### OpenDAQ Events
- `AttributeChanged`: Component attribute modified
- `TagsChanged`: Component tags modified
- `StatusChanged`: Component status modified
- `ConnectionStatusChanged`: Device connection status changed
- `ComponentAdded`: New component added
- `ComponentRemoved`: Component removed

### Qt Events
- Custom drag-and-drop events for tabs
- Mouse/keyboard events
- Paint events

### Event Subscription Pattern
```cpp
// Subscribe
component.getOnComponentCoreEvent() += daq::event(this, &Widget::onCoreEvent);

// Unsubscribe (in destructor)
component.getOnComponentCoreEvent() -= daq::event(this, &Widget::onCoreEvent);
```

## Error Handling

### Exception Handling
- All OpenDAQ operations wrapped in try-catch
- Errors logged via AppContext::getLoggerComponent()
- UI remains responsive even on errors

### Async Error Handling
- Errors in async handlers logged but don't crash UI
- Weak pointer checks prevent use-after-free

## Performance Considerations

### Update Scheduling
- SignalValueWidget uses UpdateScheduler for periodic updates
- Prevents excessive updates
- Configurable update frequency

### Lazy Loading
- Component tree loads on demand
- Properties loaded when tab opened
- Icons loaded from resource system

### Caching
- Property objects cached in ComponentWidget
- Status container cached
- Icons cached in IconProvider

## Testing Considerations

### Unit Testing
- Each module can be tested independently
- Mock OpenDAQ components for testing
- Qt Test framework for UI testing

### Integration Testing
- Test with real OpenDAQ instances
- Test remote component scenarios
- Test async event handling

## Extension Points

### Adding New Component Types
1. Create new TreeElement class (inherit from BaseTreeElement)
2. Register in ComponentFactory
3. Add icon to icons/
4. Implement onSelected() and openTab()

### Adding New Widgets
1. Create widget class
2. Add to widgets/ module
3. Register in CMakeLists.txt
4. Use in ComponentTreeElement::openTab()

### Adding New Property Types
1. Create PropertyItem class in property/
2. Implement editor widget
3. Register in PropertyObjectView
4. Handle serialization/deserialization

