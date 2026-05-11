#include "MainWindow.h"
#include "context/AppContext.h"
#include "dialogs/new_version_dialog.h"

#include <QApplication>
#include <QTimer>
#include <QShowEvent>
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
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>

#include "context/gui_constants.h"
#include "component/base_tree_element.h"
#include "component/component_tree_widget.h"

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>

namespace
{
class SidebarSelectionDelegate final : public QStyledItemDelegate
{
public:
    explicit SidebarSelectionDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        const bool hovered = opt.state.testFlag(QStyle::State_MouseOver);

        if ((selected || hovered) && opt.widget)
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor("#dfe9ff") : QColor("#ececec"));

            QRect r = opt.rect.adjusted(6, 1, -6, -1);

            if (auto* view = qobject_cast<const QAbstractItemView*>(opt.widget))
            {
                const int viewportW = view->viewport() ? view->viewport()->width() : r.width();
                r.setLeft(6);
                r.setRight(viewportW - 6);
            }

            painter->drawRoundedRect(r, 10.0, 10.0);
            painter->restore();
        }

        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_MouseOver;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("OpenDAQ GUI");

#ifndef APP_VERSION
#define APP_VERSION "0.0"
#endif
    m_updateChecker = new UpdateChecker(QString::fromUtf8(APP_VERSION), this);
    connect(m_updateChecker, &UpdateChecker::updateAvailable,
            this, &MainWindow::onUpdateAvailable);
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
    showHiddenAction->setChecked(AppContext::Instance()->showInvisibleComponents());
    connect(showHiddenAction, &QAction::toggled, this, &MainWindow::onShowHiddenComponentsToggled);

    expandAllPropertiesAction = viewMenu->addAction("Expand all properties");
    expandAllPropertiesAction->setCheckable(true);
    expandAllPropertiesAction->setChecked(AppContext::Instance()->expandAllProperties());
    connect(expandAllPropertiesAction, &QAction::toggled, this, [](bool checked)
    {
        AppContext::Instance()->setExpandAllProperties(checked);
    });

    viewMenu->addSeparator();

    // Available tabs submenu
    availableTabsMenu = viewMenu->addMenu("Available Tabs");
    availableTabsMenu->setEnabled(false);

    viewMenu->addSeparator();

    // Reset Layout action (will be connected after LayoutManager is created)
    resetLayoutAction = viewMenu->addAction("Reset Layout");

    // Modules menu
    QMenu* modulesMenu = menuBar->addMenu("Modules");
    QAction* loadModuleAction = modulesMenu->addAction("Load Module...");
    connect(loadModuleAction, &QAction::triggered, this, &MainWindow::onLoadModuleTriggered);
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
    leftWidget->setObjectName("leftPanel");
    leftWidget->setAttribute(Qt::WA_StyledBackground, true);
    leftWidget->setStyleSheet(
        "QWidget#leftPanel {"
        "  background-color: #f2f2f2;"
        "  border: 1px solid #e0e0e0;"
        "}"
    );
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN, 
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN, 
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN, 
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN);
    leftLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    searchBox = new QLineEdit();
    searchBox->setPlaceholderText(tr("Search components..."));
    searchBox->setClearButtonEnabled(true);
    searchBox->setStyleSheet(
        "QLineEdit {"
        "  background-color: #f7f7f7;"
        "  border: 1px solid #e5e5e5;"
        "  border-radius: 10px;"
        "  padding: 8px 10px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #c8d6ff;"
        "  background-color: #ffffff;"
        "}"
    );
    leftLayout->addWidget(searchBox);

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
    componentTreeWidget->setAlternatingRowColors(false);
    componentTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    componentTreeWidget->setAllColumnsShowFocus(false);
    componentTreeWidget->setItemDelegate(new SidebarSelectionDelegate(componentTreeWidget));
    componentTreeWidget->setAttribute(Qt::WA_StyledBackground, true);
    componentTreeWidget->setStyleSheet(
        "QTreeWidget {"
        "  background-color: #f2f2f2;"
        "  border: none;"
        "  font-size: 13px;"
        "  outline: 0;"
        "  show-decoration-selected: 1;"
        "  selection-background-color: transparent;"
        "  selection-color: #1f2a44;"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  border-radius: 8px;"
        "}"
        "QTreeWidget::item:selected, QTreeWidget::item:selected:active, QTreeWidget::item:selected:!active {"
        "  color: #1f2a44;"
        "}"
"QTreeView::branch {"
        "  background: transparent;"
        "}"
        "QTreeView::branch:selected {"
        "  background: transparent;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f2f2f2;"
        "  border: none;"
        "  padding: 6px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  color: #6b6b6b;"
        "}"
        "QHeaderView {"
        "  background-color: #f2f2f2;"
        "}"
    );
    auto instance = AppContext::Instance()->daqInstance();
    if (instance.assigned())
        componentTreeWidget->loadInstance(instance);

    // Connect component selection to show properties
    connect(componentTreeWidget, &ComponentTreeWidget::componentSelected,
            this, &MainWindow::onComponentSelected);

    connect(searchBox, &QLineEdit::textChanged,
            componentTreeWidget, &ComponentTreeWidget::setSearchFilter);

    leftLayout->addWidget(componentTreeWidget);
    
    // Add widgets to main splitter
    QWidget* leftWrapper = new QWidget();
    QVBoxLayout* leftWrapperLayout = new QVBoxLayout(leftWrapper);
    leftWrapperLayout->setContentsMargins(0, 0, 0, 0);
    leftWrapperLayout->setSpacing(0);
    leftWrapperLayout->addWidget(leftWidget);

    mainSplitter->addWidget(leftWrapper);
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



void MainWindow::onComponentSelected(BaseTreeElement* element)
{
    if (!element)
        return;

    layoutManager->clear();
    element->onSelected();

    // Update available tabs menu
    layoutManager->updateAvailableTabsMenu(element);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // Defer update check so the window appears first and the request doesn't block startup
    QTimer::singleShot(1000, this, [this]() {
        if (m_updateChecker)
            m_updateChecker->checkForUpdates();
    });
}

void MainWindow::onUpdateAvailable(const QString& version, const QString& changelog, const QString& releaseUrl,
                                    const QList<ReleaseAsset>& assets)
{
    NewVersionDialog dlg(version, changelog, releaseUrl, assets, this);
    dlg.exec();
}

void MainWindow::onShowHiddenComponentsToggled(bool checked)
{
    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Show hidden components: {}", checked ? "ON" : "OFF");

    // Update the component tree to show/hide hidden components
    if (componentTreeWidget)
        componentTreeWidget->setShowHidden(checked);
}

void MainWindow::onLoadModuleTriggered()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        tr("Select Module(s)"),
        QString(),
        tr("Shared Libraries (*.so *.dll *.dylib);;All Files (*)")
    );

    if (paths.isEmpty())
        return;

    auto instance = AppContext::Instance()->daqInstance();
    if (!instance.assigned())
    {
        QMessageBox::warning(this, tr("Load Module"), tr("No openDAQ instance available."));
        return;
    }

    const auto loggerComponent = AppContext::LoggerComponent();
    QStringList errors;

    for (const QString& path : paths)
    {
        try
        {
            instance.getModuleManager().loadModule(path.toStdString());
            LOG_I("Module loaded: {}", path.toStdString());
        }
        catch (const std::exception& e)
        {
            errors << tr("%1:\n%2").arg(path, e.what());
        }
    }

    if (!errors.isEmpty())
        QMessageBox::critical(this, tr("Load Module"), tr("Failed to load module(s):\n\n%1").arg(errors.join("\n\n")));
}


