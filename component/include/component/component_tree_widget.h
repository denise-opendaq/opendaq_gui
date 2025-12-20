#pragma once
#include <QTreeWidget>
#include "component_factory.h"
#include "base_tree_element.h"
#include "AppContext.h"
#include <opendaq/opendaq.h>

// Main widget that wraps QTreeWidget for openDAQ components
class ComponentTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    ComponentTreeWidget(QWidget* parent = nullptr)
        : QTreeWidget(parent)
        , rootElement(nullptr)
    {
        setupUI();
    }

    ~ComponentTreeWidget() override
    {
        if (rootElement)
        {
            delete rootElement;
            rootElement = nullptr;
        }
    }

    // Load openDAQ instance into the tree
    void loadInstance(const daq::InstancePtr& instance)
    {
        // Clear existing tree
        clear();
        if (rootElement)
        {
            delete rootElement;
            rootElement = nullptr;
        }

        if (!instance.assigned())
        {
            qWarning() << "Cannot load null instance";
            return;
        }

        try
        {
            // Create root element
            rootElement = new DeviceTreeElement(this, instance.getRootDevice());
            rootElement->init();

            // Expand the root
            if (rootElement->getTreeItem())
            {
                rootElement->getTreeItem()->setExpanded(true);
            }
        }
        catch (const std::exception& e)
        {
            qWarning() << "Error loading instance:" << e.what();
        }
    }

    // Get the selected BaseTreeElement
    BaseTreeElement* getSelectedElement() const
    {
        auto selectedItems = this->selectedItems();
        if (selectedItems.isEmpty())
            return nullptr;

        auto item = selectedItems.first();
        auto elementPtr = item->data(0, Qt::UserRole).value<void*>();
        return static_cast<BaseTreeElement*>(elementPtr);
    }

    // Set whether to show hidden components
    void setShowHidden(bool show)
    {
        AppContext::instance()->setShowInvisibleComponents(show);
        refreshVisibility();
    }

    // Set component type filter
    void setComponentTypeFilter(const QStringList& types)
    {
        AppContext::instance()->setShowComponentTypes(types);
        refreshVisibility();
    }

    // Refresh visibility based on current filters
    void refreshVisibility()
    {
        if (rootElement)
        {
            rootElement->showFiltered();
        }
    }

Q_SIGNALS:
    // Emitted when a component is selected in the tree
    void componentSelected(BaseTreeElement* element);

private:
    void setupUI()
    {
        setHeaderLabel("Components");
        setContextMenuPolicy(Qt::CustomContextMenu);

        // Connect signals
        connect(this, &QTreeWidget::itemSelectionChanged,
                this, &ComponentTreeWidget::onSelectionChanged);
        connect(this, &QTreeWidget::customContextMenuRequested,
                this, &ComponentTreeWidget::onContextMenuRequested);
    }

private Q_SLOTS:
    void onSelectionChanged()
    {
        auto element = getSelectedElement();
        if (element)
        {
            Q_EMIT componentSelected(element);
        }
    }

    void onContextMenuRequested(const QPoint& pos)
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
            {
                menu->exec(mapToGlobal(pos));
            }
            delete menu;
        }
    }

private:
    BaseTreeElement* rootElement;
};
