#include "context/QueuedEventHandler.h"
#include "context/AppContext.h"
#include <opendaq/custom_log.h>

// EventQueue implementation

void EventQueue::operator+=(Subscription& sub)
{
    if (!sub)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("EventQueue::operator+= called with invalid delegate");
        return;
    }

    std::lock_guard<std::mutex> lock(listenersMutex);
    listeners.emplace(sub.hashCode, sub);
}

void EventQueue::operator+=(Subscription&& sub)
{
    if (!sub)
    {
        const auto loggerComponent = AppContext::LoggerComponent();
        LOG_W("EventQueue::operator+= called with invalid delegate");
        return;
    }

    std::lock_guard<std::mutex> lock(listenersMutex);
    size_t id = sub.hashCode;
    listeners.emplace(id, std::move(sub));
}

void EventQueue::operator-=(const Subscription& sub)
{
    std::lock_guard<std::mutex> lock(listenersMutex);
    if (auto it = listeners.find(sub.hashCode); it != listeners.end())
        listeners.erase(it);
}

void EventQueue::operator-=(Subscription&& sub)
{
    std::lock_guard<std::mutex> lock(listenersMutex);
    if (auto it = listeners.find(sub.hashCode); it != listeners.end())
        listeners.erase(it);
}

void EventQueue::enqueue(daq::ComponentPtr sender, daq::CoreEventArgsPtr eventArgs)
{
    std::lock_guard<std::mutex> lock(mutex);
    events.push({sender, eventArgs});
}

void EventQueue::dispatchAll()
{
    // Swap the queue to minimize lock time
    std::queue<QueuedEvent> localQueue;
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::swap(localQueue, events);
    }

    // Dispatch all events to all listeners in the main thread
    while (!localQueue.empty())
    {
        auto& event = localQueue.front();

        // Lock only during iteration, not during callback execution
        std::lock_guard<std::mutex> lock(listenersMutex);
        for (const auto& [id, callback] : listeners)
        {
            try
            {
                callback(event.sender, event.eventArgs);
            }
            catch (const std::exception& e)
            {
                const auto loggerComponent = AppContext::LoggerComponent();
                LOG_W("Error dispatching queued event to listener {}: {}", id, e.what());
            }
            catch (...)
            {
                const auto loggerComponent = AppContext::LoggerComponent();
                LOG_W("Unknown error dispatching queued event to listener {}", id);
            }
        }

        localQueue.pop();
    }
}

size_t EventQueue::size() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return events.size();
}
