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
    stopBoost();
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

    LOG_INFOF("AudioEngine::start() (Route-B broadcast) loopback=%s mic=%s vcable=%s",
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

    LOG_INFO("AudioEngine::stop() (Route-B broadcast)");
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
            LOG_INFOF("AudioThread: loopback capture OK — %uHz %uch",
                      loopbackCapture->sampleRate(), loopbackCapture->channels());
        } else {
            LOG_ERROR("AudioThread: failed to initialize loopback capture");
        }
    } else {
        LOG_INFO("AudioThread: no loopback device — loopback capture skipped");
    }

    // Initialize mic capture
    if (!m_micDeviceId.isEmpty()) {
        auto id = m_micDeviceId.toStdWString();
        if (micCapture->initialize(id.c_str(), false)) {
            micCapture->start();
            hasMic = true;
            LOG_INFOF("AudioThread: mic capture OK — %uHz %uch",
                      micCapture->sampleRate(), micCapture->channels());
        } else {
            LOG_ERROR("AudioThread: failed to initialize mic capture");
        }
    } else {
        LOG_INFO("AudioThread: no mic device — mic capture skipped");
    }

    // Initialize VB-Cable render output
    {
        auto id = m_virtualCableDeviceId.toStdWString();
        if (vcableRender->initialize(id.c_str())) {
            vcableRender->start();
            hasOutput = true;
            LOG_INFOF("AudioThread: VB-Cable render OK — %uHz %uch bufferFrames=%u",
                      vcableRender->sampleRate(), vcableRender->channels(), vcableRender->bufferFrames());
        } else {
            LOG_ERROR("AudioThread: failed to initialize VB-Cable render");
            QMetaObject::invokeMethod(this, [this]() {
                emit errorOccurred(QString::fromUtf8(u8"无法打开虚拟声卡输出设备"));
            }, Qt::QueuedConnection);
        }
    }

    LOG_INFOF("AudioThread: starting loop — hasLoopback=%s hasMic=%s hasOutput=%s",
              hasLoopback ? "yes" : "no",
              hasMic ? "yes" : "no",
              hasOutput ? "yes" : "no");

    // Processing buffers (interleaved stereo float32)
    std::vector<float> loopbackBuf(PROCESS_BUFFER_FRAMES * 2, 0.0f);
    std::vector<float> micBuf(PROCESS_BUFFER_FRAMES * 2, 0.0f);
    std::vector<float> mixBuf(PROCESS_BUFFER_FRAMES * 2, 0.0f);

    // Diagnostic counters
    uint32_t loopCount = 0;
    uint32_t totalLoopbackFrames = 0;
    uint32_t totalMicFrames = 0;
    uint32_t totalOutFrames = 0;
    uint32_t zeroOutCount = 0;

    // Main audio processing loop
    while (!m_stopRequested.load()) {
        size_t loopbackFrames = 0;
        size_t micFrames = 0;

        // Read loopback (BT audio)
        if (hasLoopback) {
            loopbackFrames = loopbackCapture->readFrames(loopbackBuf.data(), PROCESS_BUFFER_FRAMES);
            totalLoopbackFrames += static_cast<uint32_t>(loopbackFrames);
        }

        // Read microphone
        if (hasMic) {
            micFrames = micCapture->readFrames(micBuf.data(), PROCESS_BUFFER_FRAMES);
            totalMicFrames += static_cast<uint32_t>(micFrames);
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
            totalOutFrames += static_cast<uint32_t>(outFrames);
        } else {
            zeroOutCount++;
        }

        // Periodic stats every 200 iterations (~1s at 5ms sleep)
        if (++loopCount % 200 == 0) {
            LOG_INFOF("AudioThread stats [iter %u]: loopbackFrames=%u micFrames=%u outFrames=%u zeroIter=%u",
                      loopCount, totalLoopbackFrames, totalMicFrames, totalOutFrames, zeroOutCount);
        }

        // Sleep for ~5ms to avoid busy-waiting
        // WASAPI shared mode buffers are typically 10ms
        Sleep(5);
    }

    LOG_INFOF("AudioThread: loop exited after %u iterations (loopbackFrames=%u micFrames=%u outFrames=%u)",
              loopCount, totalLoopbackFrames, totalMicFrames, totalOutFrames);

    // Cleanup
    if (hasLoopback) loopbackCapture->stop();
    if (hasMic) micCapture->stop();
    if (hasOutput) vcableRender->stop();

    if (hTask) {
        AvRevertMmThreadCharacteristics(hTask);
    }

    CoUninitialize();
}

