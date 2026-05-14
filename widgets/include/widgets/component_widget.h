#pragma once

#include <QString>
#include <QWidget>

#include <opendaq/component_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class QFrame;
class QHBoxLayout;
class QLabel;
class QTabWidget;

class ComponentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ComponentWidget(const daq::ComponentPtr& component,
                             QWidget* parent = nullptr,
                             const QString& treeIconName = {});
    ~ComponentWidget() override;

protected:
    ComponentWidget(const daq::ComponentPtr& component,
                      QWidget* parent,
                      Qt::Initialization,
                      const QString& treeIconName = {});

    void setupUI();
    virtual QWidget* buildHeaderCard();
    virtual void populateTabs();

    // Prepends the 64×64 tree icon column (uses treeIconName + IconProvider).
    void addTreeIconToHeaderLayout(QHBoxLayout* cardLayout, QWidget* card);

    void updateStatus();
    void updateStatusContainer();
    void updateTags();

    bool eventFilter(QObject* obj, QEvent* event) override;
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

    daq::ComponentPtr component;
    QString           treeIconName;

    QLabel*     nameLabel            = nullptr;
    QLabel*     descLabel            = nullptr;
    QLabel*     statusLabel          = nullptr;
    QFrame*     statusSep            = nullptr;
    QWidget*    statusContainerBlock = nullptr;
    QWidget*    tagsRow              = nullptr;
    QTabWidget* tabs                 = nullptr;
};
