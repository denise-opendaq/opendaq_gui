#include "LayoutManager.h"
#include "DetachedWindow.h"
#include "context/AppContext.h"
#include "component/base_tree_element.h"
#include <QApplication>
#include <QTabBar>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCursor>
#include <QMenu>
#include <QAction>
#include <opendaq/custom_log.h>
#include <opendaq/logger_component_ptr.h>

LayoutManager::LayoutManager(QSplitter* contentSplitter, QObject* parent)
    : QObject(parent)
    , contentSplitter(contentSplitter)
{
    if (!contentSplitter)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_E("LayoutManager: contentSplitter is null");
        return;
    }

    // Create drop overlay
    dropOverlay = new DropOverlay(qobject_cast<QWidget*>(parent));
    dropOverlay->setTargetWidget(contentSplitter);
    dropOverlay->hide();

    // Create main tab widget
    mainWidget = createTabGroup();
    tabWidgets.removeOne(mainWidget); // Remove from tabWidgets as mainWidget is special
    contentSplitter->addWidget(mainWidget);

    // Install event filter on qApp to handle drag & drop events
    qApp->installEventFilter(this);
}

LayoutManager::~LayoutManager()
{
    closeAllDetachedWindows();
}

DetachableTabWidget* LayoutManager::createTabGroup()
{
    auto* tw = new DetachableTabWidget();
    tw->setTabsClosable(true);
    registerTabGroup(tw);
    tabWidgets.append(tw);
    return tw;
}

void LayoutManager::registerTabGroup(DetachableTabWidget* tw)
{
    if (!tw)
        return;

    connect(tw, &DetachableTabWidget::tabDetached,
            this, &LayoutManager::onTabDetached);
    connect(tw, &DetachableTabWidget::tabSplitRequested,
            this, &LayoutManager::onTabSplitRequested);
    connect(tw, &DetachableTabWidget::tabMoveCompleted,
            this, &LayoutManager::onTabMoveCompleted);
    connect(tw, &QTabWidget::tabCloseRequested,
            this, &LayoutManager::onTabCloseRequested);
    connect(tw, &DetachableTabWidget::tabPinToggled,
            this, &LayoutManager::onTabPinToggled);
}

DetachableTabWidget* LayoutManager::addTab(QWidget* widget, const QString& title, LayoutZone zone)
{
    if (!widget)
        return nullptr;

    // Ensure mainWidget always exists
    ensureMainWidget();

    DetachableTabWidget* targetTabWidget = nullptr;

    // Handle non-default zones with splitAround
    if (zone != LayoutZone::Default)
    {
        Qt::Orientation orientation = (zone == LayoutZone::Left || zone == LayoutZone::Right) 
                                     ? Qt::Horizontal 
                                     : Qt::Vertical;
        bool before = (zone == LayoutZone::Left || zone == LayoutZone::Top);
        
        splitAround(mainWidget, orientation, before, widget, title);
        
        // Find the newly created tab widget
        DetachableTabWidget* result = findTabWidgetContaining(widget);
        return result ? result : mainWidget;
    }

    // Default zone: add to mainWidget
    targetTabWidget = mainWidget;

    // If widget is not already in targetTabWidget, add it
    int tabIndex = targetTabWidget->indexOf(widget);
    if (tabIndex < 0)
    {
        tabIndex = targetTabWidget->addTab(widget, title);
        targetTabWidget->setCurrentIndex(tabIndex);
        
        // Create pin button for the new tab
        targetTabWidget->updatePinButton(tabIndex);
    }
    else
    {
        targetTabWidget->setCurrentIndex(tabIndex);
    }

    const auto loggerComponent = AppContext::LoggerComponent();
    const char* zoneNames[] = {"main", "left", "right", "top", "bottom"};
    LOG_I("Tab '{}' added to {} zone", title.toStdString(), zoneNames[static_cast<int>(zone)]);

    return targetTabWidget;
}

DetachableTabWidget* LayoutManager::findTabWidgetContaining(QWidget* widget) const
{
    if (!widget)
        return nullptr;

    // Check mainWidget first
    if (mainWidget && mainWidget->indexOf(widget) >= 0)
        return mainWidget;

    // Check all tab widgets
    for (auto* tw : tabWidgets)
    {
        if (tw && tw->indexOf(widget) >= 0)
            return tw;
    }

    return nullptr;
}

