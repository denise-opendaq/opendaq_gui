#include "component/component_tree_widget.h"
#include "component/device_tree_element.h"
#include "context/AppContext.h"
#include "context/gui_constants.h"
#include "context/gui_styles.h"
#include "context/theme.h"
#include <functional>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <opendaq/instance_ptr.h>
#include <opendaq/custom_log.h>

ComponentTreeWidget::ComponentTreeWidget(QWidget* contentHost, QWidget* parent)
    : QWidget(parent)
    , contentHost(contentHost)
{
    setupUI();
}

ComponentTreeWidget::~ComponentTreeWidget() = default;

ComponentTreeWidget* ComponentTreeWidget::fromTreeWidget(QTreeWidget* tree)
{
    return qobject_cast<ComponentTreeWidget*>(tree ? tree->parentWidget() : nullptr);
}

void ComponentTreeWidget::loadInstance(const daq::InstancePtr& instance)
{
    m_tree->clear();
    rootElement.reset();

    if (!instance.assigned())
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Cannot load null instance");
        QMessageBox::critical(this, "Error", "Cannot load instance: instance is null.");
        return;
    }

    try
    {
        if (!contentHost)
        {
            const auto loggerComponent = AppContext::LoggerComponent();
            LOG_E("ComponentTreeWidget::loadInstance: contentHost is null");
            return;
        }

        rootElement = std::make_unique<DeviceTreeElement>(m_tree, instance.getRootDevice(), contentHost);
        rootElement->init();

        if (rootElement->getTreeItem())
            rootElement->getTreeItem()->setExpanded(true);

        refreshVisibility();
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error loading instance: {}", e.what());
        QMessageBox::critical(this, "Error",
            QString("Failed to load instance: %1").arg(e.what()));
    }
}

void ComponentTreeWidget::setContentHost(QWidget* contentHost)
{
    this->contentHost = contentHost;
}

BaseTreeElement* ComponentTreeWidget::getSelectedElement() const
{
    const auto selectedItems = m_tree->selectedItems();
    if (selectedItems.isEmpty())
        return nullptr;

    const auto item = selectedItems.first();
    const auto elementPtr = item->data(0, Qt::UserRole).value<void*>();
    return static_cast<BaseTreeElement*>(elementPtr);
}

BaseTreeElement* ComponentTreeWidget::findElementByGlobalId(const QString& globalId) const
{
    if (!rootElement)
        return nullptr;

    std::function<BaseTreeElement*(BaseTreeElement*)> search = [&](BaseTreeElement* element) -> BaseTreeElement* 
    {
        if (!element)
            return nullptr;

        if (element->getGlobalId() == globalId)
            return element;

        for (auto* child : element->getChildren().values()) 
        {
            if (auto* found = search(child))
                return found;
        }

        return nullptr;
    };

    return search(rootElement.get());
}

void ComponentTreeWidget::setShowHidden(bool show)
{
    AppContext::Instance()->setShowInvisibleComponents(show);
    refreshVisibility();
}

void ComponentTreeWidget::setSearchFilter(const QString& text)
{
    m_searchFilter = text;
    refreshVisibility();
}

bool ComponentTreeWidget::applySearchFilter(QTreeWidgetItem* item, const QString& filter)
{
    bool anyChildVisible = false;
    for (int i = 0; i < item->childCount(); ++i)
        if (applySearchFilter(item->child(i), filter))
            anyChildVisible = true;

    const bool selfMatches = item->text(0).contains(filter, Qt::CaseInsensitive);
    const bool visible = selfMatches || anyChildVisible;
    item->setHidden(!visible);
    if (anyChildVisible)
        item->setExpanded(true);
    return visible;
}

void ComponentTreeWidget::refreshVisibility()
{
    if (m_searchFilter.isEmpty())
    {
        if (rootElement)
            rootElement->showFiltered();
        return;
    }

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        applySearchFilter(m_tree->topLevelItem(i), m_searchFilter);
}

void ComponentTreeWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(GUIConstants::DEFAULT_LAYOUT_SPACING);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search components..."));
    m_searchBox->setClearButtonEnabled(true);
    Theme::bindStyle(m_searchBox, [] { return GUIStyles::searchLineEditStyle(); });
    connect(m_searchBox, &QLineEdit::textChanged, this, &ComponentTreeWidget::setSearchFilter);
    layout->addWidget(m_searchBox);

    m_tree = new QTreeWidget(this);
    m_tree->setAlternatingRowColors(false);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setAllColumnsShowFocus(false);
    m_tree->setAttribute(Qt::WA_StyledBackground, true);
    m_tree->setStyleSheet(GUIStyles::sidebarTreeStyle());
    m_tree->setHeaderLabel(tr("Components"));
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    // m_tree->setItemDelegate(new SidebarItemDelegate(m_tree));
    m_tree->viewport()->setAttribute(Qt::WA_Hover);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &ComponentTreeWidget::onSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ComponentTreeWidget::onContextMenuRequested);
    connect(AppContext::Instance(), &AppContext::showInvisibleChanged,
            this, &ComponentTreeWidget::refreshVisibility);
}

void ComponentTreeWidget::onSelectionChanged()
{
    const auto element = getSelectedElement();
    if (element)
        Q_EMIT componentSelected(element);
}

void ComponentTreeWidget::onContextMenuRequested(const QPoint& pos)
{
    const auto item = m_tree->itemAt(pos);
    if (!item)
        return;

    const auto elementPtr = item->data(0, Qt::UserRole).value<void*>();
    if (elementPtr)
    {
        const auto element = static_cast<BaseTreeElement*>(elementPtr);
        auto menu = element->onCreateRightClickMenu(this);
        if (menu && menu->actions().count() > 0)
            menu->exec(m_tree->mapToGlobal(pos));
        delete menu;
    }
}
