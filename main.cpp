#include <QApplication>
#include <QDir>
#include <QSysInfo>

#include "MainWindow.h"
#include "context/AppContext.h"
#include "context/gui_constants.h"

#include <opendaq/instance_factory.h>
#include <opendaq/logger_sink_ptr.h>
#include <opendaq/log_level.h>
#include <opendaq/device_info_factory.h>


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Disable all UI animations globally to prevent crashes during resize/zoom on macOS
    // This is a workaround for Qt bug where animations can access invalid memory
    app.setEffectEnabled(Qt::UI_AnimateMenu, false);
    app.setEffectEnabled(Qt::UI_AnimateCombo, false);
    app.setEffectEnabled(Qt::UI_AnimateTooltip, false);
    app.setEffectEnabled(Qt::UI_AnimateToolBox, false);

    auto deviceInfo = daq::DeviceInfo("daqmock://client_device", "OpenDAQClient");
    deviceInfo.setManufacturer(GUIConstants::getClientManufacturer().toStdString());
    deviceInfo.setSerialNumber(GUIConstants::getClientSerialNumber().toStdString());

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
