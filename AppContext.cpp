#include "AppContext.h"
#include "UpdateScheduler.h"
#include <opendaq/opendaq.h>
#include <QStringList>

// PIMPL to hide openDAQ headers from Qt headers
class AppContext::Private
{
public:
    daq::InstancePtr daqInstance;
    bool showInvisible = false;
    QStringList componentTypes; // empty means show all
    QTextEdit* logTextEdit = nullptr;
    UpdateScheduler* scheduler = nullptr;
};

AppContext::AppContext(QObject* parent)
    : QObject(parent)
    , d(new Private())
{
    d->scheduler = new UpdateScheduler(this);
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

void AppContext::setLogTextEdit(QTextEdit* logTextEdit)
{
    if (!d->logTextEdit)
    {
        d->logTextEdit = logTextEdit;
    }
}

void AppContext::addLogMessage(const QString &text)
{
    d->logTextEdit->append(text);
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
    d->showInvisible = show;
}

QStringList AppContext::showComponentTypes() const
{
    return d->componentTypes;
}

void AppContext::setShowComponentTypes(const QStringList& types)
{
    d->componentTypes = types;
}

UpdateScheduler* AppContext::updateScheduler() const
{
    return d->scheduler;
}
