#pragma once

#include <QObject>

namespace BlueDrop {

class AudioEngine;

class MixerVM : public QObject {
    Q_OBJECT
    Q_PROPERTY(float phoneVolume READ phoneVolume WRITE setPhoneVolume NOTIFY phoneVolumeChanged)
    Q_PROPERTY(float micVolume READ micVolume WRITE setMicVolume NOTIFY micVolumeChanged)
    Q_PROPERTY(float phoneMixRatio READ phoneMixRatio WRITE setPhoneMixRatio NOTIFY phoneMixRatioChanged)
    Q_PROPERTY(bool phoneMuted READ phoneMuted WRITE setPhoneMuted NOTIFY phoneMutedChanged)
    Q_PROPERTY(bool micMuted READ micMuted WRITE setMicMuted NOTIFY micMutedChanged)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY engineRunningChanged)
public:
    explicit MixerVM(AudioEngine* engine, QObject* parent = nullptr);

    float phoneVolume() const;
    float micVolume() const;
    float phoneMixRatio() const;
    bool phoneMuted() const;
    bool micMuted() const;
    bool engineRunning() const;

    void setPhoneVolume(float v);
    void setMicVolume(float v);
    void setPhoneMixRatio(float v);
    void setPhoneMuted(bool v);
    void setMicMuted(bool v);

    Q_INVOKABLE void startEngine();
    Q_INVOKABLE void stopEngine();

signals:
    void phoneVolumeChanged();
    void micVolumeChanged();
    void phoneMixRatioChanged();
    void phoneMutedChanged();
    void micMutedChanged();
    void engineRunningChanged();

private:
    AudioEngine* m_engine;
};

} // namespace BlueDrop
