#pragma once
#include <QIcon>
#include <QString>
#include <QMap>

class IconProvider
{
public:
    static IconProvider& instance();

    // Get icon by name (without extension)
    QIcon icon(const QString& iconName) const;

    // Check if icon exists
    bool hasIcon(const QString& iconName) const;

private:
    IconProvider() = default;
    ~IconProvider() = default;

    IconProvider(const IconProvider&) = delete;
    IconProvider& operator=(const IconProvider&) = delete;

    mutable QMap<QString, QIcon> iconCache;
};
