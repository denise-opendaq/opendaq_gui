#include "context/UpdateScheduler.h"
#include "context/AppContext.h"

// Save Qt keywords and undefine them before including openDAQ
#ifdef signals
#pragma push_macro("signals")
#pragma push_macro("slots")
#pragma push_macro("emit")
#undef signals
#undef slots
#undef emit
#define QT_MACROS_PUSHED
#endif

#include <opendaq/opendaq.h>

#ifdef QT_MACROS_PUSHED
#pragma pop_macro("emit")
#pragma pop_macro("slots")
#pragma pop_macro("signals")
#undef QT_MACROS_PUSHED
#endif

UpdateScheduler::UpdateScheduler(QObject* parent)
    : QObject(parent)
    , timer(new QTimer(this))
{
    connect(timer, &QTimer::timeout, this, &UpdateScheduler::onTimeout);
    timer->setInterval(1000); // Default: update every second
}

UpdateScheduler::~UpdateScheduler()
{
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

    // Start timer if this is the first registered object
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

    // Stop timer if no more objects to update
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

void UpdateScheduler::onTimeout()
{
    // Run openDAQ scheduler main loop iteration to process events
    try {
        auto instance = AppContext::instance()->daqInstance();
        if (instance.assigned()) {
            instance.getContext().getScheduler().runMainLoopIteration();
        }
    } catch (const std::exception&) {
        // Ignore exceptions from scheduler
    }

    // Iterate through all registered objects
    // QPointer automatically becomes null if the object was deleted
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

