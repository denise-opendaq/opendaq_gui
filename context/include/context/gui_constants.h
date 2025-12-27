#pragma once

#include <QSize>

// GUI Constants for consistent sizing across the application
namespace GUIConstants {
    // Window sizes
    constexpr int DEFAULT_WINDOW_WIDTH = 1200;
    constexpr int DEFAULT_WINDOW_HEIGHT = 800;
    constexpr int MIN_WINDOW_WIDTH = 800;
    constexpr int MIN_WINDOW_HEIGHT = 600;
    
    // Dialog sizes
    constexpr int DEFAULT_DIALOG_WIDTH = 600;
    constexpr int DEFAULT_DIALOG_HEIGHT = 400;
    constexpr int DEVICE_INFO_DIALOG_WIDTH = 600;
    constexpr int DEVICE_INFO_DIALOG_HEIGHT = 400;
    
    // Add Device/Server/Function Block dialog sizes
    constexpr int ADD_DEVICE_DIALOG_WIDTH = 800;
    constexpr int ADD_DEVICE_DIALOG_HEIGHT = 500;
    constexpr int ADD_DEVICE_DIALOG_MIN_WIDTH = 600;
    constexpr int ADD_DEVICE_DIALOG_MIN_HEIGHT = 400;
    constexpr int ADD_DEVICE_INFO_DIALOG_WIDTH = 800;
    constexpr int ADD_DEVICE_INFO_DIALOG_HEIGHT = 600;
    
    constexpr int ADD_SERVER_DIALOG_WIDTH = 1000;
    constexpr int ADD_SERVER_DIALOG_HEIGHT = 600;
    constexpr int ADD_SERVER_DIALOG_MIN_WIDTH = 800;
    constexpr int ADD_SERVER_DIALOG_MIN_HEIGHT = 500;
    
    constexpr int ADD_FUNCTION_BLOCK_DIALOG_WIDTH = 1000;
    constexpr int ADD_FUNCTION_BLOCK_DIALOG_HEIGHT = 600;
    constexpr int ADD_FUNCTION_BLOCK_DIALOG_MIN_WIDTH = 800;
    constexpr int ADD_FUNCTION_BLOCK_DIALOG_MIN_HEIGHT = 500;
    
    constexpr int ADD_DEVICE_CONFIG_DIALOG_WIDTH = 1200;
    constexpr int ADD_DEVICE_CONFIG_DIALOG_HEIGHT = 600;
    constexpr int ADD_DEVICE_CONFIG_DIALOG_MIN_WIDTH = 1000;
    constexpr int ADD_DEVICE_CONFIG_DIALOG_MIN_HEIGHT = 500;
    
    // Splitter sizes
    constexpr int DEFAULT_VERTICAL_SPLITTER_CONTENT = 600;
    constexpr int DEFAULT_VERTICAL_SPLITTER_LOG = 200;
    constexpr int DEFAULT_HORIZONTAL_SPLITTER_LEFT = 300;
    constexpr int DEFAULT_HORIZONTAL_SPLITTER_RIGHT = 900;
    
    // Layout margins and spacing
    constexpr int DEFAULT_LAYOUT_MARGIN = 8;
    constexpr int DEFAULT_LAYOUT_SPACING = 8;
    constexpr int DIALOG_LAYOUT_MARGIN = 0;
    constexpr int DIALOG_LAYOUT_SPACING = 0;
}

