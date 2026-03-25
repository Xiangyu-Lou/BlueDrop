#include "SessionVolumeController.h"
#include "system/Logger.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

namespace BlueDrop {

SessionVolumeController::SessionVolumeController(QObject* parent)
    : QObject(parent)
{
}

SessionVolumeController::~SessionVolumeController() = default;

bool SessionVolumeController::findBluetoothSession(const QString& endpointId,
                                                    const QString& connectedDeviceName)
{
    // Release any previous session
    m_sessionVolume.Reset();
    m_sessionName.clear();
    m_sessionPid = 0;

    if (endpointId.isEmpty()) {
        LOG_WARN("SessionVolumeController: empty endpointId");
        return false;
    }

    // Get the device enumerator
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: CoCreateInstance failed: 0x%08X", hr);
        return false;
    }

    // Open the specific endpoint
    auto wideId = endpointId.toStdWString();
    ComPtr<IMMDevice> device;
    hr = enumerator->GetDevice(wideId.c_str(), device.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: GetDevice failed: 0x%08X", hr);
        return false;
    }

    // Get session manager
    ComPtr<IAudioSessionManager2> sessionMgr;
    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                          nullptr, reinterpret_cast<void**>(sessionMgr.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: Activate IAudioSessionManager2 failed: 0x%08X", hr);
        return false;
    }

    // Enumerate all sessions
    ComPtr<IAudioSessionEnumerator> sessionEnum;
    hr = sessionMgr->GetSessionEnumerator(sessionEnum.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: GetSessionEnumerator failed: 0x%08X", hr);
        return false;
    }

    int sessionCount = 0;
    sessionEnum->GetCount(&sessionCount);
    LOG_INFOF("SessionVolumeController: found %d sessions on endpoint", sessionCount);

    // Search for BT audio session
    for (int i = 0; i < sessionCount; i++) {
        ComPtr<IAudioSessionControl> sessionCtrl;
        hr = sessionEnum->GetSession(i, sessionCtrl.GetAddressOf());
        if (FAILED(hr)) continue;

        // Get extended control for process info
        ComPtr<IAudioSessionControl2> sessionCtrl2;
        hr = sessionCtrl.As(&sessionCtrl2);
        if (FAILED(hr)) continue;

        // Get process ID
        DWORD pid = 0;
        sessionCtrl2->GetProcessId(&pid);

        // Get display name
        LPWSTR displayName = nullptr;
        sessionCtrl->GetDisplayName(&displayName);
        QString name = displayName ? QString::fromWCharArray(displayName) : QString();
        if (displayName) CoTaskMemFree(displayName);

        // Get session identifier for additional matching
        LPWSTR sessionId = nullptr;
        sessionCtrl2->GetSessionIdentifier(&sessionId);
        QString sessIdStr = sessionId ? QString::fromWCharArray(sessionId) : QString();
        if (sessionId) CoTaskMemFree(sessionId);

        // Check if this is a system session (pid 0) - BT audio is typically
        // rendered by the system audio service
        AudioSessionState state;
        sessionCtrl->GetState(&state);

        LOG_INFOF("  Session[%d]: pid=%lu name='%s' state=%d id='%s'",
                  i, pid, qPrintable(name), static_cast<int>(state),
                  qPrintable(sessIdStr.left(80)));

        // Heuristic: BT audio session matches if:
        // - Session ID or display name contains BT indicators
        // - Display name contains the connected device name
        // - Display name contains "A2DP"
        bool isBtSession = false;

        // Check session ID for Bluetooth indicators
        if (sessIdStr.contains("BTHENUM", Qt::CaseInsensitive) ||
            sessIdStr.contains("Bluetooth", Qt::CaseInsensitive) ||
            sessIdStr.contains("AudioPlayback", Qt::CaseInsensitive)) {
            isBtSession = true;
        }

        // Check display name for BT indicators
        if (name.contains("Bluetooth", Qt::CaseInsensitive) ||
            name.contains("A2DP", Qt::CaseInsensitive)) {
            isBtSession = true;
        }

        // Check if display name contains the connected device name
        if (!connectedDeviceName.isEmpty() &&
            name.contains(connectedDeviceName, Qt::CaseInsensitive)) {
            isBtSession = true;
        }

        if (isBtSession) {
            // Found it! Get ISimpleAudioVolume
            ComPtr<ISimpleAudioVolume> volume;
            hr = sessionCtrl.As(&volume);
            if (SUCCEEDED(hr)) {
                m_sessionVolume = volume;
                m_sessionName = name.isEmpty() ? "Bluetooth Audio" : name;
                m_sessionPid = pid;
                LOG_INFOF("SessionVolumeController: found BT session: '%s' (pid=%lu)",
                          qPrintable(m_sessionName), m_sessionPid);
                emit sessionFound(m_sessionName);
                return true;
            }
        }
    }

    // Fallback: if only one non-system active session exists, it might be BT
    // Or if we found no BT-specific session, try the system session (pid=0)
    for (int i = 0; i < sessionCount; i++) {
        ComPtr<IAudioSessionControl> sessionCtrl;
        hr = sessionEnum->GetSession(i, sessionCtrl.GetAddressOf());
        if (FAILED(hr)) continue;

        ComPtr<IAudioSessionControl2> sessionCtrl2;
        hr = sessionCtrl.As(&sessionCtrl2);
        if (FAILED(hr)) continue;

        DWORD pid = 0;
        sessionCtrl2->GetProcessId(&pid);

        // System session (pid=0) is often the BT audio on the BT endpoint
        if (pid == 0) {
            AudioSessionState state;
            sessionCtrl->GetState(&state);

            ComPtr<ISimpleAudioVolume> volume;
            hr = sessionCtrl.As(&volume);
            if (SUCCEEDED(hr)) {
                m_sessionVolume = volume;
                m_sessionName = "System Audio (likely Bluetooth)";
                m_sessionPid = 0;
                LOG_INFO("SessionVolumeController: using system session (pid=0) as fallback");
                emit sessionFound(m_sessionName);
                return true;
            }
        }
    }

    LOG_WARN("SessionVolumeController: no BT audio session found");
    return false;
}

void SessionVolumeController::setVolume(float volume)
{
    if (!m_sessionVolume) return;

    float clamped = std::clamp(volume, 0.0f, 1.0f);
    HRESULT hr = m_sessionVolume->SetMasterVolume(clamped, nullptr);
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: SetMasterVolume failed: 0x%08X", hr);
    }
}

float SessionVolumeController::volume() const
{
    if (!m_sessionVolume) return 1.0f;

    float vol = 1.0f;
    m_sessionVolume->GetMasterVolume(&vol);
    return vol;
}

void SessionVolumeController::setMuted(bool muted)
{
    if (!m_sessionVolume) return;

    HRESULT hr = m_sessionVolume->SetMute(muted ? TRUE : FALSE, nullptr);
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: SetMute failed: 0x%08X", hr);
    }
}

bool SessionVolumeController::isMuted() const
{
    if (!m_sessionVolume) return false;

    BOOL muted = FALSE;
    m_sessionVolume->GetMute(&muted);
    return muted != FALSE;
}

} // namespace BlueDrop
