#pragma once

#include <QColor>
#include <QObject>
#include <QString>

#include <functional>

class QGuiApplication;
class QPalette;
class QWidget;

// Runtime theme colors derived from the active QPalette.
//
// QSS palette() tokens adapt automatically, but imperative painting
// (QTreeWidgetItem backgrounds, delegates, charts) needs resolved QColor
// values. Theme rebuilds those colors when the system palette changes.
class Theme : public QObject
{
    Q_OBJECT

public:
    static Theme& instance();

    // Listen for QEvent::ApplicationPaletteChange and perform an initial rebuild.
    // Call once after QApplication construction.
    static void install(QGuiApplication* app);

    // Read at paint time — always returns current values, like palette().color(role).
    static QColor subtle() { return instance().m_subtle; }
    static QColor border() { return instance().m_border; }
    static QColor midlight() { return instance().m_midlight; }

    // Apply sidebar panel styling. Background follows palette(alternate-base) automatically.
    static void styleSidebarPanel(QWidget* widget, const QString& objectName);

    // Keep @p widget stylesheet in sync with palette changes.
    static void bindStyle(QWidget* widget, const std::function<QString()>& styleFor);

Q_SIGNALS:
    void changed();

private:
    Theme() = default;

    void rebuild(const QPalette& pal);
    bool eventFilter(QObject* watched, QEvent* event) override;

    QColor m_subtle;
    QColor m_border;
    QColor m_midlight;
};
