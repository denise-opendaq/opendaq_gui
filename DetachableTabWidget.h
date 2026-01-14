#pragma once

#include <QTabWidget>
#include <QTabBar>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QDrag>
#include <QIcon>
#include <QVariant>
#include <QPoint>
#include <QToolButton>
#include <QMap>

#include "DropOverlay.h"

// VSCode-like tab drag & drop (in-process):
//  - Dragging a tab starts a QDrag with a custom mime type.
//  - Dropping onto another tab bar moves/reorders tabs between groups.
//  - Dropping elsewhere is handled by MainWindow (split overlay, etc.).
//
// NOTE: This is in-process drag only (we serialize a pointer in mime data).

class DetachableTabWidget;

class DetachableTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit DetachableTabBar(QWidget* parent = nullptr);

Q_SIGNALS:
    void detachRequested(int index, const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QPoint dragStartPos;
    int dragIndex = -1;
    bool dragArmed = false;
};

class DetachableTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    struct TabInfo
    {
        QWidget* page = nullptr;
        QString title;
        QIcon icon;
        QString toolTip;
        QString whatsThis;
        QVariant data;
    };

    static constexpr const char* TabMimeType = "application/x-opendaq-tab";

    explicit DetachableTabWidget(QWidget* parent = nullptr);

    void setEnableDetach(bool enable);
    void setEnableSplit(bool enable);

    TabInfo takeTabInfo(int index);
    int insertTabInfo(int index, TabInfo&& info);

    static bool decodeTabMime(const QMimeData* mime, DetachableTabWidget*& outSource, int& outIndex);
    
    // Pin management
    void setTabPinned(int index, bool pinned);
    bool isTabPinned(int index) const;
    void updatePinButton(int index);

Q_SIGNALS:
    // Detach tab to a floating window. (Closing the window destroys the content.)
    void tabDetached(QWidget* widget, const QString& title, const QPoint& globalPos);

    // Existing API (optional, used by context menu actions if you add them later)
    void tabSplitRequested(int index, Qt::Orientation orientation, DropZone zone);

    // Emitted after a successful move/reorder between tab widgets (via drop on tab bar)
    void tabMoveCompleted(DetachableTabWidget* sourceWidget);
    
    // Pin/unpin tab
    void tabPinToggled(QWidget* widget, bool pinned);

private Q_SLOTS:
    void onDetachRequested(int index, const QPoint& globalPos);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;

private:
    bool isPointOnTabBar(const QPoint& widgetPos) const;
    int dropInsertIndexFromPos(const QPoint& widgetPos) const;
    QToolButton* createPinButton(int index);
    void onPinButtonClicked(int index);

private:
    bool detachEnabled = true;
    bool splitEnabled = true;
    QMap<int, QToolButton*> pinButtons; // Map tab index to pin button
};
