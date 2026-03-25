#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <thread>

namespace BlueDrop {

class RingBuffer;
class AudioCaptureStream;
class AudioRenderStream;
class Resampler;

class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    // Set devices (must be called before start, or will restart streams)
    void setLoopbackDevice(const QString& deviceId);
    void setMicrophoneDevice(const QString& deviceId);
    void setVirtualCableDevice(const QString& deviceId);

    // Start/stop audio processing
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    bool isRunning() const { return m_running.load(); }

    // Volume controls (thread-safe via atomics)
    void setPhoneVolume(float gain);     // 0.0 ~ 2.0
    void setMicVolume(float gain);       // 0.0 ~ 2.0
    void setPhoneMixRatio(float ratio);  // 0.0 ~ 1.0
    void setPhoneMuted(bool muted);
    void setMicMuted(bool muted);

    float phoneVolume() const { return m_phoneVolume.load(); }
    float micVolume() const { return m_micVolume.load(); }
    float phoneMixRatio() const { return m_phoneMixRatio.load(); }
    bool phoneMuted() const { return m_phoneMuted.load(); }
    bool micMuted() const { return m_micMuted.load(); }

signals:
    void errorOccurred(const QString& message);

private:
    void audioThreadFunc();
    static float softClip(float sample);

    // Device IDs
    QString m_loopbackDeviceId;
    QString m_micDeviceId;
    QString m_virtualCableDeviceId;

    // Audio streams
    std::unique_ptr<AudioCaptureStream> m_loopbackCapture;
    std::unique_ptr<AudioCaptureStream> m_micCapture;
    std::unique_ptr<AudioRenderStream> m_virtualCableRender;

    // Audio thread
    std::thread m_audioThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    // Volume controls (atomic for thread safety)
    std::atomic<float> m_phoneVolume{1.0f};
    std::atomic<float> m_micVolume{1.0f};
    std::atomic<float> m_phoneMixRatio{1.0f};
    std::atomic<bool> m_phoneMuted{false};
    std::atomic<bool> m_micMuted{false};
};

} // namespace BlueDrop