QList<DetachableTabWidget*> LayoutManager::getAllTabWidgets() const
{
    QList<DetachableTabWidget*> all;
    if (mainWidget)
        all.append(mainWidget);
    all.append(tabWidgets);
    return all;
}

void LayoutManager::removeTab(QWidget* widget)
{
    if (!widget)
        return;

    DetachableTabWidget* tabWidget = findTabWidgetContaining(widget);
    if (tabWidget)
    {
        int index = tabWidget->indexOf(widget);
        if (index != -1)
        {
            tabWidget->removeTab(index);
            widget->deleteLater();
            cleanupIfEmpty(tabWidget);
        }
    }
}

bool LayoutManager::isTabOpen(const QString& tabName) const
{
    // Check all tab widgets (including mainWidget)
    for (auto* tabWidget : getAllTabWidgets())
    {
        for (int i = 0; i < tabWidget->count(); ++i)
        {
            if (tabWidget->tabText(i) == tabName)
                return true;
        }
    }

    // Check detached windows
    for (auto* window : detachedWindows)
    {
        if (window->windowTitle() == tabName)
            return true;
    }

    return false;
}

void LayoutManager::removeUnpinnedTabsFromWidget(DetachableTabWidget* tabWidget)
{
    if (!tabWidget)
        return;

    for (int i = tabWidget->count() - 1; i >= 0; --i)
    {
        QWidget* tabPage = tabWidget->widget(i);
        if (tabPage && !isTabPinned(tabPage))
        {
            tabWidget->removeTab(i);
            tabPage->deleteLater();
        }
    }
}

void LayoutManager::clear()
{
    // Remove only unpinned tabs from all tab widgets (including mainWidget)
    for (auto* tabWidget : getAllTabWidgets())
        removeUnpinnedTabsFromWidget(tabWidget);
    
    // Clean up empty tab widgets (but keep mainWidget)
    QList<DetachableTabWidget*> widgetsToCheck = tabWidgets;
    for (auto* tabWidget : widgetsToCheck)
    {
        if (tabWidget && tabWidget->count() == 0)
            cleanupIfEmpty(tabWidget);
    }
    
    // Collapse empty splitters recursively
    collapseEmptySplitters(contentSplitter);
    
    // If mainWidget is not empty, move it to tabWidgets and create a new empty mainWidget
    if (mainWidget && mainWidget->count() > 0)
    {
        if (!tabWidgets.contains(mainWidget))
            tabWidgets.append(mainWidget);
        
        QWidget* parent = mainWidget->parentWidget();
        QSplitter* parentSplitter = qobject_cast<QSplitter*>(parent);
        int oldIndex = parentSplitter ? parentSplitter->indexOf(mainWidget) : -1;
        
        DetachableTabWidget* oldMainWidget = mainWidget;
        mainWidget = createTabGroup();
        tabWidgets.removeOne(mainWidget);
        
        if (parentSplitter && oldIndex >= 0)
            parentSplitter->insertWidget(oldIndex, mainWidget);
        else if (parent == contentSplitter)
            contentSplitter->addWidget(mainWidget);
        else
            contentSplitter->addWidget(mainWidget);
    }
    else
    {
        ensureMainWidget();
    }
    
    // Note: detached windows are NOT closed - they remain open as they are intentionally detached
}

void LayoutManager::closeAllDetachedWindows()
{
    for (auto* window : detachedWindows)
    {
        window->close();
        window->deleteLater();
    }
    detachedWindows.clear();
}

DetachableTabWidget* LayoutManager::getDefaultTabWidget() const
{
    return mainWidget;
}

QRect LayoutManager::contentGlobalRect() const
{
    if (!contentSplitter)
        return QRect();

    const QPoint tl = contentSplitter->mapToGlobal(QPoint(0, 0));
    return QRect(tl, contentSplitter->size());
}

bool LayoutManager::isOverAnyTabBar(const QPoint& globalPos) const
{
    QWidget* w = QApplication::widgetAt(globalPos);
    while (w)
    {
        if (qobject_cast<QTabBar*>(w))
            return true;
        w = w->parentWidget();
    }
    return false;
}

