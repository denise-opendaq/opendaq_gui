#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QSplitter>
#include <QList>
#include <QStringList>
#include <QEvent>

#include "DetachableTabWidget.h"
#include "DropOverlay.h"

class DetachedWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();

    // Helper methods for creating content widgets
    QWidget* createPropertiesWidget();
    QWidget* createDeviceInfoWidget();
    QWidget* createChannelsWidget();
    QWidget* createSignalsWidget();

    // Docking/splitting helpers (VSCode-like)
    DetachableTabWidget* createTabGroup();
    void registerTabGroup(DetachableTabWidget* tw);
    QRect contentGlobalRect() const;
    bool isOverAnyTabBar(const QPoint& globalPos) const;
    DetachableTabWidget* tabGroupAtGlobalPos(const QPoint& globalPos) const;

    void splitAround(DetachableTabWidget* target, Qt::Orientation orientation, bool before,
                     QWidget* widget, const QString& title);

    void cleanupIfEmpty(DetachableTabWidget* tw);
    void removeTabGroup(DetachableTabWidget* tw);
    void collapseSplitterIfNeeded(QSplitter* splitter);
    void clearSplitterRecursively(QSplitter* splitter);

    // Tab management
    void trackClosedTab(const QString& title);
    void restoreDefaultLayout();

private Q_SLOTS:
    void onViewSelectionChanged(int index);
    void onTreeItemSelected();
    void onShowHiddenComponentsToggled(bool checked);

    // Tab management
    void onTabDetached(QWidget* widget, const QString& title, const QPoint& globalPos);
    void onTabSplitRequested(int index, Qt::Orientation orientation, DropZone zone);
    void onTabMoveCompleted(DetachableTabWidget* sourceWidget);
    void onDetachedWindowClosed(QWidget* contentWidget, const QString& title);
    void onTabCloseRequested(int index);

    // Menu actions
    void onRestoreTab(const QString& tabName);
    void onResetLayout();

private:
    // Main splitters
    QSplitter* mainSplitter = nullptr;      // Horizontal: left + right
    QSplitter* verticalSplitter = nullptr;  // Vertical: tabs + log
    QSplitter* contentSplitter = nullptr;   // Root for split views in content area

    // Left panel
    QComboBox* viewSelector = nullptr;
    QTreeWidget* componentTree = nullptr;

    // Right panel - can have multiple tab widgets for split view
    DetachableTabWidget* tabWidget = nullptr;
    QList<DetachableTabWidget*> tabWidgets; // Track all tab widgets

    // Bottom log panel
    QTextEdit* logTextEdit = nullptr;

    // Detached windows
    QList<DetachedWindow*> detachedWindows;

    // Drop overlay for visual feedback during drag
    DropOverlay* dropOverlay = nullptr;

    // Closed tabs tracking
    QStringList closedTabs;

    // Menu
    QAction* showHiddenAction = nullptr;
    QMenu* restoreTabMenu = nullptr;
};
