#include "context/gui_constants.h"
#include <QSysInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace GUIConstants {

const QString& getClientManufacturer()
{
    static const QString value = QString("openDAQ-GUI-%1").arg(QSysInfo::machineHostName());
    return value;
}

const QString& getClientSerialNumber()
{
#ifdef Q_OS_WIN
    static const QString value = QString::number(GetCurrentProcessId());
#else
    static const QString value = QString::number(getpid());
#endif
    return value;
}

} // namespace GUIConstants
