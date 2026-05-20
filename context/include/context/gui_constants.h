#pragma once

#include <QSize>
#include <QString>

// GUI Constants for consistent sizing across the application
namespace GUIConstants {
    // Lazy-initialized once; use when filtering "our" device in discovery
    const QString& getClientManufacturer();
    const QString& getClientSerialNumber();

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

    constexpr int NEW_VERSION_DIALOG_WIDTH = 500;
    constexpr int NEW_VERSION_DIALOG_HEIGHT = 450;
    constexpr int NEW_VERSION_DIALOG_MIN_WIDTH = 400;
    constexpr int NEW_VERSION_DIALOG_MIN_HEIGHT = 300;

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

    // ============================================================================
    // Colors
    //
    // Adaptive tokens map to QPalette roles and are resolved by Qt at runtime,
    // so they automatically follow the system light/dark theme. Use these in
    // QSS strings for any color that should adapt (backgrounds, text, borders).
    //
    // Accent / status colors are fixed values chosen to retain reasonable
    // contrast against both light and dark surfaces.
    // ============================================================================
    namespace Colors
    {
        // --- Adaptive (palette) tokens for QSS ----------------------------------
        // Use directly in stylesheet strings, e.g. `color: palette(text);`.
        inline constexpr const char* WINDOW            = "palette(window)";          // main window / sidebars
        inline constexpr const char* SURFACE           = "palette(base)";            // cards / content surfaces
        inline constexpr const char* ALT_SURFACE       = "palette(alternate-base)";  // subtle inputs / sections
        inline constexpr const char* TEXT_PRIMARY      = "palette(text)";            // main foreground text
        inline constexpr const char* TEXT_ON_WINDOW    = "palette(window-text)";
        inline constexpr const char* TEXT_MUTED        = "palette(placeholder-text)";// secondary / muted text
        inline constexpr const char* TEXT_ON_HIGHLIGHT = "palette(highlighted-text)";
        inline constexpr const char* BORDER            = "palette(mid)";             // prefer GUIStyles::borderColor()
        inline constexpr const char* SEPARATOR         = "palette(midlight)";        // hairline separators
        inline constexpr const char* HOVER             = "palette(midlight)";        // subtle hover bg
        inline constexpr const char* HIGHLIGHT         = "palette(highlight)";       // system accent (selection)
        inline constexpr const char* LINK              = "palette(link)";
        inline constexpr const char* ACCENT            = "palette(accent)";             // primary accent color

        // --- Semantic / status colors (fixed) ----------------------------------
        inline constexpr const char* SUCCESS           = "#22c55e";
        inline constexpr const char* SUCCESS_BG        = "rgba(34, 197, 94, 38)";
        inline constexpr const char* WARNING           = "#f59e0b";
        inline constexpr const char* WARNING_BG        = "rgba(245, 158, 11, 38)";
        inline constexpr const char* ERROR_FG          = "#ef4444";
        inline constexpr const char* ERROR_BG          = "rgba(239, 68, 68, 38)";
        inline constexpr const char* INFO              = "#3b82f6";
        inline constexpr const char* INFO_BG           = "rgba(59, 130, 246, 38)";
        inline constexpr const char* DEBUG_FG          = "#8b5cf6";
        inline constexpr const char* DEBUG_BG          = "rgba(139, 92, 246, 38)";
        inline constexpr const char* NEUTRAL           = "#9ca3af";
        inline constexpr const char* NEUTRAL_BG        = "rgba(156, 163, 175, 38)";

        // Tag palette — fg/bg pairs that look acceptable in light & dark mode.
        // Each background uses alpha so the underlying surface tints through.
        struct TagSwatch { const char* bg; const char* fg; };
        inline constexpr TagSwatch TAG_PALETTE[] = {
            {"rgba(59, 130, 246, 45)",  "#3b82f6"},  // blue
            {"rgba(34, 197, 94, 45)",   "#22c55e"},  // green
            {"rgba(245, 158, 11, 45)",  "#f59e0b"},  // amber
            {"rgba(236, 72, 153, 45)",  "#ec4899"},  // pink
            {"rgba(139, 92, 246, 45)",  "#8b5cf6"},  // violet
            {"rgba(249, 115, 22, 45)",  "#f97316"},  // orange
        };
        inline constexpr int TAG_PALETTE_SIZE = sizeof(TAG_PALETTE) / sizeof(TAG_PALETTE[0]);
    }
}

