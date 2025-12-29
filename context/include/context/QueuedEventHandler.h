#pragma once

#include <opendaq/opendaq.h>
#include <functional>
#include <queue>
#include <mutex>
#include <map>

// Global event queue manager (singleton)
// Manages a queue of events that need to be dispatched in the main thread
class EventQueue
{
public:
    using Subscription = delegate<void(daq::ComponentPtr&, daq::CoreEventArgsPtr&)>;
    EventQueue() = default;
    ~EventQueue() = default;

    // Operator+= for adding listeners using delegate (similar to openDAQ Event)
    void operator+=(Subscription& sub);
    void operator+=(Subscription&& sub);

    void operator-=(const Subscription& sub);
    void operator-=(Subscription&& sub);

    // Enqueue an event for later dispatch (thread-safe)
    void enqueue(daq::ComponentPtr sender, daq::CoreEventArgsPtr eventArgs);

    // Dispatch all pending events (must be called from main thread)
    void dispatchAll();

    // Get number of pending events
    size_t size() const;

private:

    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    struct QueuedEvent
    {
        daq::ComponentPtr sender;
        daq::CoreEventArgsPtr eventArgs;
    };

    mutable std::mutex mutex;
    std::queue<QueuedEvent> events;

    std::mutex listenersMutex;
    std::map<size_t, Subscription> listeners; // Map of hashCode -> callback
};
