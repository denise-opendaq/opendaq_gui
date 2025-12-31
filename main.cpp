#include <QApplication>
#include <QDir>

#include "MainWindow.h"
#include "context/AppContext.h"

#include <opendaq/instance_factory.h>
#include <opendaq/logger_sink_ptr.h>
#include <opendaq/log_level.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Create openDAQ instance with our custom logger sink from AppContext
    auto builder = daq::InstanceBuilder().setGlobalLogLevel(daq::LogLevel::Info)
                                         .addLoggerSink(AppContext::Instance()->getLoggerSink())
                                         .addLoggerSink(daq::StdOutLoggerSink())
                                         .addDiscoveryServer("mdns")
                                         .setUsingSchedulerMainLoop(true);

    // Add extra module path if defined (for bundle packaging)
#ifdef EXTRA_MODULE_PATH
    // Build absolute path relative to application directory
    QString appDir = QCoreApplication::applicationDirPath();
    QString modulePath = QDir(appDir).filePath(EXTRA_MODULE_PATH);
    builder.addModulePath(modulePath.toStdString());
#endif

    auto instance = builder.build();

    // Set it in global context so it's accessible from anywhere
    AppContext::Instance()->setDaqInstance(instance);

    MainWindow window;
    window.show();

    return app.exec();
}
