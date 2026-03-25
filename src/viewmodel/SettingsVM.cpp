#include "SettingsVM.h"
#include "system/SystemChecker.h"

namespace BlueDrop {

SettingsVM::SettingsVM(const SystemCheckResult& result, QObject* parent)
    : QObject(parent)
    , m_osVersion(result.osVersionString)
    , m_osVersionOk(result.osVersionOk)
    , m_bluetoothAvailable(result.bluetoothAvailable)
    , m_bluetoothAdapterName(result.bluetoothAdapterName)
    , m_vbCableInstalled(result.vbCableInstalled)
    , m_vbCableDeviceName(result.vbCableDeviceName)
{
}

} // namespace BlueDrop
