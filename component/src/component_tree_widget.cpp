#include "component/component_tree_widget.h"
#include "component/device_tree_element.h"
#include "context/AppContext.h"
#include <functional>
#include <QMessageBox>
#include <opendaq/instance_ptr.h>
#include <opendaq/custom_log.h>

ComponentTreeWidget::ComponentTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setupUI();
}

ComponentTreeWidget::~ComponentTreeWidget() = default;

void ComponentTreeWidget::loadInstance(const daq::InstancePtr& instance)
{
    // Clear existing tree
    clear();
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
        // Create root element
        rootElement = std::make_unique<DeviceTreeElement>(this, instance.getRootDevice());
        rootElement->init();

        // Expand the root
        if (rootElement->getTreeItem())
            rootElement->getTreeItem()->setExpanded(true);
        
        // Apply visibility filters
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

BaseTreeElement* ComponentTreeWidget::getSelectedElement() const
{
    auto selectedItems = this->selectedItems();
    if (selectedItems.isEmpty())
        return nullptr;

    auto item = selectedItems.first();
    auto elementPtr = item->data(0, Qt::UserRole).value<void*>();
    return static_cast<BaseTreeElement*>(elementPtr);
}

BaseTreeElement* ComponentTreeWidget::findElementByGlobalId(const QString& globalId) const
{
    if (!rootElement)
        return nullptr;

    // Helper lambda to recursively search
    std::function<BaseTreeElement*(BaseTreeElement*)> search = [&](BaseTreeElement* element) -> BaseTreeElement* 
    {
        if (!element)
            return nullptr;

        if (element->getGlobalId() == globalId)
            return element;

        // Search in children
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

void ComponentTreeWidget::setComponentTypeFilter(const QSet<QString>& types)
{
    AppContext::Instance()->setShowComponentTypes(types);
    refreshVisibility();
}

void ComponentTreeWidget::refreshVisibility()
{
    if (rootElement)
        rootElement->showFiltered();
}

void ComponentTreeWidget::setupUI()
{
    setHeaderLabel("Components");
    setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect signals
    connect(this, &QTreeWidget::itemSelectionChanged,
            this, &ComponentTreeWidget::onSelectionChanged);
    connect(this, &QTreeWidget::customContextMenuRequested,
            this, &ComponentTreeWidget::onContextMenuRequested);
}

void ComponentTreeWidget::onSelectionChanged()
{
    auto element = getSelectedElement();
    if (element)
        Q_EMIT componentSelected(element);
}

void ComponentTreeWidget::onContextMenuRequested(const QPoint& pos)
{
    auto item = itemAt(pos);
    if (!item)
        return;

    auto elementPtr = item->data(0, Qt::UserRole).value<void*>();
    if (elementPtr)
    {
        auto element = static_cast<BaseTreeElement*>(elementPtr);
        auto menu = element->onCreateRightClickMenu(this);
        if (menu && menu->actions().count() > 0)
            menu->exec(mapToGlobal(pos));
        delete menu;
    }
}

