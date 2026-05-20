#include "context/gui_styles.h"
#include "context/gui_constants.h"
#include "context/theme.h"
#include <QPalette>

namespace GUIStyles
{

QString midlightColor()
{
    return Theme::midlight().name(QColor::HexRgb);
}

QString borderColor()
{
    return Theme::border().name(QColor::HexRgb);
}

QString hairlineHorizontalStyle()
{
    return QStringLiteral("background: %1; border: none; max-height: 1px;").arg(borderColor());
}

QString hairlineVerticalStyle()
{
    return QStringLiteral("background: %1; border: none; max-width: 1px;").arg(borderColor());
}

QString cardStyle(const QString& objectName, const QString& qssClass, int radius)
{
    return QStringLiteral(
               "%1#%2 {"
               "  border: 1px solid %3;"
               "  border-radius: %4px;"
               "}")
        .arg(qssClass, objectName, borderColor())
        .arg(radius);
}

QString sidebarPanelStyle(const QString& objectName)
{
    return QStringLiteral(
               "QWidget#%1 {"
               "  background: palette(alternate-base);"
               "  border: 1px solid %2;"
               "}")
        .arg(objectName, borderColor());
}

QString sidebarPanelStyle(const QString& objectName, const QColor& background)
{
    return QStringLiteral(
               "QWidget#%1 {"
               "  background-color: %2;"
               "  border: none;"
               "}")
        .arg(objectName, background.name(QColor::HexRgb));
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

QString underlineTabBarAccentStyle()
{
    const QString accent = QString::fromUtf8(GUIConstants::Colors::ACCENT);
    return QStringLiteral(
               "QTabWidget::pane { border: none; }"
               "QTabBar { border: none; }"
               "QTabBar::tab {"
               "  padding: 10px 18px;"
               "  color: palette(placeholder-text);"
               "  font-size: 13px;"
               "  border: none;"
               "  border-bottom: 2px solid transparent;"
               "}"
               "QTabBar::tab:hover {"
               "  border-bottom: 2px solid %1;"
               "}"
               "QTabBar::tab:selected {"
               "  color: %1;"
               "  border-bottom: 2px solid %1;"
               "}")
        .arg(accent);
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
        "  padding: 5px 8px;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
        "QTreeWidget QHeaderView {"
        "  background-color: transparent;"
        "  border: none;"
        "}"
        "QTreeWidget QHeaderView::section {"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 8px 8px 4px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  letter-spacing: 0.5px;"
        "  color: palette(placeholder-text);"
        "}"
        "QScrollBar:vertical {"
        "  width: 6px;"
        "  background: transparent;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: palette(mid);"
        "  border-radius: 3px;"
        "  min-height: 24px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}");
}

QString searchLineEditStyle()
{
    return QStringLiteral(
        "QLineEdit {"
        "  background: transparent;"
        "  border: 1px solid %1;"
        "  border-radius: 10px;"
        "  padding: 8px 10px;"
        "  font-size: 13px;"
        "  color: palette(text);"
        "}"
        "QLineEdit:focus {"
        "  border: 2px solid palette(highlight);"
        "}")
        .arg(borderColor());
}

QString roundedComboBoxStyle()
{
    return QStringLiteral(
        "QComboBox {"
        "  background: transparent;"
        "  border: 1px solid %1;"
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
        "}")
        .arg(borderColor());
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
               "  border: 1.5px solid %1;"
               "  border-radius: 12px;"
               "}"
               "QPushButton:disabled {"
               "  border: 1.5px solid %4;"
               "  border-radius: 10px;"
               "  padding: 10px 10px;"
               "  color: palette(placeholder-text);"
               "}")
        .arg(GUIConstants::Colors::ACCENT,
             GUIConstants::Colors::HIGHLIGHT,
             borderColor());
}

QString iconToolButtonStyle(bool withCheckedState)
{
    QString s = QStringLiteral(
        "QToolButton {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "}").arg(borderColor());

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
        "  border: 1px solid %1;"
        "  border-radius: 10px;"
        "  padding: 6px;"
        "}"
        "QToolButton:disabled { color: palette(placeholder-text); }")
        .arg(borderColor());
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
        "  border-bottom: 1px solid %1;"
        "  border-right: 1px solid palette(midlight);"
        "}")
        .arg(borderColor());
}

QString pillButtonStyle(const QString& fg, bool bold)
{
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

