#include "MainWindow.h"
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
#include <QResizeEvent>
#include <QAbstractAnimation>
#include <QApplication>
#include <QWindowStateChangeEvent>
#include <QWidget>

#include "context/gui_constants.h"
#include "component/base_tree_element.h"
#include "component/component_tree_widget.h"

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("OpenDAQ GUI");
    resize(GUIConstants::DEFAULT_WINDOW_WIDTH, GUIConstants::DEFAULT_WINDOW_HEIGHT);
    setMinimumSize(GUIConstants::MIN_WINDOW_WIDTH, GUIConstants::MIN_WINDOW_HEIGHT);
    // Disable all dock animations to prevent crashes during resize/zoom
    // This prevents Qt's internal QWidgetAnimator from running during layout changes
    setDockOptions(QMainWindow::DockOption(0));
    
    // Install event filter on application to catch all resize events before they reach widgets
    // This allows us to stop animations before Qt's layout system processes them
    if (QApplication::instance())
    {
        QApplication::instance()->installEventFilter(this);
    }

    setupMenuBar();
    setupUI();

    // Ensure drag events are delivered even when hovering splitter handles/empty areas.
    // (Otherwise Qt may not deliver DragEnter/DragMove at all, and the overlay won't show.)
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    if (componentTreeWidget)
        disconnect(componentTreeWidget, &ComponentTreeWidget::componentSelected,
                   this, &MainWindow::onComponentSelected);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Intercept resize events at application level to stop animations before layout processes them
    // This is critical for preventing crashes in Qt's internal QWidgetAnimator
    if (event->type() == QEvent::Resize && obj != this)
    {
        // Stop animations before any widget resize to prevent Qt's internal animations from crashing
        stopAllAnimations();
    }
    
    // Drag & drop events are now handled by LayoutManager
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setGeometry(const QRect& rect)
{
    // Stop animations BEFORE geometry change to prevent crashes
    // This is called before resizeEvent, so we can prevent Qt's internal animations from starting
    stopAllAnimations();
    
    QMainWindow::setGeometry(rect);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    // Stop animations before resize to prevent crashes
    // This is a workaround for Qt bug where animations can access invalid memory during resize
    // Qt's internal QWidgetAnimator can still run even with dock options disabled
    
    stopAllAnimations();
    
    QMainWindow::resizeEvent(event);
}

