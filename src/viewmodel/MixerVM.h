#pragma once

#include <QObject>
#include <QString>

namespace BlueDrop {

class AudioEngine;
class SessionVolumeController;
class DeviceEnumerator;

class MixerVM : public QObject {
    Q_OBJECT
    // Existing mix controls (for Route B: BT+mic -> VB-Cable)
    Q_PROPERTY(float phoneVolume READ phoneVolume WRITE setPhoneVolume NOTIFY phoneVolumeChanged)
    Q_PROPERTY(float micVolume READ micVolume WRITE setMicVolume NOTIFY micVolumeChanged)
    Q_PROPERTY(float phoneMixRatio READ phoneMixRatio WRITE setPhoneMixRatio NOTIFY phoneMixRatioChanged)
    Q_PROPERTY(bool phoneMuted READ phoneMuted WRITE setPhoneMuted NOTIFY phoneMutedChanged)
    Q_PROPERTY(bool micMuted READ micMuted WRITE setMicMuted NOTIFY micMutedChanged)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY engineRunningChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // BT audio monitoring volume (Route A: what user hears in headphones)
    Q_PROPERTY(float btMonitorVolume READ btMonitorVolume WRITE setBtMonitorVolume NOTIFY btMonitorVolumeChanged)
    Q_PROPERTY(bool btMonitorMuted READ btMonitorMuted WRITE setBtMonitorMuted NOTIFY btMonitorMutedChanged)
    Q_PROPERTY(bool btSessionFound READ btSessionFound NOTIFY btSessionFoundChanged)

    // Boost mode (Route A amplification beyond 100%)
    Q_PROPERTY(bool boostEnabled READ boostEnabled WRITE setBoostEnabled NOTIFY boostEnabledChanged)
    Q_PROPERTY(float boostGain READ boostGain WRITE setBoostGain NOTIFY boostGainChanged)
    Q_PROPERTY(bool boostMuted READ boostMuted WRITE setBoostMuted NOTIFY boostMutedChanged)
    Q_PROPERTY(bool boostAvailable READ boostAvailable NOTIFY boostAvailableChanged)
public:
    explicit MixerVM(AudioEngine* engine, SessionVolumeController* sessionVol,
                     DeviceEnumerator* deviceEnum,
                     QObject* parent = nullptr);

    float phoneVolume() const;
    float micVolume() const;
    float phoneMixRatio() const;
    bool phoneMuted() const;
    bool micMuted() const;
    bool engineRunning() const;
    QString lastError() const { return m_lastError; }

    float btMonitorVolume() const;
    bool btMonitorMuted() const;
    bool btSessionFound() const;

    bool boostEnabled() const { return m_boostEnabled; }
    float boostGain() const { return m_boostGain; }
    bool boostMuted() const { return m_boostMuted; }
    bool boostAvailable() const;  // true if VB-Cable installed

    void setPhoneVolume(float v);
    void setMicVolume(float v);
    void setPhoneMixRatio(float v);
    void setPhoneMuted(bool v);
    void setMicMuted(bool v);

    void setBtMonitorVolume(float v);
    void setBtMonitorMuted(bool v);

    void setBoostEnabled(bool v);
    void setBoostGain(float v);
    void setBoostMuted(bool v);

    Q_INVOKABLE void startEngine();
    Q_INVOKABLE void stopEngine();

    /// Scan for BT audio session on the given endpoint
    Q_INVOKABLE void scanBtSession(const QString& endpointId,
                                    const QString& deviceName = {});

signals:
    void phoneVolumeChanged();
    void micVolumeChanged();
    void phoneMixRatioChanged();
    void phoneMutedChanged();
    void micMutedChanged();
    void engineRunningChanged();
    void lastErrorChanged();
    void btMonitorVolumeChanged();
    void btMonitorMutedChanged();
    void btSessionFoundChanged();
    void boostEnabledChanged();
    void boostGainChanged();
    void boostMutedChanged();
    void boostAvailableChanged();

private:
    AudioEngine* m_engine;
    SessionVolumeController* m_sessionVol;
    DeviceEnumerator* m_deviceEnum;
    QString m_lastError;
    float m_btVolume = 1.0f;
    bool m_btMuted = false;

    // Boost state
    bool m_boostEnabled = false;
    float m_boostGain = 1.0f;
    bool m_boostMuted = false;
    QString m_savedDefaultEndpoint;    // Saved before boost switches endpoint
    QString m_monitorDeviceId;         // Current monitor headphone device ID
};

} // namespace BlueDrop
