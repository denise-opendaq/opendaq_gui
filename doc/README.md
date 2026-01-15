# OpenDAQ Qt GUI Application

## Overview

OpenDAQ Qt GUI is a graphical user interface application for managing and interacting with OpenDAQ instances, devices, signals, and components. The application provides a modern, VSCode-like interface with detachable tabs, split views, and comprehensive component management.

## Features

- **Component Tree View**: Hierarchical display of OpenDAQ components (devices, function blocks, channels, signals, input ports)
- **Property Management**: View and edit component properties with real-time synchronization
- **Signal Monitoring**: Real-time signal value display with automatic updates
- **Status Display**: Component status monitoring with automatic updates
- **Detachable Tabs**: VSCode-like tab system with drag-and-drop, splitting, and detaching capabilities
- **Remote Component Support**: Full support for remote OpenDAQ components with async event handling
- **Device Discovery**: Automatic device discovery via mDNS
- **Logging**: Integrated logging system with Qt text edit sink

## Architecture

### Project Structure

```
qt_gui/
├── component/          # Component tree management
├── context/            # Application context and global state
├── dialogs/            # Dialog windows (add device, function block, etc.)
├── logger/             # Logging infrastructure
├── property/           # Property editing and display
├── widgets/            # Reusable UI widgets
├── icons/              # Application icons
├── external/           # External dependencies (Qt, OpenDAQ)
└── doc/                # Documentation
```

### Core Components

#### 1. MainWindow (`MainWindow.h/cpp`)
The main application window that orchestrates all UI components:
- Left panel: Component tree widget with view selector
- Right panel: Tab-based content area with split view support
- Bottom panel: Log viewer
- Menu bar with various actions

**Key Features:**
- VSCode-like tab system with drag-and-drop
- Split view support (horizontal/vertical)
- Detachable windows
- Tab management and layout restoration

#### 2. Component Tree (`component/`)
Hierarchical tree representation of OpenDAQ components:

- **ComponentTreeWidget**: Main tree widget displaying components
- **BaseTreeElement**: Base class for all tree elements
- **ComponentTreeElement**: Generic component representation
- **DeviceTreeElement**: Device-specific handling
- **FunctionBlockTreeElement**: Function block representation
- **SignalTreeElement**: Signal representation
- **InputPortTreeElement**: Input port representation
- **FolderTreeElement**: Folder/container elements

**Features:**
- Context menus for component actions
- Selection handling
- Tab opening for component views
- Event subscription for real-time updates

#### 3. Widgets (`widgets/`)

- **ComponentWidget**: Displays component properties and status
  - Property editing with bidirectional sync
  - Status display (from StatusContainer)
  - Async event handling to prevent deadlocks

- **PropertyObjectView**: Generic property editor
  - Tree view of properties
  - Support for various property types
  - Read-only and editable properties

- **SignalValueWidget**: Real-time signal value display
  - Automatic updates via UpdateScheduler
  - Unit display
  - Time-domain signal support

- **InputPortWidget**: Input port management
  - Signal connection/disconnection
  - Signal selector

- **FunctionBlockWidget**: Function block configuration

#### 4. Context (`context/`)

- **AppContext**: Global application context (singleton)
  - OpenDAQ instance management
  - Logger sink access
  - Component visibility settings
  - Update scheduler

- **UpdateScheduler**: Periodic update scheduler for widgets
  - Registers updatable widgets
  - Periodic refresh mechanism

- **IconProvider**: Icon management for components

#### 5. Property System (`property/`)

Property editing infrastructure:
- Property item types (string, bool, int, float, list, dict, etc.)
- Property editors for different types
- Property validation
- Change notification

#### 6. Dialogs (`dialogs/`)

- **AddDeviceDialog**: Add new device dialog
  - Connection string input
  - Device discovery
  - Configuration support

- **AddFunctionBlockDialog**: Add function block dialog
  - Function block type selection
  - Configuration support

- **AddDeviceConfigDialog**: Device configuration dialog

- **AddServerDialog**: Server connection dialog

