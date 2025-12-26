#include "context/UpdateScheduler.h"
#include "context/AppContext.h"

#include <opendaq/opendaq.h>

UpdateScheduler::UpdateScheduler(QObject* parent)
    : QObject(parent)
    , schedulerTimer(new QTimer(this))
    , timer(new QTimer(this))
{
    // Scheduler timer runs every 10ms for openDAQ main loop
    // Must run in main thread - timer is created in main thread so this is guaranteed
    connect(schedulerTimer, &QTimer::timeout, this, &UpdateScheduler::onSchedulerTimeout);
    schedulerTimer->setInterval(10);
    schedulerTimer->start(); // Always running for openDAQ scheduler
    
    // Updatables timer runs every second
    connect(timer, &QTimer::timeout, this, &UpdateScheduler::onUpdatablesTimeout);
    timer->setInterval(1000); // Default: update every second
}

UpdateScheduler::~UpdateScheduler()
{
    schedulerTimer->stop();
    timer->stop();
}

void UpdateScheduler::registerUpdatable(QObject* updatable)
{
    if (!updatable)
        return;

    // Check if already registered
    for (const auto& ptr : updatables)
    {
        if (ptr.data() == updatable)
            return;
    }

    updatables.append(QPointer<QObject>(updatable));

    // Start updatables timer if this is the first registered object
    if (updatables.size() == 1)
    {
        timer->start();
    }
}

void UpdateScheduler::unregisterUpdatable(QObject* updatable)
{
    if (!updatable)
        return;

    // Remove the object from the list
    updatables.removeAll(QPointer<QObject>(updatable));

    // Clean up null pointers while we're at it
    updatables.removeAll(QPointer<QObject>());

    // Stop updatables timer if no more objects to update
    if (updatables.isEmpty())
    {
        timer->stop();
    }
}

void UpdateScheduler::setInterval(int milliseconds)
{
    timer->setInterval(milliseconds);
}

int UpdateScheduler::interval() const
{
    return timer->interval();
}

int UpdateScheduler::count() const
{
    return updatables.size();
}

void UpdateScheduler::onSchedulerTimeout()
{
    // Run openDAQ scheduler main loop iteration to process events
    // Called every 10ms - MUST run in main thread
    try {
        auto instance = AppContext::instance()->daqInstance();
        if (instance.assigned()) {
            instance.getContext().getScheduler().runMainLoopIteration();
        }
    } catch (const std::exception&) {
        // Ignore exceptions from scheduler
    }
}

void UpdateScheduler::onUpdatablesTimeout()
{
    // Iterate through all registered objects
    // QPointer automatically becomes null if the object was deleted
    // Called every second
    for (int i = updatables.size() - 1; i >= 0; --i)
    {
        QPointer<QObject> ptr = updatables[i];

        // Remove null pointers (deleted objects)
        if (ptr.isNull())
        {
            updatables.removeAt(i);
            continue;
        }

        // Try to cast to IUpdatable and call update
        IUpdatable* updatable = dynamic_cast<IUpdatable*>(ptr.data());
        if (updatable)
        {
            try
            {
                updatable->onScheduledUpdate();
            }
            catch (...)
            {
                // Ignore exceptions from individual updates
                // to prevent one widget from breaking others
            }
        }
    }

    // Stop timer if all objects were deleted
    if (updatables.isEmpty())
    {
        timer->stop();
    }
}

