#include <QApplication>
#include <QDir>
#include <QSysInfo>

#include "MainWindow.h"
#include "context/AppContext.h"

#include <opendaq/instance_factory.h>
#include <opendaq/logger_sink_ptr.h>
#include <opendaq/log_level.h>
#include <opendaq/device_info_factory.h>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Generate unique device info
    QString manufacturer = QString("openDAQ-GUI-%1").arg(QSysInfo::machineHostName());

    // Get process ID for unique serial number
#ifdef Q_OS_WIN
    qint64 pid = GetCurrentProcessId();
#else
    qint64 pid = getpid();
#endif
    QString serialNumber = QString::number(pid);

    auto deviceInfo = daq::DeviceInfo("daqmock://client_device", "OpenDAQClient");
    deviceInfo.setManufacturer(manufacturer.toStdString());
    deviceInfo.setSerialNumber(serialNumber.toStdString());

    // Create openDAQ instance with our custom logger sink from AppContext
    auto builder = daq::InstanceBuilder().setGlobalLogLevel(daq::LogLevel::Info)
                                         .addLoggerSink(AppContext::Instance()->getLoggerSink())
                                         .addLoggerSink(daq::StdOutLoggerSink())
                                         .addDiscoveryServer("mdns")
                                         .setDefaultRootDeviceInfo(deviceInfo)
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
