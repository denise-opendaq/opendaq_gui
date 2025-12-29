#pragma once

#include <QObject>
#include <QTextEdit>
#include <QSet>
#include <QString>
#include <memory>

// Forward declarations to avoid including openDAQ headers in Qt header files
// This prevents conflicts between openDAQ's operator& and Qt's macros
namespace daq {
    class InstancePtr;
    class SinkPtr;
    class LoggerSinkPtr;
    class LoggerComponentPtr;
}

class UpdateScheduler;
class EventQueue;

// Global application context - singleton for accessing openDAQ instance
// from anywhere in the application
class AppContext : public QObject
{
    Q_OBJECT

public:
    // Get singleton instance
    static AppContext* Instance();

    // OpenDAQ instance access
    daq::InstancePtr daqInstance() const;
    void setDaqInstance(const daq::InstancePtr& instance);

    // Logger sink access
    daq::LoggerSinkPtr getLoggerSink() const;
    QTextEdit* getLogTextEdit() const;

    // Convenience method to get instance from anywhere
    static daq::InstancePtr Daq();

    // Logger component helper for easy logging
    static daq::LoggerComponentPtr LoggerComponent();

    // Visibility settings
    bool showInvisibleComponents() const;
    void setShowInvisibleComponents(bool show);

    QSet<QString> showComponentTypes() const;
    void setShowComponentTypes(const QSet<QString>& types);

    // Update scheduler for periodic widget updates
    UpdateScheduler* updateScheduler() const;

    EventQueue* eventQueue() const;
    static EventQueue* DaqEvent();

Q_SIGNALS:
    // Emitted when openDAQ instance changes (pass as void* to avoid template in signal)
    void daqInstanceChanged();

    // Emitted when showInvisible setting changes
    void showInvisibleChanged(bool show);

private:
    AppContext(QObject* parent = nullptr);
    ~AppContext() override;

    // Disable copy/move
    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;
    AppContext(AppContext&&) = delete;
    AppContext& operator=(const AppContext&&) = delete;

    class Private;
    std::unique_ptr<Private> d;
};