DetachableTabWidget* LayoutManager::tabGroupAtGlobalPos(const QPoint& globalPos) const
{
    auto pick = [](const QPoint& pos) -> DetachableTabWidget*
    {
        QWidget* w = QApplication::widgetAt(pos);
        while (w)
        {
            if (auto* tw = qobject_cast<DetachableTabWidget*>(w))
                return tw;
            w = w->parentWidget();
        }
        return nullptr;
    };

    // Fast path
    if (auto* tw = pick(globalPos))
        return tw;

    // Probe nearby points for better UX
    static const QPoint offsets[] =
    {
        {-10, 0}, {10, 0}, {0, -10}, {0, 10},
        {-25, 0}, {25, 0}, {0, -25}, {0, 25}
    };

    for (const QPoint& off : offsets)
    {
        if (auto* tw = pick(globalPos + off))
            return tw;
    }

    return nullptr;
}

void LayoutManager::splitAround(DetachableTabWidget* target, Qt::Orientation orientation, bool before,
                                QWidget* widget, const QString& title)
{
    if (!target || !widget)
        return;

    DetachableTabWidget* newGroup = createTabGroup();
    newGroup->addTab(widget, title);
    newGroup->setCurrentIndex(0);

    QSplitter* parent = qobject_cast<QSplitter*>(target->parentWidget());
    if (!parent)
    {
        // Fallback: just append
        contentSplitter->addWidget(newGroup);
        return;
    }

    if (parent->orientation() == orientation)
    {
        int idx = parent->indexOf(target);
        if (!before)
            idx++;
        parent->insertWidget(idx, newGroup);
        parent->setSizes({1, 1});
        return;
    }

    // Need a nested splitter
    QSplitter* nested = new QSplitter(orientation);
    nested->setChildrenCollapsible(false);
    nested->setAcceptDrops(true);

    const int idx = parent->indexOf(target);
    QWidget* old = parent->replaceWidget(idx, nested);

    if (before)
    {
        nested->addWidget(newGroup);
        nested->addWidget(old);
    }
    else
    {
        nested->addWidget(old);
        nested->addWidget(newGroup);
    }
    nested->setSizes({1, 1});
}

void LayoutManager::cleanupIfEmpty(DetachableTabWidget* tw)
{
    if (!tw)
        return;

    if (tw->count() != 0)
        return;

    // Keep at least one tab group alive
    if (tabWidgets.size() <= 1)
        return;

    removeTabGroup(tw);
}

void LayoutManager::removeTabGroup(DetachableTabWidget* tw)
{
    if (!tw)
        return;

    tabWidgets.removeOne(tw);

    // Update mainWidget pointer
    if (tw == mainWidget)
        mainWidget = nullptr;

    QSplitter* parent = qobject_cast<QSplitter*>(tw->parentWidget());
    if (parent)
        tw->setParent(nullptr);

    tw->deleteLater();

    if (parent)
        collapseSplitterIfNeeded(parent);

    // Ensure mainWidget exists if it was removed
    if (!mainWidget)
        ensureMainWidget();
}

void LayoutManager::collapseSplitterIfNeeded(QSplitter* splitter)
{
    if (!splitter)
        return;

    if (splitter->count() != 1)
        return;

    QWidget* only = splitter->widget(0);
    QSplitter* parent = qobject_cast<QSplitter*>(splitter->parentWidget());

    // Don't delete the root splitter
    if (!parent || splitter == contentSplitter)
        return;

    const int idx = parent->indexOf(splitter);

    only->setParent(nullptr);
    parent->replaceWidget(idx, only);
    splitter->deleteLater();

    collapseSplitterIfNeeded(parent);
}

