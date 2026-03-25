#include "AudioEngine.h"
#include "AudioCaptureStream.h"
#include "AudioRenderStream.h"
#include "RingBuffer.h"
#include "Resampler.h"
#include "system/Logger.h"

#include <Windows.h>
#include <avrt.h>
#include <cmath>
#include <vector>

#pragma comment(lib, "Avrt.lib")

namespace BlueDrop {

// Internal processing buffer size (in frames)
static constexpr size_t PROCESS_BUFFER_FRAMES = 480; // 10ms at 48kHz

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
}

AudioEngine::~AudioEngine()
{
    stop();
}

void AudioEngine::setLoopbackDevice(const QString& deviceId)
{
    m_loopbackDeviceId = deviceId;
}

void AudioEngine::setMicrophoneDevice(const QString& deviceId)
{
    m_micDeviceId = deviceId;
}

void AudioEngine::setVirtualCableDevice(const QString& deviceId)
{
    m_virtualCableDeviceId = deviceId;
}

void AudioEngine::setPhoneVolume(float gain) {
    m_phoneVolume.store(std::clamp(gain, 0.0f, 2.0f));
}

void AudioEngine::setMicVolume(float gain) {
    m_micVolume.store(std::clamp(gain, 0.0f, 2.0f));
}

void AudioEngine::setPhoneMixRatio(float ratio) {
    m_phoneMixRatio.store(std::clamp(ratio, 0.0f, 1.0f));
}

void AudioEngine::setPhoneMuted(bool muted) {
    m_phoneMuted.store(muted);
}

void AudioEngine::setMicMuted(bool muted) {
    m_micMuted.store(muted);
}

void AudioEngine::start()
{
    if (m_running.load()) {
        LOG_WARN("AudioEngine already running");
        return;
    }

    LOG_INFOF("AudioEngine::start() loopback=%s mic=%s vcable=%s",
              qPrintable(m_loopbackDeviceId),
              qPrintable(m_micDeviceId),
              qPrintable(m_virtualCableDeviceId));

    // Validate device IDs
    if (m_virtualCableDeviceId.isEmpty()) {
        LOG_ERROR("No virtual cable device set");
        emit errorOccurred(QString::fromUtf8(u8"未设置虚拟声卡输出设备"));
        return;
    }

    m_stopRequested.store(false);

    m_audioThread = std::thread([this]() {
        audioThreadFunc();
    });

    m_running.store(true);
    LOG_INFO("AudioEngine started");
}

void AudioEngine::stop()
{
    if (!m_running.load()) return;

    m_stopRequested.store(true);

    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }

    m_running.store(false);
}

float AudioEngine::softClip(float sample)
{
    // Soft clipping using tanh-like curve
    if (sample > 1.0f) {
        return 1.0f - std::exp(-sample);
    } else if (sample < -1.0f) {
        return -(1.0f - std::exp(sample));
    }
    return sample;
}

void AudioEngine::audioThreadFunc()
{
    // Elevate thread priority via MMCSS
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    // Initialize COM for this thread
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Initialize capture and render streams
    auto loopbackCapture = std::make_unique<AudioCaptureStream>();
    auto micCapture = std::make_unique<AudioCaptureStream>();
    auto vcableRender = std::make_unique<AudioRenderStream>();

    bool hasLoopback = false;
    bool hasMic = false;
    bool hasOutput = false;

    // Initialize loopback capture (BT audio from monitoring endpoint)
    if (!m_loopbackDeviceId.isEmpty()) {
        auto id = m_loopbackDeviceId.toStdWString();
        if (loopbackCapture->initialize(id.c_str(), true)) {
            loopbackCapture->start();
            hasLoopback = true;
        }
    }

    // Initialize mic capture
    if (!m_micDeviceId.isEmpty()) {
        auto id = m_micDeviceId.toStdWString();
        if (micCapture->initialize(id.c_str(), false)) {
            micCapture->start();
            hasMic = true;
        }
    }

    // Initialize VB-Cable render output
    {
        auto id = m_virtualCableDeviceId.toStdWString();
        if (vcableRender->initialize(id.c_str())) {
            vcableRender->start();
            hasOutput = true;
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                emit errorOccurred(QString::fromUtf8(u8"无法打开虚拟声卡输出设备"));
            }, Qt::QueuedConnection);
        }
    }

    // Processing buffers (interleaved stereo float32)
    std::vector<float> loopbackBuf(PROCESS_BUFFER_FRAMES * 2, 0.0f);
    std::vector<float> micBuf(PROCESS_BUFFER_FRAMES * 2, 0.0f);
    std::vector<float> mixBuf(PROCESS_BUFFER_FRAMES * 2, 0.0f);

    // Main audio processing loop
    while (!m_stopRequested.load()) {
        size_t loopbackFrames = 0;
        size_t micFrames = 0;

        // Read loopback (BT audio)
        if (hasLoopback) {
            loopbackFrames = loopbackCapture->readFrames(loopbackBuf.data(), PROCESS_BUFFER_FRAMES);
        }

        // Read microphone
        if (hasMic) {
            micFrames = micCapture->readFrames(micBuf.data(), PROCESS_BUFFER_FRAMES);
        }

        // Determine output frame count
        size_t outFrames = std::max(loopbackFrames, micFrames);

        if (outFrames > 0 && hasOutput) {
            // Load atomic controls
            float phoneVol = m_phoneMuted.load() ? 0.0f : m_phoneVolume.load();
            float micVol = m_micMuted.load() ? 0.0f : m_micVolume.load();
            float phoneRatio = m_phoneMixRatio.load();

            // Mix: Route B = (BT * phoneVol * phoneRatio) + (Mic * micVol)
            for (size_t i = 0; i < outFrames * 2; i++) {
                float phoneSample = (i < loopbackFrames * 2) ? loopbackBuf[i] * phoneVol * phoneRatio : 0.0f;
                float micSample = (i < micFrames * 2) ? micBuf[i] * micVol : 0.0f;

                mixBuf[i] = softClip(phoneSample + micSample);
            }

            // Write to VB-Cable
            vcableRender->writeFrames(mixBuf.data(), outFrames);
        }

        // Sleep for ~5ms to avoid busy-waiting
        // WASAPI shared mode buffers are typically 10ms
        Sleep(5);
    }

    // Cleanup
    if (hasLoopback) loopbackCapture->stop();
    if (hasMic) micCapture->stop();
    if (hasOutput) vcableRender->stop();

    if (hTask) {
        AvRevertMmThreadCharacteristics(hTask);
    }

    CoUninitialize();
}

} // namespace BlueDrop
