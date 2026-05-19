#include "context/gui_styles.h"
#include "context/gui_constants.h"

namespace GUIStyles
{

QString cardStyle(const QString& objectName, const QString& qssClass, int radius)
{
    return QStringLiteral(
               "%1#%2 {"
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
               "  border: none;"
               "}")
        .arg(objectName);
}

QString underlineTabBarStyle()
{
    return QStringLiteral(
        "QTabWidget::pane { border: none; }"
        "QTabBar { border: none; }"
        "QTabBar::tab {"
        "  padding: 10px 18px;"
        "  color: palette(placeholder-text);"
        "  font-size: 13px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "}");
}

QString sidebarTreeStyle()
{
    return QStringLiteral(
        "QTreeWidget {"
        "  background: transparent;"
        "  border: none;"
        "  font-size: 13px;"
        "  outline: 0;"
        "  color: palette(text);"
        "  selection-background-color: transparent;"
        "  selection-color: palette(text);"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  border-radius: 8px;"
        "}"
        "QHeaderView::section {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 6px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  color: palette(placeholder-text);"
        "}");
}

QString searchLineEditStyle()
{
    return QStringLiteral(
        "QLineEdit {"
        "  background: transparent;"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  padding: 8px 10px;"
        "  font-size: 13px;"
        "  color: palette(text);"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid palette(highlight);"
        "}");
}

QString roundedComboBoxStyle()
{
    return QStringLiteral(
        "QComboBox {"
        "  background: transparent;"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  padding: 9px 12px;"
        "  font-size: 13px;"
        "  color: palette(text);"
        "}"
        "QComboBox:focus {"
        "  border: 1px solid palette(highlight);"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 24px;"
        "}");
}

QString primaryButtonStyle()
{
    return QStringLiteral(
               "QPushButton {"
               "  border: 1.5px solid %1;"
               "  border-radius: 10px;"
               "  padding: 10px 10px;"
               "  color: palette(highlighted-text);"
               "}"
               "QPushButton:hover {"
               "  border: 1.5px solid %2;"
               "  border-radius: 12px;"
               "}"
               "QPushButton:pressed {"
               "  border: 1.5px solid %3;"
               "  border-radius: 12px;"
               "}"
               "QPushButton:disabled {"
               "  border: 1.5px solid palette(mid);"
               "  border-radius: 10px;"
               "  padding: 10px 10px;"
               "  color: palette(placeholder-text);"
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
        "}");

    if (withCheckedState)
    {
        s += QStringLiteral(
            "QToolButton:checked {"
            "  border-color: %1;"
            "}").arg(GUIConstants::Colors::WARNING);
    }
    return s;
}

QString roundedToolButtonStyle()
{
    return QStringLiteral(
        "QToolButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "  padding: 6px;"
        "}"
        "QToolButton:disabled { color: palette(placeholder-text); }");
}

QString linkButtonStyleFor(const QString& accent)
{
    return QStringLiteral(
               "QPushButton { color: %1; font-size: 13px; padding: 0 4px; border: none; }"
               "QPushButton:hover { text-decoration: underline; }")
        .arg(accent);
}

QString transparentInlineLineEditStyle()
{
    return QStringLiteral(
        "QLineEdit { color: palette(placeholder-text); font-size: 13px;"
        " padding: 0; margin: 0; border: none; }");
}

QString transparentInlinePlainTextEditStyle()
{
    return QStringLiteral(
        "QPlainTextEdit { color: palette(placeholder-text); font-size: 13px;"
        " padding: 0; margin: 0; border: none; }");
}

QString tagBubbleStyle(const QString& bg)
{
    return QStringLiteral("QWidget { background: %1; border-radius: 10px; }").arg(bg);
}

QString tagBubbleLabelStyle(const QString& fg)
{
    return QStringLiteral(
               "color: %1; font-size: 12px; font-weight: 600;"
               " border: none;")
        .arg(fg);
}

QString tagBubbleRemoveButtonStyle(const QString& fg)
{
    return QStringLiteral(
               "QPushButton { color: %1; border: none;"
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
        "  gridline-color: palette(midlight);"
        "  font-size: 13px;"
        "  color: palette(text);"
        "  border: none;"
        "}"
        "QTableWidget::item { padding: 6px 8px; }"
        "QHeaderView::section {"
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
    Q_UNUSED(bg);
    return QStringLiteral(
               "QPushButton {"
               "  border: none;"
               "  border-radius: 16px;"
               "  padding: 6px 14px;"
               "  font-size: 13px;"
               "  color: %1;"
               "  %2"
               "}")
        .arg(fg, bold ? QStringLiteral("font-weight: 600;") : QString());
}

QString neutralPillStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  border: none;"
        "  border-radius: 16px;"
        "  padding: 6px 14px;"
        "  font-size: 13px;"
        "  color: palette(text);"
        "}");
}

} // namespace GUIStyles