void MainWindow::stopAllAnimations()
{
    // Stop our custom animations
    if (layoutManager && layoutManager->getDropOverlay())
    {
        layoutManager->getDropOverlay()->stopAnimations();
    }
    
    // Try to find and stop any animations that are children of this window or the application
    // This may catch some Qt internal animations
    QList<QAbstractAnimation*> windowAnimations = findChildren<QAbstractAnimation*>();
    for (QAbstractAnimation* anim : windowAnimations)
    {
        if (anim && anim->state() == QAbstractAnimation::Running)
        {
            anim->stop();
        }
    }
    
    // Also check application-level animations
    if (QApplication::instance())
    {
        QList<QAbstractAnimation*> appAnimations = QApplication::instance()->findChildren<QAbstractAnimation*>();
        for (QAbstractAnimation* anim : appAnimations)
        {
            if (anim && anim->state() == QAbstractAnimation::Running)
            {
                anim->stop();
            }
        }
    }
    
    // Also check all widgets in the application for animations
    // This is more aggressive but may catch Qt's internal animations
    QWidgetList allWidgets = QApplication::allWidgets();
    for (QWidget* widget : allWidgets)
    {
        if (widget)
        {
            QList<QAbstractAnimation*> widgetAnimations = widget->findChildren<QAbstractAnimation*>();
            for (QAbstractAnimation* anim : widgetAnimations)
            {
                if (anim && anim->state() == QAbstractAnimation::Running)
                {
                    anim->stop();
                }
            }
        }
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    // Stop animations when window state changes (minimize, maximize, fullscreen, etc.)
    // These state changes can trigger layout animations that cause crashes
    if (event->type() == QEvent::WindowStateChange)
    {
        stopAllAnimations();
    }
    
    QMainWindow::changeEvent(event);
}

void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();

    // File menu
    QMenu* fileMenu = menuBar->addMenu("File");
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

    // Reset Layout action (will be connected after LayoutManager is created)
    resetLayoutAction = viewMenu->addAction("Reset Layout");
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
    leftLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN, 
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN, 
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN, 
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN);
    leftLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    viewSelector = new QComboBox();
    viewSelector->addItems({"System Overview", "Signals", "Channels", "Function blocks", "Full Topology"});
    connect(viewSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onViewSelectionChanged);
    leftLayout->addWidget(viewSelector);

    // === RIGHT PANEL (Content) ===
    // Create vertical splitter for content area (tabs | log)
    verticalSplitter = new QSplitter(Qt::Vertical);
    
    // Content area splitter (for tab management) with margins
    QWidget* contentWrapper = new QWidget();
    QVBoxLayout* contentWrapperLayout = new QVBoxLayout(contentWrapper);
    contentWrapperLayout->setContentsMargins(0, GUIConstants::DEFAULT_LAYOUT_MARGIN, 
                                             GUIConstants::DEFAULT_LAYOUT_MARGIN, 0);
    contentWrapperLayout->setSpacing(0);
    
    contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setAcceptDrops(true);
    contentWrapperLayout->addWidget(contentSplitter);
    
    verticalSplitter->addWidget(contentWrapper);

    // Create LayoutManager BEFORE ComponentTreeWidget
    layoutManager = new LayoutManager(contentSplitter, this);

    // Create ComponentTreeWidget and load openDAQ instance
    componentTreeWidget = new ComponentTreeWidget(layoutManager);
    auto instance = AppContext::Instance()->daqInstance();
    if (instance.assigned())
        componentTreeWidget->loadInstance(instance);

    // Connect component selection to show properties
    connect(componentTreeWidget, &ComponentTreeWidget::componentSelected,
            this, &MainWindow::onComponentSelected);

    leftLayout->addWidget(componentTreeWidget);
    
    // Add widgets to main splitter
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(verticalSplitter);
    mainSplitter->setStretchFactor(0, 0); // Left panel doesn't stretch
    mainSplitter->setStretchFactor(1, 1); // Right panel stretches

    // Connect Reset Layout action from menu
    if (resetLayoutAction)
    {
        connect(resetLayoutAction, &QAction::triggered, layoutManager, &LayoutManager::onResetLayout);
        connect(layoutManager, &LayoutManager::layoutReset, layoutManager, &LayoutManager::clearAvailableTabsMenu);
        
        // Set available tabs menu in layout manager
        layoutManager->setAvailableTabsMenu(availableTabsMenu);
    }

    // Log panel with right margin
    auto logContainerWidget = AppContext::Instance()->getLogContainerWidget();
    if(logContainerWidget) {
        QWidget* logWrapper = new QWidget();
        QVBoxLayout* logWrapperLayout = new QVBoxLayout(logWrapper);
        logWrapperLayout->setContentsMargins(0, 0, GUIConstants::DEFAULT_LAYOUT_MARGIN, 0);
        logWrapperLayout->setSpacing(0);
        logWrapperLayout->addWidget(logContainerWidget);
        verticalSplitter->addWidget(logWrapper);
    }

    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Application started...");
    LOG_I("Tip: Drag tabs like in VSCode to move/split/detach.");


    verticalSplitter->setSizes({GUIConstants::DEFAULT_VERTICAL_SPLITTER_CONTENT, 
                                 GUIConstants::DEFAULT_VERTICAL_SPLITTER_LOG});

    mainSplitter->setSizes({GUIConstants::DEFAULT_HORIZONTAL_SPLITTER_LEFT,
                            GUIConstants::DEFAULT_HORIZONTAL_SPLITTER_RIGHT});
}

// ============================================================================
// Slots
// ============================================================================


void MainWindow::onViewSelectionChanged(int index)
{
    const QString viewName = viewSelector->itemText(index);
    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("View changed to: {}", viewName.toStdString());

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

void MainWindow::onComponentSelected(BaseTreeElement* element)
{
    if (!element)
        return;

    layoutManager->clear();
    element->onSelected();

    // Update available tabs menu
    layoutManager->updateAvailableTabsMenu(element);
}

void MainWindow::onShowHiddenComponentsToggled(bool checked)
{
    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Show hidden components: {}", checked ? "ON" : "OFF");

    // Update the component tree to show/hide hidden components
    if (componentTreeWidget)
        componentTreeWidget->setShowHidden(checked);
}


