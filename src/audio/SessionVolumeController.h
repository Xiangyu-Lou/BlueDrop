#pragma once

#include <QObject>
#include <QString>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <wrl/client.h>

namespace BlueDrop {

/// Controls the volume of a specific audio session on a given endpoint.
/// Used to independently adjust Bluetooth audio volume without
/// affecting other PC audio on the same playback device.
class SessionVolumeController : public QObject {
    Q_OBJECT
public:
    explicit SessionVolumeController(QObject* parent = nullptr);
    ~SessionVolumeController() override;

    /// Scan sessions on the given endpoint and find the BT audio session.
    /// @param endpointId  WASAPI device ID of the playback endpoint
    ///                    where BT audio is routed (the monitoring device).
    /// @param connectedDeviceName  Name of the connected BT device (for matching).
    /// @return true if a BT audio session was found.
    bool findBluetoothSession(const QString& endpointId,
                              const QString& connectedDeviceName = {});

    /// Set volume of the BT audio session (0.0 ~ 1.0).
    void setVolume(float volume);
    float volume() const;

    /// Mute/unmute the BT audio session.
    void setMuted(bool muted);
    bool isMuted() const;

    /// Whether a valid session is currently attached.
    bool hasSession() const { return m_sessionVolume != nullptr; }

    /// Get info about the found session (for debugging).
    QString sessionDisplayName() const { return m_sessionName; }
    DWORD sessionProcessId() const { return m_sessionPid; }

    // --- Default endpoint switching (via IPolicyConfig) ---

    /// Get the current default playback endpoint ID.
    static QString getDefaultPlaybackEndpoint();

    /// Set the default playback endpoint (for eConsole + eMultimedia roles).
    /// Returns true on success.
    static bool setDefaultPlaybackEndpoint(const QString& deviceId);

signals:
    void sessionFound(const QString& displayName);
    void sessionLost();

private:
    Microsoft::WRL::ComPtr<ISimpleAudioVolume> m_sessionVolume;
    QString m_sessionName;
    DWORD m_sessionPid = 0;
};

} // namespace BlueDrop
