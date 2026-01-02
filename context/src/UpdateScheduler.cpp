#include "context/UpdateScheduler.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"
#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>
#include <algorithm>

UpdateScheduler::UpdateScheduler(QObject* parent)
    : QObject(parent)
    , schedulerTimer(new QTimer(this))
    , timer(new QTimer(this))
    , eventQueueTimer(new QTimer(this))
{
    // Scheduler timer runs every 10ms for openDAQ main loop
    // Must run in main thread - timer is created in main thread so this is guaranteed
    connect(schedulerTimer, &QTimer::timeout, this, &UpdateScheduler::onSchedulerTimeout);
    schedulerTimer->setInterval(UpdateSchedulerConstants::SCHEDULER_INTERVAL_MS);
    schedulerTimer->start(); // Always running for openDAQ scheduler

    // Updatables timer runs every second
    connect(timer, &QTimer::timeout, this, &UpdateScheduler::onUpdatablesTimeout);
    timer->setInterval(UpdateSchedulerConstants::DEFAULT_UPDATABLES_INTERVAL_MS);

    // Event queue timer runs every 10ms to dispatch queued events
    connect(eventQueueTimer, &QTimer::timeout, this, &UpdateScheduler::onEventQueueTimeout);
    eventQueueTimer->setInterval(UpdateSchedulerConstants::EVENT_QUEUE_INTERVAL_MS);
    eventQueueTimer->start(); // Always running to process queued events
}

UpdateScheduler::~UpdateScheduler()
{
    schedulerTimer->stop();
    timer->stop();
    eventQueueTimer->stop();
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
        timer->start();
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
        timer->stop();
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
    try 
    {
        auto instance = AppContext::Instance()->daqInstance();
        if (instance.assigned())
            instance.getContext().getScheduler().runMainLoopIteration();
    } 
    catch (const std::exception& e) 
    {
        // Log but don't propagate exceptions from scheduler
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Error in scheduler main loop iteration: {}", e.what());
    } 
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_D("Unknown error in scheduler main loop iteration");
    }
}

void UpdateScheduler::onUpdatablesTimeout()
{
    // Use erase-remove idiom for efficient removal of null pointers
    // QPointer automatically becomes null if the object was deleted
    // Called every second
    updatables.erase(
        std::remove_if(updatables.begin(), updatables.end(), [](QPointer<QObject>& ptr)
        {
            if (ptr.isNull())
                return true; // Remove null pointers

            // Try to cast to IUpdatable and call update
            IUpdatable* updatable = dynamic_cast<IUpdatable*>(ptr.data());
            if (updatable)
            {
                try
                {
                    updatable->onScheduledUpdate();
                }
                catch (const std::exception& e)
                {
                    // Log but don't propagate exceptions from individual updates
                    // to prevent one widget from breaking others
                    const auto loggerComponent = AppContext::LoggerComponent();
                    LOG_W("Error in scheduled update: {}", e.what());
                }
                catch (...)
                {
                    // Catch any other exceptions
                    const auto loggerComponent = AppContext::LoggerComponent();
                    LOG_W("Unknown error in scheduled update");
                }
            }
            return false; // Keep non-null pointers
        }),
        updatables.end()
    );

    // Stop timer if all objects were deleted
    if (updatables.isEmpty())
        timer->stop();
}

void UpdateScheduler::onEventQueueTimeout()
{
    // Dispatch all queued events in the main thread
    // Called every 10ms - MUST run in main thread
    try
    {
        AppContext::Instance()->eventQueue()->dispatchAll();
    }
    catch (const std::exception& e)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Error dispatching event queue: {}", e.what());
    }
    catch (...)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("Unknown error dispatching event queue");
    }
}

