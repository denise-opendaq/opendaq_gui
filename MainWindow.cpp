#include "MainWindow.h"
#include "context/AppContext.h"
#include "dialogs/new_version_dialog.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "context/gui_constants.h"
#include "context/gui_styles.h"
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

            QColor selBg = opt.palette.color(QPalette::Highlight);
            selBg.setAlpha(70);
            QColor hoverBg = opt.palette.color(QPalette::Text);
            hoverBg.setAlpha(20);
            painter->setBrush(selected ? selBg : hoverBg);

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

    setupMenuBar();
    setupUI();
}

MainWindow::~MainWindow()
{
    if (componentTreeWidget)
        disconnect(componentTreeWidget, &ComponentTreeWidget::componentSelected,
                   this, &MainWindow::onComponentSelected);
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

    mainSplitter = new QSplitter(Qt::Horizontal, central);
    mainLayout->addWidget(mainSplitter);

    // === LEFT PANEL (Tree) ===
    QWidget* leftWidget = new QWidget();
    leftWidget->setObjectName("leftPanel");
    leftWidget->setAttribute(Qt::WA_StyledBackground, true);
    leftWidget->setStyleSheet(GUIStyles::sidebarPanelStyle("leftPanel"));
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                    GUIConstants::DEFAULT_LAYOUT_MARGIN);
    leftLayout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    searchBox = new QLineEdit();
    searchBox->setPlaceholderText(tr("Search components..."));
    searchBox->setClearButtonEnabled(true);
    searchBox->setStyleSheet(GUIStyles::searchLineEditStyle());
    leftLayout->addWidget(searchBox);

    // === RIGHT PANEL (Content) ===
    verticalSplitter = new QSplitter(Qt::Vertical);

    QWidget* contentWrapper = new QWidget();
    QVBoxLayout* contentWrapperLayout = new QVBoxLayout(contentWrapper);
    contentWrapperLayout->setContentsMargins(0, GUIConstants::DEFAULT_LAYOUT_MARGIN,
                                             GUIConstants::DEFAULT_LAYOUT_MARGIN, 0);
    contentWrapperLayout->setSpacing(0);

    contentHost = new QWidget();
    auto* contentHostLayout = new QVBoxLayout(contentHost);
    contentHostLayout->setContentsMargins(0, 0, 0, 0);
    contentHostLayout->setSpacing(0);
    contentWrapperLayout->addWidget(contentHost);

    verticalSplitter->addWidget(contentWrapper);

    componentTreeWidget = new ComponentTreeWidget(contentHost);
    componentTreeWidget->setAlternatingRowColors(false);
    componentTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    componentTreeWidget->setAllColumnsShowFocus(false);
    componentTreeWidget->setItemDelegate(new SidebarSelectionDelegate(componentTreeWidget));
    componentTreeWidget->setAttribute(Qt::WA_StyledBackground, true);
    componentTreeWidget->setStyleSheet(GUIStyles::sidebarTreeStyle());
    auto instance = AppContext::Instance()->daqInstance();
    if (instance.assigned())
        componentTreeWidget->loadInstance(instance);

    connect(componentTreeWidget, &ComponentTreeWidget::componentSelected,
            this, &MainWindow::onComponentSelected);

    connect(searchBox, &QLineEdit::textChanged,
            componentTreeWidget, &ComponentTreeWidget::setSearchFilter);

    leftLayout->addWidget(componentTreeWidget);

    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(verticalSplitter);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);

    // Log panel with right margin
    auto logContainerWidget = AppContext::Instance()->getLogContainerWidget();
    if (logContainerWidget)
    {
        QWidget* logWrapper = new QWidget();
        QVBoxLayout* logWrapperLayout = new QVBoxLayout(logWrapper);
        logWrapperLayout->setContentsMargins(0, 0, GUIConstants::DEFAULT_LAYOUT_MARGIN, 0);
        logWrapperLayout->setSpacing(0);
        logWrapperLayout->addWidget(logContainerWidget);
        verticalSplitter->addWidget(logWrapper);
    }

    const auto loggerComponent = AppContext::LoggerComponent();
    LOG_I("Application started...");
    LOG_I("Tip: Double-click a tab inside a component view to detach it into a window.");

    verticalSplitter->setSizes({GUIConstants::DEFAULT_VERTICAL_SPLITTER_CONTENT,
                                 GUIConstants::DEFAULT_VERTICAL_SPLITTER_LOG});

    mainSplitter->setSizes({GUIConstants::DEFAULT_HORIZONTAL_SPLITTER_LEFT,
                            GUIConstants::DEFAULT_HORIZONTAL_SPLITTER_RIGHT});
}

void MainWindow::clearContent()
{
    if (!contentHost)
        return;

    QLayout* layout = contentHost->layout();
    if (!layout)
        return;

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (QWidget* w = item->widget())
        {
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }
}

// ============================================================================
// Slots
// ============================================================================

void MainWindow::onComponentSelected(BaseTreeElement* element)
{
    if (!element)
        return;

    clearContent();
    element->onSelected();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
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

    for (const QString& path : paths)
    {
        try
        {
            instance.getModuleManager().loadModule(path.toStdString());
            LOG_I("Module loaded: {}", path.toStdString());
        }
        catch (const std::exception& e)
        {
            LOG_E("Failed to load module: {}", e.what());
        }
    }
}