void LayoutManager::collapseEmptySplitters(QSplitter* splitter)
{
    if (!splitter)
        return;

    // Recursively process all child splitters first
    for (int i = splitter->count() - 1; i >= 0; --i)
    {
        QWidget* child = splitter->widget(i);
        if (auto* childSplitter = qobject_cast<QSplitter*>(child))
            collapseEmptySplitters(childSplitter);
    }

    // Check if this splitter should be collapsed
    // A splitter should be collapsed if:
    // 1. It has only one child, OR
    // 2. All its children are empty tab widgets (count == 0)
    
    if (splitter->count() == 0)
    {
        // Empty splitter - remove it if not root
        QSplitter* parent = qobject_cast<QSplitter*>(splitter->parentWidget());
        if (parent && splitter != contentSplitter)
        {
            int idx = parent->indexOf(splitter);
            if (idx >= 0)
            {
                // Remove empty splitter by replacing with nothing (will collapse)
                splitter->setParent(nullptr);
                splitter->deleteLater();
                collapseSplitterIfNeeded(parent);
            }
        }
        return;
    }

    // Check if all children are empty tab widgets
    bool allEmpty = true;
    int nonEmptyCount = 0;
    for (int i = 0; i < splitter->count(); ++i)
    {
        QWidget* child = splitter->widget(i);
        if (auto* tabWidget = qobject_cast<DetachableTabWidget*>(child))
        {
            if (tabWidget->count() > 0)
            {
                allEmpty = false;
                nonEmptyCount++;
            }
        }
        else if (auto* childSplitter = qobject_cast<QSplitter*>(child))
        {
            // Check if child splitter has any non-empty tab widgets
            bool hasNonEmpty = false;
            QList<QWidget*> widgetsToCheck;
            widgetsToCheck.append(childSplitter);
            while (!widgetsToCheck.isEmpty())
            {
                QWidget* w = widgetsToCheck.takeFirst();
                if (auto* tw = qobject_cast<DetachableTabWidget*>(w))
                {
                    if (tw->count() > 0)
                    {
                        hasNonEmpty = true;
                        break;
                    }
                }
                else if (auto* s = qobject_cast<QSplitter*>(w))
                {
                    for (int j = 0; j < s->count(); ++j)
                        widgetsToCheck.append(s->widget(j));
                }
            }
            if (hasNonEmpty)
            {
                allEmpty = false;
                nonEmptyCount++;
            }
        }
        else
        {
            allEmpty = false;
            nonEmptyCount++;
        }
    }

    // If all children are empty, remove empty tab widgets and collapse splitter
    if (allEmpty)
    {
        // Remove all empty tab widgets (except mainWidget)
        QList<QWidget*> toRemove;
        for (int i = 0; i < splitter->count(); ++i)
        {
            QWidget* child = splitter->widget(i);
            if (auto* tabWidget = qobject_cast<DetachableTabWidget*>(child))
            {
                if (tabWidget->count() == 0 && tabWidget != mainWidget)
                    toRemove.append(tabWidget);
            }
        }
        
        // Remove empty tab widgets
        for (QWidget* widget : toRemove)
        {
            widget->setParent(nullptr);
            if (auto* tabWidget = qobject_cast<DetachableTabWidget*>(widget))
                cleanupIfEmpty(tabWidget);
        }
        
        // After removal, check if splitter should be collapsed
        // Re-check count after removals
        int remainingCount = splitter->count();
        
        if (remainingCount == 0)
        {
            // Empty splitter - remove it if not root
            QSplitter* parent = qobject_cast<QSplitter*>(splitter->parentWidget());
            if (parent && splitter != contentSplitter)
            {
                int idx = parent->indexOf(splitter);
                if (idx >= 0)
                {
                    splitter->setParent(nullptr);
                    splitter->deleteLater();
                    collapseSplitterIfNeeded(parent);
                }
            }
        }
        else if (remainingCount == 1)
        {
            // Only one child left - collapse splitter
            collapseSplitterIfNeeded(splitter);
        }
    }
    else if (nonEmptyCount == 1)
    {
        // Only one non-empty child - collapse splitter
        collapseSplitterIfNeeded(splitter);
    }
}

void LayoutManager::clearSplitterRecursively(QSplitter* splitter)
{
    if (!splitter)
        return;

    while (splitter->count() > 0)
    {
        QWidget* w = splitter->widget(0);
        w->setParent(nullptr);

        if (auto* child = qobject_cast<QSplitter*>(w))
            clearSplitterRecursively(child);

        w->deleteLater();
    }
}

void LayoutManager::restoreDefaultLayout()
{
    // Close all detached windows
    closeAllDetachedWindows();

    // Reset content area to a single tab group
    clearSplitterRecursively(contentSplitter);
    tabWidgets.clear();
    mainWidget = nullptr;

    mainWidget = createTabGroup();
    tabWidgets.removeOne(mainWidget); // Remove from tabWidgets as mainWidget is special
    contentSplitter->setOrientation(Qt::Horizontal);
    contentSplitter->addWidget(mainWidget);

    if (dropOverlay)
    {
        dropOverlay->setTargetWidget(contentSplitter);
        dropOverlay->hide();
    }
}

