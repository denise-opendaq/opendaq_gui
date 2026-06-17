#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class BaseTreeElement;
class AppContext;

namespace daq
{
    class InstancePtr;
}

// Sidebar widget: search field + component tree.
class ComponentTreeWidget : public QWidget
{
    Q_OBJECT

public:
    ComponentTreeWidget(QWidget* contentHost, QWidget* parent = nullptr);
    ~ComponentTreeWidget() override;

    QTreeWidget* treeWidget() const { return m_tree; }

    static ComponentTreeWidget* fromTreeWidget(QTreeWidget* tree);

    void loadInstance(const daq::InstancePtr& instance);

    void setContentHost(QWidget* contentHost);

    BaseTreeElement* getSelectedElement() const;

    BaseTreeElement* findElementByGlobalId(const QString& globalId) const;

    void setShowHidden(bool show);

    void setSearchFilter(const QString& text);

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

    QLineEdit* m_searchBox = nullptr;
    QTreeWidget* m_tree = nullptr;
    std::unique_ptr<BaseTreeElement> rootElement;
    QWidget* contentHost = nullptr;
    QString m_searchFilter;
};
