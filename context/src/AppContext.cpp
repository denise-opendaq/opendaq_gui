#include "context/AppContext.h"
#include "context/UpdateScheduler.h"
#include "logger/qt_text_edit_sink.h"

#include <opendaq/opendaq.h>
#include <QSet>

// PIMPL to hide openDAQ headers from Qt headers
class AppContext::Private
{
public:
    daq::InstancePtr daqInstance;
    bool showInvisible = false;
    QSet<QString> componentTypes; // empty means show all
    daq::LoggerSinkPtr loggerSink;
    UpdateScheduler* scheduler = nullptr;
};

AppContext::AppContext(QObject* parent)
    : QObject(parent)
    , d(new Private())
{
    d->scheduler = new UpdateScheduler(this);
    d->loggerSink = createQTextEditLoggerSink();
}

AppContext::~AppContext()
{
    delete d;
}

AppContext* AppContext::instance()
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

daq::InstancePtr AppContext::daq()
{
    return instance()->daqInstance();
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

