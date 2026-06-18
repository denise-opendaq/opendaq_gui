#pragma once

#include <QWidget>

#include <opendaq/device_ptr.h>

class QVBoxLayout;

class SynchronizationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SynchronizationWidget(const daq::DevicePtr& device, QWidget* parent = nullptr);

private:
    void buildSourceRow(QVBoxLayout* layout);

    daq::DevicePtr device;
};
