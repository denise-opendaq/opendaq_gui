#pragma once
#include <QIcon>
#include <QString>
#include <QMap>

class IconProvider
{
public:
    static IconProvider& instance()
    {
        static IconProvider s_instance;
        return s_instance;
    }

    // Get icon by name (without extension)
    QIcon icon(const QString& iconName) const
    {
        if (iconCache.contains(iconName))
        {
            return iconCache[iconName];
        }

        // Try to load from resources
        QString path = QString(":/icons/%1.png").arg(iconName);
        QIcon loadedIcon(path);

        if (!loadedIcon.isNull())
        {
            iconCache[iconName] = loadedIcon;
            return loadedIcon;
        }

        // Return empty icon if not found
        return QIcon();
    }

    // Check if icon exists
    bool hasIcon(const QString& iconName) const
    {
        QString path = QString(":/icons/%1.png").arg(iconName);
        return QIcon(path).availableSizes().size() > 0;
    }

private:
    IconProvider() = default;
    ~IconProvider() = default;

    IconProvider(const IconProvider&) = delete;
    IconProvider& operator=(const IconProvider&) = delete;

    mutable QMap<QString, QIcon> iconCache;
};
