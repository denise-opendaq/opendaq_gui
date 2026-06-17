#pragma once

#include <QString>

#include <QColor>

// ============================================================================
// GUIStyles
//
// Reusable Qt stylesheet snippets and inline label styles used across the
// application. All visual values are expressed via QPalette adaptive tokens
// (palette(text), palette(base), palette(mid), ...) and the fixed accent /
// status colors defined in GUIConstants::Colors, so widgets follow the system
// light/dark theme automatically.
//
// Two styles of helpers are provided:
//   * inline constexpr const char* — for short single-rule label styles that
//     don't need formatting. Use directly with QWidget::setStyleSheet().
//   * QString-returning functions — for multi-selector / parameterized
//     stylesheets that need string interpolation.
// ============================================================================
namespace GUIStyles
{
    // ------------------------------------------------------------------------
    // Label inline styles
    // ------------------------------------------------------------------------

    // Big card title: name shown in the header card of a Component / Device /
    // Signal / FunctionBlock widget. 20px / 700.
    inline constexpr const char* HEADER_TITLE_LABEL =
        "font-size: 20px; font-weight: 700; color: palette(text);";

    // Section title shown on top of a content section (e.g. "Properties",
    // "Recording", "Input ports"). 16px / 600.
    inline constexpr const char* SECTION_TITLE_LABEL =
        "color: palette(text); font-size: 16px; font-weight: 600;"
        " background: transparent; border: none;";

    // Description line shown beneath the header title. 13px / muted.
    inline constexpr const char* DESCRIPTION_LABEL =
        "font-size: 13px; color: palette(placeholder-text);";

    // Caption for label-in-row layouts ("Domain:", "Tick frequency:", ...).
    inline constexpr const char* CAPTION_LABEL =
        "color: palette(placeholder-text); font-size: 13px;";

    // Value for label-in-row layouts.
    inline constexpr const char* VALUE_LABEL =
        "color: palette(text); font-size: 13px; font-weight: 500;";

    // Value rendered with a monospace font (e.g. timestamps, raw values).
    inline constexpr const char* MONO_VALUE_LABEL =
        "color: palette(text); font-size: 13px; font-weight: 500; font-family: monospace;";

    // Small UPPERCASE caption used as a "section eyebrow" (11px / 600 /
    // text-transform: uppercase / muted). Includes a transparent background
    // so it sits cleanly inside cards.
    inline constexpr const char* SMALL_CAPS_LABEL =
        "color: palette(placeholder-text); font-size: 11px; font-weight: 600;"
        " text-transform: uppercase; letter-spacing: 0.5px;"
        " background: transparent; border: none;";

    // Empty-state placeholder text. 14px / muted.
    inline constexpr const char* EMPTY_STATE_LABEL =
        "color: palette(placeholder-text); font-size: 14px;";

    // Error inline text (red, 14px). Use sparingly — prefer a status pill.
    inline constexpr const char* ERROR_INLINE_LABEL_FMT =
        "color: %1; font-size: 14px;";  // %1 = GUIConstants::Colors::ERROR_FG

    // ------------------------------------------------------------------------
    // Hairline separators (QFrame::HLine / VLine)
    QString midlightColor();
    QString borderColor();
    QString hairlineHorizontalStyle();
    QString hairlineVerticalStyle();

    /// Adaptive "card" container — light surface with subtle border + rounded
    /// corners. Caller must `setObjectName(objectName)` on the widget and set
    /// `Qt::WA_StyledBackground` so the style applies only to that widget.
    ///
    /// @param objectName  the unique object name targeted in the QSS selector.
    /// @param qssClass    "QWidget" / "QFrame" / etc.; selects the widget type.
    /// @param radius      border radius in px (default 16).
    QString cardStyle(const QString& objectName,
                      const QString& qssClass = QStringLiteral("QWidget"),
                      int radius = 16);

    /// Sidebar panel (left rail in dialogs): adaptive background with thin border.
    /// Caller must `setObjectName(objectName)`.
    QString sidebarPanelStyle(const QString& objectName);
    QString sidebarPanelStyle(const QString& objectName, const QColor& background);

    /// Underline-style QTabBar used by the main component view, dialog tab
    /// strips, etc. Selected tab gets the system accent underline.
    QString underlineTabBarStyle();

    /// Underline tab bar for the component view: brand-blue selected label +
    /// underline; blue bottom line on hover for other tabs.
    QString underlineTabBarAccentStyle();

    /// Sidebar QTreeWidget style (component-like, transparent selection — visual
    /// selection state is painted by the delegate, not the stylesheet).
    QString sidebarTreeStyle();

    /// Rounded input field with focus border. Used for search boxes and the
    /// generic input look in dialogs. Adaptive across themes.
    QString searchLineEditStyle();

    /// Rounded combo box matching `searchLineEditStyle()`.
    QString roundedComboBoxStyle();

    /// Big primary call-to-action button (e.g. "Add Function Block"). Uses the
    /// fixed brand accent so the action stands out regardless of theme.
    QString primaryButtonStyle();

    /// Square icon tool button: thin border, rounded, transparent bg, hover
    /// fill from `palette(midlight)`. Optionally adds a `:checked` state.
    QString iconToolButtonStyle(bool withCheckedState = false);

    /// Rounded ("pill") tool button matching the rounded input style — used by
    /// e.g. the disconnect button in the input-port selector row.
    QString roundedToolButtonStyle();

    /// Style for an inline link-like QPushButton (no border, accent color,
    /// underline on hover). Caller provides the accent color.
    QString linkButtonStyleFor(const QString& accent);

    /// Inline transparent QLineEdit used inside status-message rows.
    QString transparentInlineLineEditStyle();

    /// Inline transparent QPlainTextEdit used inside expanded status messages.
    QString transparentInlinePlainTextEditStyle();

    /// Background style for a small tag/pill widget.
    ///   bg — full color expression (HEX or `rgba(...)`).
    QString tagBubbleStyle(const QString& bg);

    /// Label style for the text inside a tag/pill.
    QString tagBubbleLabelStyle(const QString& fg);

    /// "Remove" button rendered inside a tag/pill (the × at the right).
    QString tagBubbleRemoveButtonStyle(const QString& fg);

    /// Single-color status text style used for "● Active" / "● Inactive"
    /// labels. The caller provides the color.
    QString coloredStatusLabelStyle(const QString& color);

    /// Status dot label style. Caller provides the color.
    QString statusDotStyle(const QString& color);

    /// QTableWidget log-viewer body style (used by the spdlog table sink).
    QString logTableStyle();

    /// Pill QPushButton style for log-level filters.
    ///   bg / fg are full color expressions; caller decides if the pill is
    ///   "checked" (highlighted) or "unchecked" (neutral).
    QString pillButtonStyle(const QString& fg, bool bold);

    /// Neutral (unchecked) pill style using adaptive tokens.
    QString neutralPillStyle();
}

