#pragma once

#include <QObject>

// Forward declarations to avoid including openDAQ headers in Qt header files
// This prevents conflicts between openDAQ's operator& and Qt's macros
namespace daq {
    class InstancePtr;
}

// Global application context - singleton for accessing openDAQ instance
// from anywhere in the application
class AppContext : public QObject
{
    Q_OBJECT

public:
    // Get singleton instance
    static AppContext* instance();

    // OpenDAQ instance access
    daq::InstancePtr daqInstance() const;
    void setDaqInstance(const daq::InstancePtr& instance);

    // Convenience method to get instance from anywhere
    static daq::InstancePtr daq();

Q_SIGNALS:
    // Emitted when openDAQ instance changes (pass as void* to avoid template in signal)
    void daqInstanceChanged();

private:
    AppContext(QObject* parent = nullptr);
    ~AppContext() override;

    // Disable copy/move
    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;
    AppContext(AppContext&&) = delete;
    AppContext& operator=(AppContext&&) = delete;

    class Private;
    Private* d;
};
