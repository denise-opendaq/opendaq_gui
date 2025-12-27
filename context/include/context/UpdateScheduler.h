#pragma once

#include <QObject>
#include <QTimer>
#include <QPointer>
#include <QList>

// Constants for timer intervals
namespace UpdateSchedulerConstants {
    constexpr int SCHEDULER_INTERVAL_MS = 10;      // OpenDAQ scheduler main loop interval
    constexpr int DEFAULT_UPDATABLES_INTERVAL_MS = 1000;  // Default update interval for widgets
}

// Interface for objects that need periodic updates
// Must inherit from QObject to work with QPointer
class IUpdatable
{
public:
    virtual ~IUpdatable() = default;
    virtual void onScheduledUpdate() = 0;
};

// Global scheduler that manages periodic updates for multiple objects
// Uses a single QTimer to update all registered objects
// Uses QPointer for safe weak references - automatically becomes null when object is deleted
class UpdateScheduler : public QObject
{
    Q_OBJECT

public:
    explicit UpdateScheduler(QObject* parent = nullptr);
    ~UpdateScheduler() override;

    // Register a QObject-based updatable for periodic updates
    // The widget must inherit from both QObject and IUpdatable
    void registerUpdatable(QObject* updatable);

    // Unregister an object
    void unregisterUpdatable(QObject* updatable);

    // Set update interval in milliseconds (default: 1000ms)
    void setInterval(int milliseconds);

    // Get current interval
    int interval() const;

    // Get number of registered objects (includes null pointers)
    int count() const;

private Q_SLOTS:
    void onSchedulerTimeout();
    void onUpdatablesTimeout();

private:
    QTimer* schedulerTimer;  // Runs every 10ms for openDAQ scheduler
    QTimer* timer;           // Runs every second for updatables
    QList<QPointer<QObject>> updatables;
};

