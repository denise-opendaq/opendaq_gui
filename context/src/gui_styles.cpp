#include "context/gui_styles.h"
#include "context/gui_constants.h"

namespace GUIStyles
{

QString cardStyle(const QString& objectName, const QString& qssClass, int radius)
{
    return QStringLiteral(
               "%1#%2 {"
               "  background: palette(base);"
               "  border: 1px solid palette(mid);"
               "  border-radius: %3px;"
               "}")
        .arg(qssClass, objectName)
        .arg(radius);
}

QString sidebarPanelStyle(const QString& objectName)
{
    return QStringLiteral(
               "QWidget#%1 {"
               "  background-color: palette(window);"
               "  border: 1px solid palette(mid);"
               "}")
        .arg(objectName);
}

QString underlineTabBarStyle()
{
    return QStringLiteral(
        "QTabWidget { background: transparent; }"
        "QTabWidget::pane { border: none; background: transparent; }"
        "QTabBar { background: transparent; border: none; }"
        "QTabBar::tab {"
        "  padding: 10px 18px;"
        "  color: palette(placeholder-text);"
        "  background: transparent;"
        "  font-size: 13px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "}"
        "QTabBar::tab:selected {"
        "  color: palette(highlight);"
        "  background: transparent;"
        "  border-bottom: 2px solid palette(highlight);"
        "  font-weight: 600;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  color: palette(text);"
        "  border-bottom: 2px solid palette(mid);"
        "}");
}

QString sidebarTreeStyle()
{
    return QStringLiteral(
        "QTreeWidget {"
        "  background-color: palette(window);"
        "  border: none;"
        "  font-size: 13px;"
        "  outline: 0;"
        "  show-decoration-selected: 1;"
        "  color: palette(text);"
        "  selection-background-color: transparent;"
        "  selection-color: palette(text);"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  border-radius: 8px;"
        "}"
        "QTreeWidget::item:selected, QTreeWidget::item:selected:active, QTreeWidget::item:selected:!active {"
        "  color: palette(text);"
        "}"
        "QTreeView::branch {"
        "  background: transparent;"
        "}"
        "QTreeView::branch:selected {"
        "  background: transparent;"
        "}"
        "QHeaderView::section {"
        "  background-color: palette(window);"
        "  border: none;"
        "  padding: 6px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  color: palette(placeholder-text);"
        "}"
        "QHeaderView {"
        "  background-color: palette(window);"
        "}");
}

QString searchLineEditStyle()
{
    return QStringLiteral(
        "QLineEdit {"
        "  background-color: palette(alternate-base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  padding: 8px 10px;"
        "  font-size: 13px;"
        "  color: palette(text);"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid palette(highlight);"
        "  background-color: palette(base);"
        "}");
}

QString roundedComboBoxStyle()
{
    return QStringLiteral(
        "QComboBox {"
        "  background-color: palette(alternate-base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  padding: 9px 12px;"
        "  font-size: 13px;"
        "  color: palette(text);"
        "}"
        "QComboBox:focus {"
        "  border: 1px solid palette(highlight);"
        "  background-color: palette(base);"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 24px;"
        "}");
}

QString primaryButtonStyle()
{
    // Filled accent button with hover/pressed states. Foreground is
    // `palette(highlighted-text)` so the button label stays readable on the
    // brand-blue fill in both light and dark modes.
    return QStringLiteral(
               "QPushButton {"
               "  border: 1.5px solid %1;"
               "  border-radius: 10px;"
               "  padding: 10px 10px;"
               "  color: palette(highlighted-text);"
               "  background-color: %1;"
               "}"
               "QPushButton:hover {"
               "  background-color: %2;"
               "  border: 1.5px solid %2;"
               "  border-radius: 12px;"
               "  color: palette(highlighted-text);"
               "}"
               "QPushButton:pressed {"
               "  background-color: %3;"
               "  border: 1.5px solid %3;"
               "  border-radius: 12px;"
               "  color: palette(highlighted-text);"
               "}"
               "QPushButton:disabled {"
               "  border: 1.5px solid palette(mid);"
               "  border-radius: 10px;"
               "  padding: 10px 10px;"
               "  color: palette(placeholder-text);"
               "  background-color: palette(midlight);"
               "}")
        .arg(GUIConstants::Colors::ACCENT,
             GUIConstants::Colors::ACCENT_HOVER,
             GUIConstants::Colors::ACCENT_PRESSED);
}

