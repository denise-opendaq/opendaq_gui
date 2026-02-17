#pragma once

#include <QObject>
#include <QWidget>
#include <QSplitter>
#include <QList>
#include <QString>
#include <QPoint>
#include "DetachableTabWidget.h"
#include "DropOverlay.h"

// Forward declarations
class DetachedWindow;
class BaseTreeElement;

enum class LayoutZone
{
    Default,    // Open in default location (right side)
    Left,       // Open in left side
    Right,      // Open in right side
    Top,        // Open in top
    Bottom      // Open in bottom
};

class LayoutManager : public QObject
{
    Q_OBJECT

public:
    explicit LayoutManager(QSplitter* contentSplitter, QObject* parent = nullptr);
    ~LayoutManager() override;

    // Tab management
    // relativeToTabName: when non-empty and zone != Default, place new tab relative to the panel that contains this tab title
    DetachableTabWidget* addTab(QWidget* widget, const QString& title, LayoutZone zone = LayoutZone::Default,
                                const QString& relativeToTabName = QString());
    void removeTab(QWidget* widget);
    bool isTabOpen(const QString& tabName) const;
    void clear();  // Clear all unpinned tabs (but keep detached windows open)
    
    // Pin management
    void setTabPinned(QWidget* widget, bool pinned);
    bool isTabPinned(QWidget* widget) const;

    // Tab group management
    DetachableTabWidget* createTabGroup();
    void registerTabGroup(DetachableTabWidget* tw);
    void removeTabGroup(DetachableTabWidget* tw);
    void cleanupIfEmpty(DetachableTabWidget* tw);

    // Detached windows management
    QList<DetachedWindow*> getDetachedWindows() const { return detachedWindows; }
    void closeAllDetachedWindows();

    // Drop overlay management
    DropOverlay* getDropOverlay() const { return dropOverlay; }
    void setDropOverlayTarget(QWidget* target);
    void showDropOverlay();
    void hideDropOverlay();

    // VSCode-like splitting (target = panel to split around; can be tab widget or root content widget)
    void splitAround(QWidget* target, Qt::Orientation orientation, bool before,
                     QWidget* widget, const QString& title);

    // Getters
    QList<DetachableTabWidget*> getTabWidgets() const { return tabWidgets; }
    QList<DetachableTabWidget*> getAllTabWidgets() const; // Includes mainWidget and tabWidgets
    DetachableTabWidget* getDefaultTabWidget() const;
    QSplitter* getContentSplitter() const { return contentSplitter; }

    // Helper methods for drag-drop
    QRect contentGlobalRect() const;
    bool isOverAnyTabBar(const QPoint& globalPos) const;
    DetachableTabWidget* tabGroupAtGlobalPos(const QPoint& globalPos) const;

    // Reset layout
    void restoreDefaultLayout();

    // Open tab for selected element
    void openTabForElement(BaseTreeElement* element, const QString& tabName);
    
    // Menu management
    void setAvailableTabsMenu(QMenu* menu);
    void updateAvailableTabsMenu(BaseTreeElement* element);
    void openTab(const QString& tabName);
    void clearAvailableTabsMenu();

    // Event filter for drag & drop
    bool eventFilter(QObject* obj, QEvent* event) override;

Q_SIGNALS:
    void tabClosedForMenu();
    void detachedWindowClosedForMenu();
    void layoutReset();

public Q_SLOTS:
    void onResetLayout();

private Q_SLOTS:
    void onTabDetached(QWidget* widget, const QString& title, const QPoint& globalPos);
    void onTabSplitRequested(int index, Qt::Orientation orientation, DropZone zone);
    void onTabMoveCompleted(DetachableTabWidget* sourceWidget);
    void onDetachedWindowClosed(QWidget* contentWidget, const QString& title);
    void onTabCloseRequested(int index);
    void onTabPinToggled(QWidget* widget, bool pinned);

private:
    void collapseSplitterIfNeeded(QSplitter* splitter);
    void collapseEmptySplitters(QSplitter* splitter);
    void clearSplitterRecursively(QSplitter* splitter);

    // Ensure main tab widget exists (always exists)
    void ensureMainWidget();

    // Helper methods
    DetachableTabWidget* findTabWidgetContaining(QWidget* widget) const;
    DetachableTabWidget* findTabWidgetWithTab(const QString& tabTitle) const;
    void removeUnpinnedTabsFromWidget(DetachableTabWidget* tabWidget);

    // Drag & drop helper methods
    static bool isTabMimeType(const QMimeData* mime);
    bool handleDragEnter(QDragEnterEvent* event);
    bool handleDragMove(QDragMoveEvent* event);
    bool handleDrop(QDropEvent* event);
    void updateDropOverlay(const QPoint& globalPos);
    bool shouldHandleDragEvent(const QPoint& globalPos) const;

private:
    QSplitter* contentSplitter = nullptr;

    // Tab widgets
    DetachableTabWidget* mainWidget = nullptr;   // Main tab widget (always exists)
    QList<DetachableTabWidget*> tabWidgets;

    // Detached windows
    QList<DetachedWindow*> detachedWindows;

    // Drop overlay
    DropOverlay* dropOverlay = nullptr;

    // Menu for available tabs
    QMenu* availableTabsMenu = nullptr;
    BaseTreeElement* currentSelectedElement = nullptr;  // Public for access from MainWindow
};
