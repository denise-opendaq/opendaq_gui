#include "context/theme.h"

#include "context/icon_provider.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>

namespace
{
QPalette makeDarkPalette()
{
    // Palette tuned for Qt Widgets + QtCharts (uses Base/Text/Highlight heavily).
    QPalette p;
    p.setColor(QPalette::Window, QColor(0x0B, 0x12, 0x20));            // #0b1220
    p.setColor(QPalette::WindowText, QColor(0xE5, 0xE7, 0xEB));        // #e5e7eb
    p.setColor(QPalette::Base, QColor(0x0F, 0x17, 0x2A));              // #0f172a
    p.setColor(QPalette::AlternateBase, QColor(0x11, 0x1C, 0x33));     // slightly lighter
    p.setColor(QPalette::ToolTipBase, QColor(0x11, 0x1C, 0x33));
    p.setColor(QPalette::ToolTipText, QColor(0xE5, 0xE7, 0xEB));
    p.setColor(QPalette::Text, QColor(0xE5, 0xE7, 0xEB));
    p.setColor(QPalette::Button, QColor(0x11, 0x1C, 0x33));
    p.setColor(QPalette::ButtonText, QColor(0xE5, 0xE7, 0xEB));
    p.setColor(QPalette::BrightText, QColor(0xFF, 0xFF, 0xFF));

    // Accent
    p.setColor(QPalette::Highlight, QColor(0x3B, 0x82, 0xF6));         // #3b82f6
    p.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));

    // Borders / disabled
    p.setColor(QPalette::Mid, QColor(0x2A, 0x36, 0x4F));               // #2a364f
    p.setColor(QPalette::Shadow, QColor(0x00, 0x00, 0x00));

    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x9C, 0xA3, 0xAF));        // #9ca3af
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x9C, 0xA3, 0xAF));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x9C, 0xA3, 0xAF));

    return p;
}

QString loadQssResource(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}
} // namespace

namespace Theme
{
void applyDark(QApplication& app)
{
    // Use Fusion for consistent cross-platform rendering.
    if (auto* fusion = QStyleFactory::create("Fusion"))
        app.setStyle(fusion);

    app.setPalette(makeDarkPalette());

    const QString qss = loadQssResource(":/theme/dark.qss");
    if (!qss.isEmpty())
        app.setStyleSheet(qss);

    // Icons depend on palette luminance; ensure cache invalidated after palette update.
    IconProvider::instance().clearCache();
}
} // namespace Theme