QString iconToolButtonStyle(bool withCheckedState)
{
    QString s = QStringLiteral(
        "QToolButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "  background: transparent;"
        "}"
        "QToolButton:hover { background: palette(midlight); }");

    if (withCheckedState)
    {
        s += QStringLiteral(
            "QToolButton:checked {"
            "  background: %1;"
            "  border-color: %2;"
            "}").arg(GUIConstants::Colors::WARNING_BG,
                     GUIConstants::Colors::WARNING);
    }
    return s;
}

QString roundedToolButtonStyle()
{
    return QStringLiteral(
        "QToolButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  background: palette(alternate-base);"
        "  padding: 6px;"
        "}"
        "QToolButton:hover { background: palette(midlight); }"
        "QToolButton:pressed { background: palette(mid); }"
        "QToolButton:disabled { color: palette(placeholder-text); background: palette(alternate-base); }");
}

QString linkButtonStyleFor(const QString& accent)
{
    return QStringLiteral(
               "QPushButton { color: %1; font-size: 13px; padding: 0 4px; border: none; background: transparent; }"
               "QPushButton:hover { text-decoration: underline; }")
        .arg(accent);
}

QString transparentInlineLineEditStyle()
{
    return QStringLiteral(
        "QLineEdit { background: transparent; color: palette(placeholder-text); font-size: 13px;"
        " padding: 0; margin: 0; border: none;"
        " selection-background-color: palette(highlight); selection-color: palette(highlighted-text); }");
}

QString transparentInlinePlainTextEditStyle()
{
    return QStringLiteral(
        "QPlainTextEdit { background: transparent; color: palette(placeholder-text); font-size: 13px;"
        " padding: 0; margin: 0; border: none;"
        " selection-background-color: palette(highlight); selection-color: palette(highlighted-text); }");
}

QString tagBubbleStyle(const QString& bg)
{
    return QStringLiteral("QWidget { background: %1; border-radius: 10px; }").arg(bg);
}

QString tagBubbleLabelStyle(const QString& fg)
{
    return QStringLiteral(
               "color: %1; font-size: 12px; font-weight: 600;"
               " background: transparent; border: none;")
        .arg(fg);
}

QString tagBubbleRemoveButtonStyle(const QString& fg)
{
    return QStringLiteral(
               "QPushButton { color: %1; background: transparent; border: none;"
               " font-size: 13px; font-weight: 700; padding: 0; }"
               "QPushButton:hover { color: palette(text); }")
        .arg(fg);
}

QString coloredStatusLabelStyle(const QString& color)
{
    return QStringLiteral("color: %1; font-size: 13px; font-weight: 600;").arg(color);
}

QString statusDotStyle(const QString& color)
{
    return QStringLiteral("color: %1; font-size: 10px;").arg(color);
}

QString logTableStyle()
{
    return QStringLiteral(
        "QTableWidget {"
        "  background: palette(base);"
        "  gridline-color: palette(midlight);"
        "  font-size: 13px;"
        "  color: palette(text);"
        "  border: none;"
        "}"
        "QTableWidget::item { padding: 6px 8px; }"
        "QHeaderView::section {"
        "  background: palette(alternate-base);"
        "  color: palette(placeholder-text);"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  padding: 8px 10px;"
        "  border: none;"
        "  border-bottom: 1px solid palette(mid);"
        "  border-right: 1px solid palette(midlight);"
        "}");
}

QString pillButtonStyle(const QString& bg, const QString& fg, bool bold)
{
    return QStringLiteral(
               "QPushButton {"
               "  border: none;"
               "  border-radius: 16px;"
               "  padding: 6px 14px;"
               "  font-size: 13px;"
               "  background: %1;"
               "  color: %2;"
               "  %3"
               "}")
        .arg(bg, fg, bold ? QStringLiteral("font-weight: 600;") : QString());
}

QString neutralPillStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  border: none;"
        "  border-radius: 16px;"
        "  padding: 6px 14px;"
        "  font-size: 13px;"
        "  background: palette(midlight);"
        "  color: palette(text);"
        "}"
        "QPushButton:hover { background: palette(mid); }");
}

} // namespace GUIStyles

