#pragma once
#include <QTreeWidget>
#include <QString>
#include <memory>

class BaseTreeElement;
class AppContext;
class QTabWidget;

namespace daq
{
    class InstancePtr;
}

// Main widget that wraps QTreeWidget for openDAQ components
class ComponentTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    ComponentTreeWidget(QTabWidget* contentTabs, QWidget* parent = nullptr);
    ~ComponentTreeWidget() override;

    // Load openDAQ instance into the tree
    void loadInstance(const daq::InstancePtr& instance);

    // Set content tabs (for elements created after construction)
    void setContentTabs(QTabWidget* contentTabs);

    // Get the selected BaseTreeElement
    BaseTreeElement* getSelectedElement() const;

    // Find element by globalId
    BaseTreeElement* findElementByGlobalId(const QString& globalId) const;

    // Set whether to show hidden components
    void setShowHidden(bool show);

    // Set text search filter (empty string clears search)
    void setSearchFilter(const QString& text);

    // Refresh visibility based on current filters
    void refreshVisibility();

Q_SIGNALS:
    void componentSelected(BaseTreeElement* element);

private:
    void setupUI();

private Q_SLOTS:
    void onSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);

private:
    bool applySearchFilter(QTreeWidgetItem* item, const QString& filter);

    std::unique_ptr<BaseTreeElement> rootElement;
    QTabWidget* contentTabs = nullptr;
    QString m_searchFilter;
};
