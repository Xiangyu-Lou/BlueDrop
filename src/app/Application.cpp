#include "Application.h"

#include "bluetooth/BluetoothManager.h"
#include "audio/DeviceEnumerator.h"
#include "audio/AudioEngine.h"
#include "system/SystemChecker.h"

namespace BlueDrop {

Application::Application(QObject* parent)
    : QObject(parent)
{
}

Application::~Application()
{
    if (m_audioEngine) m_audioEngine->stop();
    if (m_bluetooth) m_bluetooth->stopListening();
}

bool Application::initialize()
{
    // Step 1: System checks
    m_checkResult = SystemChecker::runAllChecks();
    if (!m_checkResult.osVersionOk) {
        emit initializationFailed(m_checkResult.failureMessage);
        return false;
    }

    // Step 2: Device enumerator
    m_deviceEnumerator = std::make_unique<DeviceEnumerator>(this);
    if (!m_deviceEnumerator->initialize()) {
        emit initializationFailed(
            QString::fromUtf8(u8"无法初始化音频设备枚举器"));
        return false;
    }

    // Step 3: Bluetooth manager
    m_bluetooth = std::make_unique<BluetoothManager>(this);

    // Step 4: Audio engine
    m_audioEngine = std::make_unique<AudioEngine>(this);

    return true;
}

SystemCheckResult Application::checkResult() const
{
    return m_checkResult;
}

} // namespace BlueDrop
