#include "component/icon_provider.h"

IconProvider& IconProvider::instance()
{
    static IconProvider s_instance;
    return s_instance;
}

QIcon IconProvider::icon(const QString& iconName) const
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

bool IconProvider::hasIcon(const QString& iconName) const
{
    QString path = QString(":/icons/%1.png").arg(iconName);
    return QIcon(path).availableSizes().size() > 0;
}