void LayoutManager::setDropOverlayTarget(QWidget* target)
{
    if (dropOverlay)
        dropOverlay->setTargetWidget(target);
}

void LayoutManager::showDropOverlay()
{
    if (dropOverlay)
        dropOverlay->show();
}

void LayoutManager::hideDropOverlay()
{
    if (dropOverlay && dropOverlay->isVisible())
        dropOverlay->hide();
}

void LayoutManager::ensureMainWidget()
{
    if (mainWidget)
        return; // Already exists

    if (!contentSplitter)
        return; // Cannot create mainWidget without contentSplitter

    // Create mainWidget and add it to contentSplitter
    // Note: createTabGroup() adds it to tabWidgets, but mainWidget should not be in tabWidgets
    mainWidget = createTabGroup();
    tabWidgets.removeOne(mainWidget); // Remove from tabWidgets as mainWidget is special
    contentSplitter->addWidget(mainWidget);
}

void LayoutManager::setTabPinned(QWidget* widget, bool pinned)
{
    if (!widget)
        return;
    
    // Check if state is already set to avoid recursion
    bool currentPinned = isTabPinned(widget);
    if (currentPinned == pinned)
        return; // Already in the desired state
    
    // Store pin state in widget property
    widget->setProperty("tabPinned", pinned);
    
    // Also store in tab data if widget is in a tab widget
    DetachableTabWidget* tabWidget = findTabWidgetContaining(widget);
    if (tabWidget)
    {
        int index = tabWidget->indexOf(widget);
        if (index >= 0)
        {
            tabWidget->setTabPinned(index, pinned);
            // Note: updatePinButton is called inside setTabPinned, so we don't need to call it again
        }
    }
}

bool LayoutManager::isTabPinned(QWidget* widget) const
{
    if (!widget)
        return false;
    
    // Check widget property first
    QVariant pinnedProperty = widget->property("tabPinned");
    if (pinnedProperty.isValid())
        return pinnedProperty.toBool();
    
    // Check tab data in the widget that contains this tab
    DetachableTabWidget* tabWidget = findTabWidgetContaining(widget);
    if (tabWidget)
    {
        int index = tabWidget->indexOf(widget);
        if (index >= 0)
        {
            QVariant tabData = tabWidget->tabBar()->tabData(index);
            if (tabData.isValid())
                return tabData.toBool();
        }
    }
    
    return false; // Default: not pinned
}

// ============================================================================
// Slots
// ============================================================================

void LayoutManager::onTabDetached(QWidget* widget, const QString& title, const QPoint& globalPos)
{
    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Tab '{}' detached to new window", title.toStdString());

    auto* sourceWidget = qobject_cast<DetachableTabWidget*>(sender());

    DetachedWindow* window = new DetachedWindow(widget, title, qobject_cast<QWidget*>(parent()));
    connect(window, &DetachedWindow::windowClosed,
            this, &LayoutManager::onDetachedWindowClosed);

    // Place near cursor
    window->move(globalPos + QPoint(20, 20));

    detachedWindows.append(window);
    window->show();

    cleanupIfEmpty(sourceWidget);
}

void LayoutManager::onTabSplitRequested(int index, Qt::Orientation orientation, DropZone zone)
{
    Q_UNUSED(zone);

    auto* source = qobject_cast<DetachableTabWidget*>(sender());
    if (!source)
        return;

    DetachableTabWidget::TabInfo info = source->takeTabInfo(index);
    if (!info.page)
        return;

    // Default: split after the current view
    splitAround(source, orientation, false, info.page, info.title);
    cleanupIfEmpty(source);
}

void LayoutManager::onTabMoveCompleted(DetachableTabWidget* sourceWidget)
{
    cleanupIfEmpty(sourceWidget);
}

void LayoutManager::onDetachedWindowClosed(QWidget* contentWidget, const QString& title)
{
    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Detached window '{}' closed", title.toStdString());

    // Find and remove the window from our list
    for (int i = 0; i < detachedWindows.size(); ++i)
    {
        if (detachedWindows[i]->contentWidget() == contentWidget)
        {
            detachedWindows[i]->deleteLater();
            detachedWindows.removeAt(i);
            break;
        }
    }

    Q_EMIT detachedWindowClosedForMenu();
}

