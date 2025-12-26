#include "MainWindow.h"
#include "DetachedWindow.h"
#include "context/AppContext.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QLabel>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCursor>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("OpenDAQ GUI");
    resize(1200, 800);
    setMinimumSize(800, 600);

    setupMenuBar();
    setupUI();

    // Global router for our custom tab drag mime type.
    qApp->installEventFilter(this);

    // Ensure drag events are delivered even when hovering splitter handles/empty areas.
    // (Otherwise Qt may not deliver DragEnter/DragMove at all, and the overlay won't show.)
    setAcceptDrops(true);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj);

    if (!event || !dropOverlay || !contentSplitter)
        return QMainWindow::eventFilter(obj, event);

    const auto isTabMime = [](const QMimeData* mime) {
        return mime && mime->hasFormat(DetachableTabWidget::TabMimeType);
    };

    auto hideOverlay = [this]() {
        if (dropOverlay->isVisible())
            dropOverlay->hide();
    };

    switch (event->type()) {
        case QEvent::DragLeave: {
            hideOverlay();
            return QMainWindow::eventFilter(obj, event);
        }

        case QEvent::DragEnter: {
            auto* e = static_cast<QDragEnterEvent*>(event);
            if (!isTabMime(e->mimeData()))
                return QMainWindow::eventFilter(obj, event);

            const QPoint localPos = e->position().toPoint();
            const QPoint globalPos = mapToGlobal(localPos);

            // Let tab bars handle "drop as tab" directly.
            if (isOverAnyTabBar(globalPos)) {
                hideOverlay();
                return QMainWindow::eventFilter(obj, event);
            }

            if (!contentGlobalRect().contains(globalPos)) {
                hideOverlay();
                return QMainWindow::eventFilter(obj, event);
            }

            // VSCode-like: overlay is relative to the hovered tab group (editor group),
            // falling back to the whole content area if we can't resolve a group.
            QWidget* overlayTarget = tabGroupAtGlobalPos(globalPos);
            if (!overlayTarget)
                overlayTarget = contentSplitter;

            dropOverlay->setTargetWidget(overlayTarget);
            dropOverlay->show();
            dropOverlay->updateHighlight(globalPos - dropOverlay->geometry().topLeft());

            e->setDropAction(Qt::MoveAction);
            e->accept();
            return true;
        }

        case QEvent::DragMove: {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (!isTabMime(e->mimeData()))
                return QMainWindow::eventFilter(obj, event);

            const QPoint localPos = e->position().toPoint();
            const QPoint globalPos = mapToGlobal(localPos);

            if (isOverAnyTabBar(globalPos)) {
                hideOverlay();
                return QMainWindow::eventFilter(obj, event);
            }

            if (!contentGlobalRect().contains(globalPos)) {
                hideOverlay();
                return QMainWindow::eventFilter(obj, event);
            }

            QWidget* overlayTarget = tabGroupAtGlobalPos(globalPos);
            if (!overlayTarget)
                overlayTarget = contentSplitter;

            dropOverlay->setTargetWidget(overlayTarget);
            if (!dropOverlay->isVisible())
                dropOverlay->show();

            dropOverlay->updateHighlight(globalPos - dropOverlay->geometry().topLeft());

            e->setDropAction(Qt::MoveAction);
            e->accept();
            return true;
        }

        case QEvent::Drop: {
            auto* e = static_cast<QDropEvent*>(event);
            if (!isTabMime(e->mimeData()))
                return QMainWindow::eventFilter(obj, event);

            const QPoint eventPos = e->position().toPoint();
            const QPoint globalPos = mapToGlobal(eventPos);

            // If user drops on a tab bar, let that widget handle it.
            if (isOverAnyTabBar(globalPos)) {
                hideOverlay();
                return QMainWindow::eventFilter(obj, event);
            }

            const QRect cg = contentGlobalRect();
            if (!cg.contains(globalPos)) {
                hideOverlay();
                e->ignore();
                return true;
            }

            // Ensure the overlay geometry matches the final hovered target.
            QWidget* overlayTarget = tabGroupAtGlobalPos(globalPos);
            if (!overlayTarget)
                overlayTarget = contentSplitter;
            dropOverlay->setTargetWidget(overlayTarget);

            const QPoint localPos = globalPos - dropOverlay->geometry().topLeft();
            const DropZone zone = dropOverlay->getDropZone(localPos);

            DetachableTabWidget* source = nullptr;
            int sourceIndex = -1;
            if (!DetachableTabWidget::decodeTabMime(e->mimeData(), source, sourceIndex) || !source) {
                hideOverlay();
                e->ignore();
                return true;
            }

            // Take the tab from source.
            DetachableTabWidget::TabInfo info = source->takeTabInfo(sourceIndex);
            if (!info.page) {
                hideOverlay();
                e->ignore();
                return true;
            }

            DetachableTabWidget* target = tabGroupAtGlobalPos(globalPos);
            if (!target && !tabWidgets.isEmpty())
                target = tabWidgets.first();

            if (!target) {
                // Shouldn't happen, but avoid losing the tab.
                source->addTab(info.page, info.title);
                hideOverlay();
                e->ignore();
                return true;
            }

            switch (zone) {
                case DropZone::Center:
                case DropZone::Full: {
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

            hideOverlay();
            e->setDropAction(Qt::MoveAction);
            e->accept();
            return true;
        }

        default:
            break;
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();

    // File menu
    QMenu* fileMenu = menuBar->addMenu("File");
    fileMenu->addAction("Load configuration");
    fileMenu->addAction("Save configuration");
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("Exit");
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // View menu
    QMenu* viewMenu = menuBar->addMenu("View");
    showHiddenAction = viewMenu->addAction("Show hidden components");
    showHiddenAction->setCheckable(true);
    connect(showHiddenAction, &QAction::toggled, this, &MainWindow::onShowHiddenComponentsToggled);

    viewMenu->addSeparator();

    // Available tabs submenu
    availableTabsMenu = viewMenu->addMenu("Available Tabs");
    availableTabsMenu->setEnabled(false);

    viewMenu->addSeparator();

    QAction* resetAction = viewMenu->addAction("Reset Layout");
    connect(resetAction, &QAction::triggered, this, &MainWindow::onResetLayout);
}

void MainWindow::setupUI()
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Main horizontal splitter (left tree | right content)
    mainSplitter = new QSplitter(Qt::Horizontal, central);
    mainLayout->addWidget(mainSplitter);

    // === LEFT PANEL (Tree) ===
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(8);

    viewSelector = new QComboBox();
    viewSelector->addItems({"System Overview", "Signals", "Channels", "Function blocks", "Full Topology"});
    connect(viewSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onViewSelectionChanged);
    leftLayout->addWidget(viewSelector);

    // Create ComponentTreeWidget and load openDAQ instance
    componentTreeWidget = new ComponentTreeWidget();
    auto instance = AppContext::instance()->daqInstance();
    if (instance.assigned())
    {
        componentTreeWidget->loadInstance(instance);
    }

    // Connect component selection to show properties
    connect(componentTreeWidget, &ComponentTreeWidget::componentSelected,
            this, &MainWindow::onComponentSelected);

    leftLayout->addWidget(componentTreeWidget);

    mainSplitter->addWidget(leftWidget);

    // === RIGHT PANEL (Content + Log) ===
    QWidget* rightWidget = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(0);

    verticalSplitter = new QSplitter(Qt::Vertical, rightWidget);
    rightLayout->addWidget(verticalSplitter);

    contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setAcceptDrops(true);
    verticalSplitter->addWidget(contentSplitter);

    // Initial tab group
    tabWidget = createTabGroup();
    contentSplitter->addWidget(tabWidget);

    // Drop overlay (input-transparent tool window)
    dropOverlay = new DropOverlay(this);
    dropOverlay->setTargetWidget(contentSplitter);
    dropOverlay->hide();

    // Log panel
    auto logTextEdit = AppContext::instance()->getLogTextEdit();
    if(logTextEdit)
        verticalSplitter->addWidget(logTextEdit);
    qWarning() << "Application started...";
    qWarning() << "Tip: Drag tabs like in VSCode to move/split/detach.";


    verticalSplitter->setSizes({600, 200});

    mainSplitter->addWidget(rightWidget);
    mainSplitter->setSizes({300, 900});
}

DetachableTabWidget* MainWindow::createTabGroup()
{
    auto* tw = new DetachableTabWidget();
    tw->setTabsClosable(true);
    registerTabGroup(tw);
    tabWidgets.append(tw);
    return tw;
}

void MainWindow::registerTabGroup(DetachableTabWidget* tw)
{
    if (!tw)
        return;

    connect(tw, &DetachableTabWidget::tabDetached,
            this, &MainWindow::onTabDetached);
    connect(tw, &DetachableTabWidget::tabSplitRequested,
            this, &MainWindow::onTabSplitRequested);
    connect(tw, &DetachableTabWidget::tabMoveCompleted,
            this, &MainWindow::onTabMoveCompleted);
    connect(tw, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);
}

QRect MainWindow::contentGlobalRect() const
{
    if (!contentSplitter)
        return QRect();

    const QPoint tl = contentSplitter->mapToGlobal(QPoint(0, 0));
    return QRect(tl, contentSplitter->size());
}

bool MainWindow::isOverAnyTabBar(const QPoint& globalPos) const
{
    QWidget* w = QApplication::widgetAt(globalPos);
    while (w) {
        if (qobject_cast<QTabBar*>(w))
            return true;
        w = w->parentWidget();
    }
    return false;
}

DetachableTabWidget* MainWindow::tabGroupAtGlobalPos(const QPoint& globalPos) const
{
    auto pick = [](const QPoint& pos) -> DetachableTabWidget* {
        QWidget* w = QApplication::widgetAt(pos);
        while (w) {
            if (auto* tw = qobject_cast<DetachableTabWidget*>(w))
                return tw;
            w = w->parentWidget();
        }
        return nullptr;
    };

    // Fast path.
    if (auto* tw = pick(globalPos))
        return tw;

    // If we're over a splitter handle, Qt will often report QSplitterHandle.
    // Probe a few nearby points to select the adjacent tab group, which feels
    // much more "VSCode-like".
    static const QPoint offsets[] = {
        {-10, 0}, {10, 0}, {0, -10}, {0, 10},
        {-25, 0}, {25, 0}, {0, -25}, {0, 25}
    };

    for (const QPoint& off : offsets) {
        if (auto* tw = pick(globalPos + off))
            return tw;
    }

    return nullptr;
}

void MainWindow::splitAround(DetachableTabWidget* target, Qt::Orientation orientation, bool before,
                            QWidget* widget, const QString& title)
{
    if (!target || !widget)
        return;

    DetachableTabWidget* newGroup = createTabGroup();
    newGroup->addTab(widget, title);
    newGroup->setCurrentIndex(0);

    QSplitter* parent = qobject_cast<QSplitter*>(target->parentWidget());
    if (!parent) {
        // Fallback: just append.
        contentSplitter->addWidget(newGroup);
        return;
    }

    if (parent->orientation() == orientation) {
        int idx = parent->indexOf(target);
        if (!before)
            idx++;
        parent->insertWidget(idx, newGroup);
        parent->setSizes({1, 1});
        return;
    }

    // Need a nested splitter with the desired orientation.
    QSplitter* nested = new QSplitter(orientation);
    nested->setChildrenCollapsible(false);
    nested->setAcceptDrops(true);

    const int idx = parent->indexOf(target);
    QWidget* old = parent->replaceWidget(idx, nested);

    // old == target (should).
    if (before) {
        nested->addWidget(newGroup);
        nested->addWidget(old);
    } else {
        nested->addWidget(old);
        nested->addWidget(newGroup);
    }
    nested->setSizes({1, 1});
}

void MainWindow::cleanupIfEmpty(DetachableTabWidget* tw)
{
    if (!tw)
        return;

    if (tw->count() != 0)
        return;

    // Keep at least one tab group alive.
    if (tabWidgets.size() <= 1)
        return;

    removeTabGroup(tw);
}

void MainWindow::removeTabGroup(DetachableTabWidget* tw)
{
    if (!tw)
        return;

    tabWidgets.removeOne(tw);

    QSplitter* parent = qobject_cast<QSplitter*>(tw->parentWidget());
    if (parent) {
        tw->setParent(nullptr);
    }
    tw->deleteLater();

    if (parent)
        collapseSplitterIfNeeded(parent);
}

void MainWindow::collapseSplitterIfNeeded(QSplitter* splitter)
{
    if (!splitter)
        return;

    if (splitter->count() != 1)
        return;

    QWidget* only = splitter->widget(0);
    QSplitter* parent = qobject_cast<QSplitter*>(splitter->parentWidget());

    // Don't delete the root splitter (contentSplitter) – keep the container stable.
    if (!parent || splitter == contentSplitter)
        return;

    const int idx = parent->indexOf(splitter);

    only->setParent(nullptr);
    parent->replaceWidget(idx, only);
    splitter->deleteLater();

    collapseSplitterIfNeeded(parent);
}

void MainWindow::clearSplitterRecursively(QSplitter* splitter)
{
    if (!splitter)
        return;

    while (splitter->count() > 0) {
        QWidget* w = splitter->widget(0);
        w->setParent(nullptr);

        if (auto* child = qobject_cast<QSplitter*>(w)) {
            clearSplitterRecursively(child);
        }

        w->deleteLater();
    }
}

// ============================================================================
// Slots
// ============================================================================

void MainWindow::onTabDetached(QWidget* widget, const QString& title, const QPoint& globalPos)
{
    qWarning() << QString("Tab '%1' detached to new window").arg(title);

    auto* sourceWidget = qobject_cast<DetachableTabWidget*>(sender());

    DetachedWindow* window = new DetachedWindow(widget, title, this);
    connect(window, &DetachedWindow::windowClosed,
            this, &MainWindow::onDetachedWindowClosed);

    // Place near cursor
    window->move(globalPos + QPoint(20, 20));

    detachedWindows.append(window);
    window->show();

    cleanupIfEmpty(sourceWidget);

    // Update available tabs menu if we have a selected element
    if (currentSelectedElement) {
        updateAvailableTabsMenu(currentSelectedElement);
    }
}

void MainWindow::onTabSplitRequested(int index, Qt::Orientation orientation, DropZone zone)
{
    Q_UNUSED(zone);

    auto* source = qobject_cast<DetachableTabWidget*>(sender());
    if (!source)
        return;

    DetachableTabWidget::TabInfo info = source->takeTabInfo(index);
    if (!info.page)
        return;

    // Default: split after the current view.
    splitAround(source, orientation, false, info.page, info.title);
    cleanupIfEmpty(source);
}

void MainWindow::onTabMoveCompleted(DetachableTabWidget* sourceWidget)
{
    cleanupIfEmpty(sourceWidget);
}

void MainWindow::onDetachedWindowClosed(QWidget* contentWidget, const QString& title)
{
    qWarning() << QString("Detached window '%1' closed").arg(title);

    // Find and remove the window from our list
    for (int i = 0; i < detachedWindows.size(); ++i) {
        if (detachedWindows[i]->contentWidget() == contentWidget) {
            detachedWindows[i]->deleteLater();
            detachedWindows.removeAt(i);
            break;
        }
    }

    // Update available tabs menu if we have a selected element
    if (currentSelectedElement) {
        updateAvailableTabsMenu(currentSelectedElement);
    }
}

void MainWindow::onViewSelectionChanged(int index)
{
    const QString viewName = viewSelector->itemText(index);
    qWarning() << QString("View changed to: %1").arg(viewName);

    // Update component type filter based on selection
    QSet<QString> componentsToShow;
    if (viewName == "System Overview")
    {
        componentsToShow = {"Device", "Folder", "Signal", "Channel", "FunctionBlock"};
    }
    else if (viewName == "Signals")
    {
        componentsToShow = {"Device", "Signal"};
    }
    else if (viewName == "Channels")
    {
        componentsToShow = {"Device", "Channel"};
    }
    else if (viewName == "Function blocks")
    {
        componentsToShow = {"Device", "FunctionBlock"};
    }

    if (componentTreeWidget)
    {
        componentTreeWidget->setComponentTypeFilter(componentsToShow);
    }
}

void MainWindow::onTreeItemSelected()
{
    // This slot is no longer used since ComponentTreeWidget handles selection internally
    // The tree selection is now managed by the ComponentTreeWidget
}

void MainWindow::onComponentSelected(BaseTreeElement* element)
{
    if (!element)
        return;

    if (!tabWidget)
        return;

    currentSelectedElement = element;

    tabWidget->clear();
    element->onSelected(tabWidget);

    // Update available tabs menu
    updateAvailableTabsMenu(element);
}

void MainWindow::onShowHiddenComponentsToggled(bool checked)
{
    qWarning() << QString("Show hidden components: %1").arg(checked ? "ON" : "OFF");

    // Update the component tree to show/hide hidden components
    if (componentTreeWidget)
    {
        componentTreeWidget->setShowHidden(checked);
    }
}

void MainWindow::onTabCloseRequested(int index)
{
    auto* sourceWidget = qobject_cast<DetachableTabWidget*>(sender());
    if (!sourceWidget || index < 0 || index >= sourceWidget->count())
        return;

    const QString tabName = sourceWidget->tabText(index);
    QWidget* widget = sourceWidget->widget(index);

    sourceWidget->removeTab(index);
    widget->deleteLater();

    qWarning() << QString("Tab '%1' closed").arg(tabName);

    cleanupIfEmpty(sourceWidget);

    // Update available tabs menu if we have a selected element
    if (currentSelectedElement) {
        updateAvailableTabsMenu(currentSelectedElement);
    }
}

bool MainWindow::isTabOpen(const QString& tabName) const
{
    // Check all tab widgets and detached windows
    for (auto* tabWidget : tabWidgets) {
        for (int i = 0; i < tabWidget->count(); ++i) {
            if (tabWidget->tabText(i) == tabName) {
                return true;
            }
        }
    }

    for (auto* window : detachedWindows) {
        if (window->windowTitle() == tabName) {
            return true;
        }
    }

    return false;
}

void MainWindow::updateAvailableTabsMenu(BaseTreeElement* element)
{
    if (!element || !availableTabsMenu)
        return;

    availableTabsMenu->clear();

    QStringList availableTabs = element->getAvailableTabNames();
    if (availableTabs.isEmpty()) {
        availableTabsMenu->setEnabled(false);
        return;
    }

    // Filter out already open tabs
    QStringList unopenedTabs;
    for (const QString& tabName : availableTabs) {
        if (!isTabOpen(tabName)) {
            unopenedTabs << tabName;
        }
    }

    if (unopenedTabs.isEmpty()) {
        availableTabsMenu->setEnabled(false);
        return;
    }

    availableTabsMenu->setEnabled(true);
    for (const QString& tabName : unopenedTabs) {
        QAction* action = availableTabsMenu->addAction(tabName);
        connect(action, &QAction::triggered, this, [this, tabName]() {
            onOpenTab(tabName);
        });
    }
}

void MainWindow::onOpenTab(const QString& tabName)
{
    if (!currentSelectedElement || tabWidgets.isEmpty())
        return;

    // Find or create a tab widget to add the tab to
    DetachableTabWidget* targetTabWidget = tabWidgets.first();
    if (!targetTabWidget) {
        targetTabWidget = createTabGroup();
        contentSplitter->addWidget(targetTabWidget);
    }

    // Open the specific tab
    currentSelectedElement->openTab(tabName, targetTabWidget);

    // Update the menu
    updateAvailableTabsMenu(currentSelectedElement);

    qWarning() << QString("Tab '%1' opened").arg(tabName);
}

void MainWindow::restoreDefaultLayout()
{
    // Close all detached windows (and delete their content)
    for (auto* w : detachedWindows) {
        w->close();
        w->deleteLater();
    }
    detachedWindows.clear();

    // Reset content area to a single tab group
    clearSplitterRecursively(contentSplitter);
    tabWidgets.clear();

    tabWidget = createTabGroup();
    contentSplitter->setOrientation(Qt::Horizontal);
    contentSplitter->addWidget(tabWidget);

    // Clear current selection
    currentSelectedElement = nullptr;
    availableTabsMenu->clear();
    availableTabsMenu->setEnabled(false);

    if (dropOverlay) {
        dropOverlay->setTargetWidget(contentSplitter);
        dropOverlay->hide();
    }
}

void MainWindow::onResetLayout()
{
    restoreDefaultLayout();
    qWarning() << "Layout reset to default";
}
