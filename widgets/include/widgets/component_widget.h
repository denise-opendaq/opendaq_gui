#pragma once

#include <QWidget>

#include <opendaq/component_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class QFrame;
class QLabel;
class QTabWidget;

class ComponentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ComponentWidget(const daq::ComponentPtr& component, QWidget* parent = nullptr);
    ~ComponentWidget() override;

protected:
    ComponentWidget(const daq::ComponentPtr& component, QWidget* parent, Qt::Initialization);

    void setupUI();
    virtual QWidget* buildHeaderCard();
    virtual void populateTabs();

    void updateStatus();
    void updateStatusContainer();
    void updateTags();

    bool eventFilter(QObject* obj, QEvent* event) override;
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);

    daq::ComponentPtr component;

    QLabel*     nameLabel            = nullptr;
    QLabel*     descLabel            = nullptr;
    QLabel*     statusLabel          = nullptr;
    QFrame*     statusSep            = nullptr;
    QWidget*    statusContainerBlock = nullptr;
    QWidget*    tagsRow              = nullptr;
    QTabWidget* tabs                 = nullptr;
};