void LayoutManager::onTabCloseRequested(int index)
{
    auto* sourceWidget = qobject_cast<DetachableTabWidget*>(sender());
    if (!sourceWidget || index < 0 || index >= sourceWidget->count())
        return;

    const QString tabName = sourceWidget->tabText(index);
    QWidget* widget = sourceWidget->widget(index);

    sourceWidget->removeTab(index);
    widget->deleteLater();

    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Tab '{}' closed", tabName.toStdString());

    cleanupIfEmpty(sourceWidget);

    Q_EMIT tabClosedForMenu();
}

void LayoutManager::onTabPinToggled(QWidget* widget, bool pinned)
{
    if (!widget)
        return;
    
    // Update pin state in LayoutManager
    setTabPinned(widget, pinned);
}

void LayoutManager::setAvailableTabsMenu(QMenu* menu)
{
    availableTabsMenu = menu;
}

void LayoutManager::updateAvailableTabsMenu(BaseTreeElement* element)
{
    if (!element || !availableTabsMenu)
        return;

    currentSelectedElement = element;
    availableTabsMenu->clear();

    QStringList availableTabs = element->getAvailableTabNames();
    if (availableTabs.isEmpty())
    {
        availableTabsMenu->setEnabled(false);
        return;
    }

    // Filter out already open tabs
    QStringList unopenedTabs;
    for (const QString& tabName : availableTabs)
    {
        if (!isTabOpen(tabName))
            unopenedTabs << tabName;
    }

    if (unopenedTabs.isEmpty())
    {
        availableTabsMenu->setEnabled(false);
        return;
    }

    availableTabsMenu->setEnabled(true);
    for (const QString& tabName : unopenedTabs)
    {
        QAction* action = availableTabsMenu->addAction(tabName);
        connect(action, &QAction::triggered, this, [this, tabName]()
        {
            openTab(tabName);
        });
    }
}

void LayoutManager::openTab(const QString& tabName)
{
    if (!currentSelectedElement)
        return;

    // Open the specific tab using layout manager
    openTabForElement(currentSelectedElement, tabName);

    // Update the menu
    updateAvailableTabsMenu(currentSelectedElement);
}

void LayoutManager::clearAvailableTabsMenu()
{
    currentSelectedElement = nullptr;
    if (availableTabsMenu)
    {
        availableTabsMenu->clear();
        availableTabsMenu->setEnabled(false);
    }
}

void LayoutManager::openTabForElement(BaseTreeElement* element, const QString& tabName)
{
    if (!element)
        return;

    // Open the specific tab
    element->openTab(tabName);

    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Tab '{}' opened", tabName.toStdString());
}

void LayoutManager::onResetLayout()
{
    restoreDefaultLayout();

    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Layout reset to default");

    Q_EMIT layoutReset();
}

bool LayoutManager::isTabMimeType(const QMimeData* mime)
{
    return mime && mime->hasFormat(DetachableTabWidget::TabMimeType);
}

bool LayoutManager::shouldHandleDragEvent(const QPoint& globalPos) const
{
    // Don't handle if over tab bar (let tab widget handle it)
    if (isOverAnyTabBar(globalPos))
        return false;

    // Don't handle if outside content area
    if (!contentGlobalRect().contains(globalPos))
        return false;

    return true;
}

void LayoutManager::updateDropOverlay(const QPoint& globalPos)
{
    // VSCode-like: overlay is relative to the hovered tab group (editor group),
    // falling back to the whole content area if we can't resolve a group.
    QWidget* overlayTarget = tabGroupAtGlobalPos(globalPos);
    if (!overlayTarget)
        overlayTarget = contentSplitter;

    setDropOverlayTarget(overlayTarget);
    if (!dropOverlay->isVisible())
        showDropOverlay();

    dropOverlay->updateHighlight(dropOverlay->mapFromGlobal(globalPos));
}

