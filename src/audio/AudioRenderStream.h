#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <cstdint>

namespace BlueDrop {

// WASAPI audio render stream wrapper for outputting audio to a device.
class AudioRenderStream {
public:
    AudioRenderStream() = default;
    ~AudioRenderStream();

    AudioRenderStream(const AudioRenderStream&) = delete;
    AudioRenderStream& operator=(const AudioRenderStream&) = delete;

    // Initialize on the specified render device.
    // bufferDuration: WASAPI buffer size in 100-nanosecond units (default 10ms = 100000).
    // Use a larger value (e.g. 500000 = 50ms) for pass-through paths where timing is loose.
    bool initialize(const wchar_t* deviceId, REFERENCE_TIME bufferDuration = 100000);

    bool start();
    bool stop();
    bool isRunning() const { return m_running; }

    // Write interleaved float32 stereo frames.
    // Returns number of frames actually written.
    size_t writeFrames(const float* data, size_t frameCount);

    // Get available space in the render buffer (in frames).
    size_t availableFrames() const;

    uint32_t sampleRate() const { return m_sampleRate; }
    uint32_t channels() const { return m_channels; }
    uint32_t bufferFrames() const { return m_bufferFrames; }

private:
    Microsoft::WRL::ComPtr<IMMDevice> m_device;
    Microsoft::WRL::ComPtr<IAudioClient> m_audioClient;
    Microsoft::WRL::ComPtr<IAudioRenderClient> m_renderClient;

    uint32_t m_sampleRate = 0;
    uint32_t m_channels = 0;
    uint32_t m_bufferFrames = 0;
    bool m_isFloat = false;
    bool m_running = false;

    // Diagnostic counters
    uint32_t m_writeCount = 0;
    uint32_t m_totalFramesWritten = 0;
    uint32_t m_fullCount = 0;
};

} // namespace BlueDrop
