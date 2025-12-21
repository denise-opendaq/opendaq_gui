#pragma once
#include <QIcon>
#include <QString>
#include <QMap>
#include <QApplication>
#include <QPalette>

class IconProvider
{
public:
    static IconProvider& instance();

    // Get icon by name (without extension)
    // For dark theme: uses light_<name>.png (light icons)
    // For light theme: uses <name>.png (dark icons)
    QIcon icon(const QString& iconName) const;

    // Check if icon exists
    bool hasIcon(const QString& iconName) const;

    // Clear cache (useful when theme changes)
    void clearCache();

private:
    IconProvider() = default;
    ~IconProvider() = default;

    IconProvider(const IconProvider&) = delete;
    IconProvider& operator=(const IconProvider&) = delete;

    // Check if application is using dark theme
    bool isDarkTheme() const;

    mutable QMap<QString, QIcon> iconCache;
};
