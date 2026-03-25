#pragma once

#include <QString>

namespace BlueDrop {

struct SystemCheckResult {
    bool osVersionOk = false;
    bool bluetoothAvailable = false;
    bool vbCableInstalled = false;

    QString osVersionString;
    uint32_t osBuildNumber = 0;

    QString bluetoothAdapterName;
    QString vbCableDeviceName;

    // First failure message for UI display
    QString failureMessage;

    bool allPassed() const {
        return osVersionOk && bluetoothAvailable && vbCableInstalled;
    }
};

class SystemChecker {
public:
    static SystemCheckResult runAllChecks();

    static bool checkOsVersion(SystemCheckResult& result);
    static bool checkBluetoothAdapter(SystemCheckResult& result);
    static bool checkVBCable(SystemCheckResult& result);

private:
    static constexpr uint32_t MIN_BUILD_NUMBER = 19041; // Win10 2004
};

} // namespace BlueDrop