#### 7. Logger (`logger/`)

- **QtTextEditSink**: Custom logger sink that outputs to QTextEdit
  - Integrated with MainWindow log panel
  - Real-time log display

### Key Design Patterns

#### 1. Async Event Handling
To prevent deadlocks when working with remote components, all component events are processed asynchronously:

```cpp
QPointer<ComponentWidget> weakThis(this);
QTimer::singleShot(0, this, [weakThis, ...]() {
    if (!weakThis) return;
    // Handle event asynchronously
});
```

#### 2. Update Guard Pattern
Prevents recursive updates when syncing between component and property object:

```cpp
struct UpdateGuard {
    UpdateGuard(std::atomic<int>& cnt) : cnt(cnt) { cnt++; }
    ~UpdateGuard() { cnt--; }
    std::atomic<int>& cnt;
};
```

#### 3. Weak References
Uses `QPointer` for safe widget references in async callbacks to prevent use-after-free.

#### 4. Event Subscription
Components subscribe to OpenDAQ core events for real-time updates:
- `AttributeChanged`: Component attribute changes
- `TagsChanged`: Tag changes
- `StatusChanged`: Status container changes
- `ConnectionStatusChanged`: Connection status changes

## Building

### Requirements

- CMake 3.16 or higher
- C++17 compatible compiler
- Qt6 (Core, Widgets)
- OpenDAQ SDK

### Build Steps

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The executable will be in `build/bin/opendaq_qt_gui`.

## Usage

### Starting the Application

```bash
./build/bin/opendaq_qt_gui
```

### Main Interface

1. **Left Panel**: Component tree
   - Select view type (System Overview, Signals, Channels, etc.)
   - Browse components hierarchically
   - Right-click for context menu

2. **Right Panel**: Content area
   - Tabs for component views
   - Drag tabs to split or detach
   - Multiple split views supported

3. **Bottom Panel**: Log viewer
   - Real-time OpenDAQ logs
   - Application logs

### Working with Components

- **Select Component**: Click on component in tree to open tabs
- **Edit Properties**: Double-click property values in Component tab
- **View Status**: Status displayed in Component tab as "Statuses" property
- **Connect Signals**: Use Input Port widget to connect/disconnect signals
- **Add Device**: Right-click on device → "Add Device"
- **Add Function Block**: Right-click on device → "Add Function Block"

### Tab Management

- **Split View**: Drag tab to edge to create split
- **Detach Tab**: Drag tab outside window to detach
- **Close Tab**: Click X button on tab
- **Restore Layout**: Menu → View → Reset Layout

## Technical Details

### Thread Safety

- All UI operations must be performed on the main Qt thread
- OpenDAQ operations may block, so async handling is used
- `QPointer` ensures safe widget access in async callbacks

### Event Flow

1. User action (e.g., edit property)
2. Property change triggers write handler
3. Component attribute updated (may block for remote components)
4. Component fires `AttributeChanged` event
5. Event handler receives event (may be on different thread)
6. Event queued to Qt event loop via `QTimer::singleShot`
7. Async handler updates property object
8. UI updates automatically

### Status Container Integration

Component statuses are displayed as a read-only "Statuses" property in ComponentWidget:
- Dictionary of status names to status values
- Includes status messages when available
- Automatically updates on `StatusChanged` events
- Async handling prevents deadlocks

## Dependencies

- **Qt6**: GUI framework
  - Core: Core functionality
  - Widgets: UI widgets

- **OpenDAQ**: Data acquisition framework
  - Core objects: Property system
  - Core types: Basic types
  - OpenDAQ: Main SDK

## Known Issues

- Remote component property changes may take time due to network latency
- Large component trees may impact performance
- Status updates are async to prevent deadlocks

## Future Improvements

- [ ] Property validation UI
- [ ] Signal plotting/graphing
- [ ] Configuration presets
- [ ] Export/import configurations
- [ ] Multi-instance support
- [ ] Performance profiling tools

## License

[Add license information here]

## Contributing

[Add contributing guidelines here]


