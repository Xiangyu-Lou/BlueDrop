#include "SettingsVM.h"
#include "system/SystemChecker.h"
#include <QSettings>

namespace BlueDrop {

SettingsVM::SettingsVM(const SystemCheckResult& result, QObject* parent)
    : QObject(parent)
    , m_osVersion(result.osVersionString)
    , m_osVersionOk(result.osVersionOk)
    , m_bluetoothAvailable(result.bluetoothAvailable)
    , m_bluetoothAdapterName(result.bluetoothAdapterName)
    , m_vbCableInstalled(result.vbCableInstalled)
    , m_vbCableDeviceName(result.vbCableDeviceName)
    , m_closeAction(QSettings().value("behavior/closeAction", "").toString())
{
}

bool SettingsVM::consumeFirstLaunch()
{
    QSettings s;
    if (s.value("behavior/firstLaunchDone", false).toBool())
        return false;
    s.setValue("behavior/firstLaunchDone", true);
    return true;
}

void SettingsVM::setCloseAction(const QString& action)
{
    if (m_closeAction == action) return;
    m_closeAction = action;
    QSettings().setValue("behavior/closeAction", action);
    emit closeActionChanged();
}

} // namespace BlueDrop