bool LayoutManager::handleDragEnter(QDragEnterEvent* event)
{
    if (!isTabMimeType(event->mimeData()))
        return false;

    const QPoint globalPos = QCursor::pos();
    if (!shouldHandleDragEvent(globalPos))
    {
        hideDropOverlay();
        return false;
    }

    updateDropOverlay(globalPos);
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

bool LayoutManager::handleDragMove(QDragMoveEvent* event)
{
    if (!isTabMimeType(event->mimeData()))
        return false;

    const QPoint globalPos = QCursor::pos();
    if (!shouldHandleDragEvent(globalPos))
    {
        hideDropOverlay();
        return false;
    }

    updateDropOverlay(globalPos);
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

bool LayoutManager::handleDrop(QDropEvent* event)
{
    if (!isTabMimeType(event->mimeData()))
        return false;

    const QPoint globalPos = QCursor::pos();

    // If user drops on a tab bar, let that widget handle it.
    if (isOverAnyTabBar(globalPos))
    {
        hideDropOverlay();
        return false;
    }

    const QRect cg = contentGlobalRect();
    if (!cg.contains(globalPos))
    {
        hideDropOverlay();
        event->ignore();
        return true;
    }

    // Ensure the overlay geometry matches the final hovered target.
    QWidget* overlayTarget = tabGroupAtGlobalPos(globalPos);
    if (!overlayTarget)
        overlayTarget = contentSplitter;
    setDropOverlayTarget(overlayTarget);

    const QPoint localPos = dropOverlay->mapFromGlobal(globalPos);
    const DropZone zone = dropOverlay->getDropZone(localPos);

    DetachableTabWidget* source = nullptr;
    int sourceIndex = -1;
    if (!DetachableTabWidget::decodeTabMime(event->mimeData(), source, sourceIndex) || !source)
    {
        hideDropOverlay();
        event->ignore();
        return true;
    }

    // Take the tab from source.
    DetachableTabWidget::TabInfo info = source->takeTabInfo(sourceIndex);
    if (!info.page)
    {
        hideDropOverlay();
        event->ignore();
        return true;
    }

    DetachableTabWidget* target = tabGroupAtGlobalPos(globalPos);
    QList<DetachableTabWidget*> allTabWidgets = getTabWidgets();
    if (!target && !allTabWidgets.isEmpty())
        target = allTabWidgets.first();

    if (!target)
    {
        // Shouldn't happen, but avoid losing the tab.
        source->addTab(info.page, info.title);
        hideDropOverlay();
        event->ignore();
        return true;
    }

    switch (zone)
    {
        case DropZone::Center:
        case DropZone::Full:
        {
            target->addTab(info.page, info.title);
            target->setCurrentIndex(target->count() - 1);
            break;
        }
        case DropZone::Left:
            splitAround(target, Qt::Horizontal, true, info.page, info.title);
            break;
        case DropZone::Right:
            splitAround(target, Qt::Horizontal, false, info.page, info.title);
            break;
        case DropZone::Top:
            splitAround(target, Qt::Vertical, true, info.page, info.title);
            break;
        case DropZone::Bottom:
            splitAround(target, Qt::Vertical, false, info.page, info.title);
            break;
        default:
            // Fallback: put it back as a tab.
            target->addTab(info.page, info.title);
            target->setCurrentIndex(target->count() - 1);
            break;
    }

    cleanupIfEmpty(source);
    hideDropOverlay();
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

bool LayoutManager::eventFilter(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj);

    if (!event || !contentSplitter || !dropOverlay)
        return QObject::eventFilter(obj, event);

    switch (event->type())
    {
        case QEvent::DragLeave: 
        {
            hideDropOverlay();
            return QObject::eventFilter(obj, event);
        }

        case QEvent::DragEnter: 
        {
            auto* e = static_cast<QDragEnterEvent*>(event);
            if (handleDragEnter(e))
                return true;
            return QObject::eventFilter(obj, event);
        }

        case QEvent::DragMove: 
        {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (handleDragMove(e))
                return true;
            return QObject::eventFilter(obj, event);
        }

        case QEvent::Drop:
        {
            auto* e = static_cast<QDropEvent*>(event);
            if (handleDrop(e))
            {
                // When we consume the Drop event, QWidgetWindow::handleDropEvent
                // never runs and its m_dragTarget is never cleared. Send a
                // synthetic DragLeave so handleDragLeaveEvent resets it;
                // otherwise the next drag session hits
                // Q_ASSERT(m_dragTarget == nullptr) in handleDragEnterEvent.
                QWidget* mainWin = qobject_cast<QWidget*>(parent());
                if (mainWin && mainWin->windowHandle())
                {
                    QDragLeaveEvent leaveEvent;
                    QCoreApplication::sendEvent(mainWin->windowHandle(), &leaveEvent);
                }
                return true;
            }
            return QObject::eventFilter(obj, event);
        }

        default:
            break;
    }

    return QObject::eventFilter(obj, event);
}
