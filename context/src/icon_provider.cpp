#include "context/icon_provider.h"
#include "context/AppContext.h"
#include <QDebug>
#include <QApplication>
#include <QPalette>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

IconProvider& IconProvider::instance()
{
    static IconProvider s_instance;
    return s_instance;
}

bool IconProvider::isDarkTheme() const
{
    if (!qApp)
    {
        return false;
    }
    
    QPalette palette = qApp->palette();
    QColor bgColor = palette.color(QPalette::Window);
    
    // Check if background is dark (luminance < 128)
    int luminance = (bgColor.red() * 299 + bgColor.green() * 587 + bgColor.blue() * 114) / 1000;
    return luminance < 128;
}

QIcon IconProvider::icon(const QString& iconName) const
{
    // Check if we're using dark theme
    bool darkTheme = isDarkTheme();
    
    // Create cache key that includes theme info to avoid conflicts
    QString cacheKey = darkTheme ? QString("dark_%1").arg(iconName) : QString("light_theme_%1").arg(iconName);
    
    if (iconCache.contains(cacheKey))
    {
        return iconCache[cacheKey];
    }

    QString path;
    QIcon loadedIcon;
    
    if (darkTheme)
    {
        // For dark theme: use light icons (light_<name>.png)
        path = QString(":/icons/light_%1.png").arg(iconName);
        loadedIcon = QIcon(path);
        
        // If light version not found, fall back to regular icon
        if (loadedIcon.availableSizes().size() == 0)
        {
            path = QString(":/icons/%1.png").arg(iconName);
            loadedIcon = QIcon(path);
        }
    }
    else
    {
        // For light theme: use dark icons (regular <name>.png)
        path = QString(":/icons/%1.png").arg(iconName);
        loadedIcon = QIcon(path);
    }

    // Check if icon has available sizes (more reliable than isNull())
    if (loadedIcon.availableSizes().size() > 0)
    {
        iconCache[cacheKey] = loadedIcon;
        return loadedIcon;
    }

    // Return empty icon if not found
    const auto loggerComponent = AppContext::getLoggerComponent();
    LOG_W("IconProvider: Failed to load icon from path: {}", path.toStdString());
    return QIcon();
}

bool IconProvider::hasIcon(const QString& iconName) const
{
    QString path = QString(":/icons/%1.png").arg(iconName);
    return QIcon(path).availableSizes().size() > 0;
}

void IconProvider::clearCache()
{
    iconCache.clear();
}