// --- Boost mode implementation ---

void AudioEngine::setBoostGain(float gain)
{
    m_boostGain.store(std::clamp(gain, 0.0f, 10.0f));
}

void AudioEngine::startBoost(const QString& captureDeviceId,
                              const QString& renderDeviceId)
{
    if (m_boostRunning.load()) {
        LOG_WARN("Boost already running");
        return;
    }

    LOG_INFOF("AudioEngine::startBoost() capture=%s render=%s",
              qPrintable(captureDeviceId), qPrintable(renderDeviceId));

    if (captureDeviceId.isEmpty() || renderDeviceId.isEmpty()) {
        LOG_ERROR("Boost: missing device ID");
        emit errorOccurred(QString::fromUtf8(u8"增益模式缺少设备配置"));
        return;
    }

    m_boostStopRequested.store(false);

    // Capture device/render device IDs for the thread
    QString capId = captureDeviceId;
    QString renId = renderDeviceId;

    m_boostThread = std::thread([this, capId, renId]() {
        // Elevate thread priority
        DWORD taskIndex = 0;
        HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        auto capture = std::make_unique<AudioCaptureStream>();
        auto render = std::make_unique<AudioRenderStream>();

        bool hasCap = false;
        bool hasRen = false;

        // Initialize loopback capture from VB-Cable (where BT audio is now routed)
        {
            auto id = capId.toStdWString();
            if (capture->initialize(id.c_str(), true)) {
                capture->start();
                hasCap = true;
                LOG_INFO("Boost: loopback capture initialized");
            } else {
                LOG_ERROR("Boost: failed to init loopback capture");
            }
        }

        // Initialize render to headphone.
        // Use a 100ms buffer (1000000 × 100ns).
        // Our polling loop runs every ~15ms (Sleep(8) + overhead), while the WASAPI
        // capture period is 10ms. Each poll may drain 1-2 WASAPI periods (480-960 frames).
        // A 100ms render buffer with 50ms pre-fill gives 50ms of free space — enough to
        // absorb a burst of up to 5 periods without overflow.
        {
            auto id = renId.toStdWString();
            if (render->initialize(id.c_str(), 1000000)) {
                render->start();
                hasRen = true;
                LOG_INFOF("Boost: render initialized, buffer=%u frames", render->bufferFrames());
            } else {
                LOG_ERROR("Boost: failed to init render");
            }
        }

        if (!hasCap || !hasRen) {
            if (hasCap) capture->stop();
            if (hasRen) render->stop();
            QMetaObject::invokeMethod(this, [this]() {
                emit errorOccurred(QString::fromUtf8(u8"增益模式启动失败"));
            }, Qt::QueuedConnection);
            CoUninitialize();
            if (hTask) AvRevertMmThreadCharacteristics(hTask);
            return;
        }

        m_boostRunning.store(true);

        // Log format info for diagnosis
        LOG_INFOF("Boost: capture sampleRate=%u channels=%u",
                  capture->sampleRate(), capture->channels());
        LOG_INFOF("Boost: render  sampleRate=%u channels=%u bufferFrames=%u",
                  render->sampleRate(), render->channels(), render->bufferFrames());
        LOG_INFOF("Boost: Route-B running = %s", m_running.load() ? "YES (potential feedback!)" : "no");

        // Warn if sample rates differ — this causes gradual buffer drift and stuttering
        if (capture->sampleRate() != render->sampleRate()) {
            LOG_WARNF("Boost: sample rate mismatch! capture=%u render=%u — resampling required",
                      capture->sampleRate(), render->sampleRate());
        }

        LOG_INFO("Boost: audio loop started");

        // Resampler: converts capture rate → render rate if they differ
        Resampler resampler(capture->sampleRate(), render->sampleRate(), 2);

        // Read buffer: large enough to drain up to 4 WASAPI capture periods per poll.
        // Our loop runs every ~15ms but WASAPI delivers data every 10ms, so we can
        // accumulate 1-2 periods between polls. Using 4× gives plenty of headroom
        // and ensures we always fully drain the WASAPI capture queue.
        constexpr size_t MAX_READ_FRAMES = PROCESS_BUFFER_FRAMES * 4; // 1920 frames = 40ms
        std::vector<float> capBuf(MAX_READ_FRAMES * 2, 0.0f);
        // Resample output: worst case input_frames * (renderRate/captureRate) + 1
        const size_t rsMaxFrames = MAX_READ_FRAMES + 16;
        std::vector<float> rsBuf(rsMaxFrames * 2, 0.0f);

        // Pre-fill silence using actual render sample rate
        {
            const uint32_t renderRate = render->sampleRate() > 0 ? render->sampleRate() : 48000;
            const size_t PREFILL_FRAMES = renderRate * 50 / 1000; // 50ms (half of 100ms buffer)
            size_t available = render->availableFrames();
            size_t toFill = std::min(available, PREFILL_FRAMES);
            size_t written = 0;
            while (written < toFill) {
                size_t chunk = std::min(toFill - written, PROCESS_BUFFER_FRAMES);
                render->writeFrames(capBuf.data(), chunk);
                written += chunk;
            }
            LOG_INFOF("Boost: pre-filled %zu frames (%.1f ms) of silence",
                      written, written * 1000.0f / renderRate);
        }

        // Diagnostic counters
        uint32_t loopCount = 0;
        uint32_t zeroCapCount = 0;
        uint32_t totalCapFrames = 0;
        uint32_t totalWrittenFrames = 0;

        while (!m_boostStopRequested.load()) {
            // Drain ALL available WASAPI capture data (not just one period).
            // Without this, accumulated periods overflow the WASAPI buffer and
            // cause DATA_DISCONTINUITY (silent frame drops → audible stuttering).
            size_t frames = capture->readFrames(capBuf.data(), MAX_READ_FRAMES);

            if (frames > 0) {
                zeroCapCount = 0;  // reset consecutive-zero counter
                float gain = m_boostGain.load();
                for (size_t i = 0; i < frames * 2; i++) {
                    capBuf[i] = softClip(capBuf[i] * gain);
                }

                // Resample if needed, then write to render
                size_t outFrames;
                const float* writePtr;
                if (resampler.needsResampling()) {
                    outFrames = resampler.process(capBuf.data(), frames,
                                                  rsBuf.data(), rsMaxFrames);
                    writePtr = rsBuf.data();
                } else {
                    outFrames = frames;
                    writePtr = capBuf.data();
                }

                size_t available = render->availableFrames();
                size_t written = render->writeFrames(writePtr, outFrames);
                totalCapFrames += static_cast<uint32_t>(frames);
                totalWrittenFrames += static_cast<uint32_t>(written);

                // Warn if render buffer is dangerously low
                if (available < 240) { // < 5ms
                    LOG_WARNF("Boost [iter %u]: render near-empty! available=%zu frames", loopCount, available);
                }
                // Warn if frames were silently dropped (buffer full)
                if (written < outFrames) {
                    LOG_WARNF("Boost [iter %u]: render full, dropped %zu frames (available=%zu)",
                              loopCount, outFrames - written, available);
                }
            } else {
                zeroCapCount++;
                if (zeroCapCount == 5) {
                    LOG_WARNF("Boost [iter %u]: capture returning 0 frames for %u consecutive polls",
                              loopCount, zeroCapCount);
                }
            }

            // Periodic stats every 200 iterations (~1.6s)
            if (++loopCount % 200 == 0) {
                size_t renderAvail = render->availableFrames();
                uint32_t renderBuf = render->bufferFrames();
                float avgCapPerIter = loopCount > 0 ? static_cast<float>(totalCapFrames) / loopCount : 0.f;
                LOG_INFOF("Boost stats [iter %u]: capFrames=%u writtenFrames=%u avgCap/iter=%.0f "
                          "renderAvail=%zu/%u (%.0f%%) zeroPolls=%u",
                          loopCount, totalCapFrames, totalWrittenFrames, avgCapPerIter,
                          renderAvail, renderBuf,
                          renderBuf > 0 ? renderAvail * 100.0f / renderBuf : 0.f,
                          zeroCapCount);
            }

            // Sleep 8ms — slightly less than the 10ms WASAPI period.
            // With ~8ms sleep + ~7ms overhead ≈ 15ms per loop, we reliably
            // cross at least one full 10ms capture period and always find data.
            // Shorter sleeps (e.g. 5ms) risk polling before a period completes,
            // getting 0 frames and leaving the render buffer underfed.
            Sleep(8);
        }

        capture->stop();
        render->stop();

        if (hTask) AvRevertMmThreadCharacteristics(hTask);
        CoUninitialize();

        m_boostRunning.store(false);
        LOG_INFO("Boost: stopped");
    });
}

void AudioEngine::stopBoost()
{
    if (!m_boostRunning.load() && !m_boostThread.joinable()) return;

    m_boostStopRequested.store(true);
    if (m_boostThread.joinable()) {
        m_boostThread.join();
    }
    m_boostRunning.store(false);
    LOG_INFO("Boost: thread joined");
}

} // namespace BlueDrop
