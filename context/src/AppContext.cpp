#include "context/AppContext.h"
#include "context/UpdateScheduler.h"
#include "context/QueuedEventHandler.h"
#include "logger/qt_text_edit_sink.h"

#include <opendaq/opendaq.h>
#include <opendaq/custom_log.h>
#include <QSet>
#include <memory>

// PIMPL to hide openDAQ headers from Qt headers
class AppContext::Private
{
public:
    daq::InstancePtr daqInstance;
    bool showInvisible = false;
    QSet<QString> componentTypes; // empty means show all
    daq::LoggerSinkPtr loggerSink;
    UpdateScheduler* scheduler = nullptr;
    EventQueue eventQueue;
};

AppContext::AppContext(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->scheduler = new UpdateScheduler(this);
    d->loggerSink = createQTextEditLoggerSink();
}

AppContext::~AppContext() = default;

AppContext* AppContext::Instance()
{
    static AppContext* s_instance = new AppContext();
    return s_instance;
}

daq::InstancePtr AppContext::daqInstance() const
{
    return d->daqInstance;
}

void AppContext::setDaqInstance(const daq::InstancePtr& instance)
{
    if (!d->daqInstance.assigned())
    {
        d->daqInstance = instance;

        // Subscribe to context core events once
        try
        {
            auto context = instance.getContext();
            context.getOnCoreEvent() += std::bind(&EventQueue::enqueue, &d->eventQueue, std::placeholders::_1, std::placeholders::_2);
        }
        catch (const std::exception& e)
        {
            const auto loggerComponent = LoggerComponent();
            LOG_W("Failed to subscribe to context core events: {}", e.what());
        }

        Q_EMIT daqInstanceChanged();
    }
}

daq::LoggerSinkPtr AppContext::getLoggerSink() const
{
    return d->loggerSink;
}

QTextEdit* AppContext::getLogTextEdit() const
{
    if (!d->loggerSink.assigned())
        return nullptr;

    auto qtSink = d->loggerSink.asPtr<IQTextEditLoggerSink>(true);
    QTextEdit* textEdit = nullptr;
    qtSink->getTextEdit(&textEdit);
    return textEdit;
}

daq::InstancePtr AppContext::Daq()
{
    return Instance()->daqInstance();
}

daq::LoggerComponentPtr AppContext::LoggerComponent()
{
    return Daq().getContext().getLogger().getOrAddComponent("openDAQ GUI");
}

bool AppContext::showInvisibleComponents() const
{
    return d->showInvisible;
}

void AppContext::setShowInvisibleComponents(bool show)
{
    if (d->showInvisible != show)
    {
        d->showInvisible = show;
        Q_EMIT showInvisibleChanged(show);
    }
}

QSet<QString> AppContext::showComponentTypes() const
{
    return d->componentTypes;
}

void AppContext::setShowComponentTypes(const QSet<QString>& types)
{
    d->componentTypes = types;
}

UpdateScheduler* AppContext::updateScheduler() const
{
    return d->scheduler;
}

EventQueue* AppContext::eventQueue() const
{
    return &d->eventQueue;
}

EventQueue* AppContext::DaqEvent()
{
    return Instance()->eventQueue();
}