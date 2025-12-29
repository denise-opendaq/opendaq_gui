#include "MainWindow.h"
#include "context/AppContext.h"
#include <QApplication>
#include <opendaq/opendaq.h>

using namespace daq;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Create openDAQ instance with our custom logger sink from AppContext
    auto instance = InstanceBuilder().setGlobalLogLevel(daq::LogLevel::Info)
                                     .addLoggerSink(AppContext::Instance()->getLoggerSink())
                                     .addDiscoveryServer("mdns")
                                     .setUsingSchedulerMainLoop(true)
                                     .build();

    // Set it in global context so it's accessible from anywhere
    AppContext::Instance()->setDaqInstance(instance);

    MainWindow window;
    window.show();

    return app.exec();
}
