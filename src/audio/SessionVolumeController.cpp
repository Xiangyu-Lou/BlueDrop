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

        LOG_INFO(QString("  Session[%1]: pid=%2 name='%3' state=%4 id='%5'")
                 .arg(i).arg(pid).arg(name).arg(static_cast<int>(state)).arg(sessIdStr.left(80)));

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
                LOG_INFO(QString("SessionVolumeController: found BT session: '%1' (pid=%2)")
                         .arg(m_sessionName).arg(m_sessionPid));
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
    if (!m_sessionVolume) {
        LOG_WARNF("SessionVolumeController: setVolume(%.2f) — no session (call findBluetoothSession first)", volume);
        return;
    }

    float clamped = std::clamp(volume, 0.0f, 1.0f);
    HRESULT hr = m_sessionVolume->SetMasterVolume(clamped, nullptr);
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: SetMasterVolume(%.2f) failed: 0x%08X", clamped, hr);
    } else {
        // Only log when value changes by more than 2% to avoid slider-drag spam
        if (std::abs(clamped - m_lastLoggedVolume) >= 0.02f) {
            LOG_DEBUG(QString("SessionVolumeController: volume -> %1 (session='%2')")
                      .arg(clamped, 0, 'f', 2).arg(m_sessionName));
            m_lastLoggedVolume = clamped;
        }
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
    if (!m_sessionVolume) {
        LOG_WARNF("SessionVolumeController: setMuted(%s) — no session", muted ? "true" : "false");
        return;
    }

    HRESULT hr = m_sessionVolume->SetMute(muted ? TRUE : FALSE, nullptr);
    if (FAILED(hr)) {
        LOG_ERRORF("SessionVolumeController: SetMute(%s) failed: 0x%08X",
                   muted ? "true" : "false", hr);
    } else {
        LOG_DEBUG(QString("SessionVolumeController: muted=%1 (session='%2')")
                  .arg(muted ? "true" : "false").arg(m_sessionName));
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

// --- IPolicyConfig interface declaration (undocumented but stable Win7~11) ---
// Used by EarTrumpet, SoundSwitch, and other mainstream audio tools.

MIDL_INTERFACE("F8679F50-850A-41CF-9C72-430F290290C8")
IPolicyConfig : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX *, WAVEFORMATEX *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, struct DeviceShareMode *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, struct DeviceShareMode *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY &, const PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR deviceId, ERole role) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

static const CLSID CLSID_CPolicyConfigClient = {
    0x870AF99C, 0x171D, 0x4F9E,
    {0xAF, 0x0D, 0xE6, 0x3D, 0xF4, 0x0C, 0x2B, 0xC9}
};

namespace BlueDrop {

QString SessionVolumeController::getDefaultPlaybackEndpoint()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) return {};

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf());
    if (FAILED(hr)) return {};

    LPWSTR id = nullptr;
    hr = device->GetId(&id);
    if (FAILED(hr) || !id) return {};

    QString result = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    return result;
}

bool SessionVolumeController::setDefaultPlaybackEndpoint(const QString& deviceId)
{
    if (deviceId.isEmpty()) return false;

    ComPtr<IPolicyConfig> policyConfig;
    HRESULT hr = CoCreateInstance(
        CLSID_CPolicyConfigClient, nullptr, CLSCTX_ALL,
        __uuidof(IPolicyConfig),
        reinterpret_cast<void**>(policyConfig.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("IPolicyConfig CoCreateInstance failed: 0x%08X", hr);
        return false;
    }

    auto wideId = deviceId.toStdWString();

    // Set for both eConsole and eMultimedia roles
    hr = policyConfig->SetDefaultEndpoint(wideId.c_str(), eConsole);
    if (FAILED(hr)) {
        LOG_ERRORF("SetDefaultEndpoint(eConsole) failed: 0x%08X", hr);
        return false;
    }

    hr = policyConfig->SetDefaultEndpoint(wideId.c_str(), eMultimedia);
    if (FAILED(hr)) {
        LOG_ERRORF("SetDefaultEndpoint(eMultimedia) failed: 0x%08X", hr);
        return false;
    }

    LOG_INFOF("Default playback endpoint set to: %s", qPrintable(deviceId));
    return true;
}

} // namespace BlueDrop
