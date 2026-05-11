#pragma once

#include <QWidget>
#include <atomic>

#include <opendaq/device_ptr.h>
#include <coreobjects/core_event_args_ptr.h>

class QLabel;
class QTabWidget;
class QTimer;
class QComboBox;

class DeviceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceWidget(const daq::DevicePtr& device, QWidget* parent = nullptr);
    ~DeviceWidget() override;

private:
    void setupUI();
    QWidget* buildHeaderCard();
    void populateTabs();

    void updateStatus();
    void updateOpModeCombo();
    void updateConnectionStatus();
    void updateTags();
    void updateCurrentTime();

    QString formatFrequency(double hz) const;
    QString computeCurrentTime() const;

    bool eventFilter(QObject* obj, QEvent* event) override;
    void onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args);
    void onOpModeChanged(int index);

    daq::DevicePtr device;
    std::atomic<int> updatingFromDevice{0};

    // Header widgets
    QLabel*   nameLabel      = nullptr;
    QLabel*   descLabel      = nullptr;
    QLabel*   statusLabel    = nullptr;
    QComboBox* opModeCombo   = nullptr;
    QWidget*  connectionStatusBlock = nullptr;
    QWidget*  tagsRow               = nullptr;

    QLabel*   domainLabel    = nullptr;
    QLabel*   tickFreqLabel  = nullptr;
    QLabel*   currentTimeLbl = nullptr;

    QTabWidget* tabs  = nullptr;
    QTimer*     timer = nullptr;
};
