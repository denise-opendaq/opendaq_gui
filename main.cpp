#include "MainWindow.h"
#include "AppContext.h"
#include <QApplication>
#include <opendaq/opendaq.h>

using namespace daq;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Create openDAQ instance
    auto instance = InstanceBuilder().setModulePath("/Users/deniserokhin/projects/qt_gui/build/bin").build();
    instance.addProperty(daq::DictProperty("dict", daq::Dict<daq::IString, daq::IInteger>({{"123", 1}, {"test", 123}})));
    instance.addProperty(daq::ListProperty("list", daq::List<IBaseObject>(1, "test")));
    instance.addDevice("daq://SonyUK_Denis");

    // Set it in global context so it's accessible from anywhere
    AppContext::instance()->setDaqInstance(instance);

    MainWindow window;
    window.show();

    return app.exec();
}
