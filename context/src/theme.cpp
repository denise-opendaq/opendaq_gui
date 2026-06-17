#include "context/theme.h"

#include "context/icon_provider.h"
#include "context/gui_styles.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QWidget>

Theme& Theme::instance()
{
    static Theme t;
    return t;
}

void Theme::install(QGuiApplication* app)
{
    Theme& t = instance();

    app->installEventFilter(&t);
    QObject::connect(&t, &Theme::changed, [] {
        IconProvider::instance().clearCache();
    });

    t.rebuild(app->palette());
}

void Theme::styleSidebarPanel(QWidget* widget, const QString& objectName)
{
    if (!widget)
        return;

    widget->setObjectName(objectName);
    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setStyleSheet(GUIStyles::sidebarPanelStyle(objectName));
}

void Theme::bindStyle(QWidget* widget, const std::function<QString()>& styleFor)
{
    if (!widget)
        return;

    const auto apply = [widget, styleFor] {
        widget->setStyleSheet(styleFor());
    };
    apply();
    QObject::connect(&instance(), &Theme::changed, widget, apply);
}

bool Theme::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ApplicationPaletteChange && qApp)
        rebuild(qApp->palette());

    return QObject::eventFilter(watched, event);
}

void Theme::rebuild(const QPalette& pal)
{
    m_subtle = pal.color(QPalette::Base).darker(108);
    m_border = pal.color(QPalette::Base).darker(115);
    m_midlight = pal.color(QPalette::Base).darker(104);

    // Inject subtle into AlternateBase so QSS palette(alternate-base) tokens
    // resolve to our derived color without any explicit callbacks.
    // The comparison breaks the re-entry loop: setPalette posts another
    // ApplicationPaletteChange, but the second rebuild finds AlternateBase
    // already matching m_subtle and skips to emit changed().
    if (pal.color(QPalette::AlternateBase) != m_subtle) {
        QPalette extended = pal;
        extended.setColor(QPalette::AlternateBase, m_subtle);
        QApplication::setPalette(extended);
        return;
    }

    Q_EMIT changed();
}
