#include "MainWindow.h"
#include "context/AppContext.h"
#include <QApplication>
#include <opendaq/opendaq.h>

// Initialize resources from component library
// Resources in static libraries need explicit initialization in Qt6
// The function is defined in component library's qrc_icons.cpp
// We need to declare it with proper namespace handling
#ifdef QT_NAMESPACE
namespace QT_NAMESPACE {
#endif
    extern int qInitResources_icons();
#ifdef QT_NAMESPACE
}
using namespace QT_NAMESPACE;
#endif

using namespace daq;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Initialize component resources (icons)
    // This is required because resources in static libraries are not automatically initialized
    qInitResources_icons();

    // Create openDAQ instance
    auto instance = InstanceBuilder().setGlobalLogLevel(daq::LogLevel::Info)
                                                       .setModulePath("/Users/deniserokhin/projects/qt_gui/build/bin")
                                                       .setUsingSchedulerMainLoop(true)
                                                       .build();

    // Set it in global context so it's accessible from anywhere
    AppContext::instance()->setDaqInstance(instance);

    MainWindow window;
    window.show();

    return app.exec();
}
