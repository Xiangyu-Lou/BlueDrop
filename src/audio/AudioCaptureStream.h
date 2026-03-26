#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

namespace BlueDrop {

// WASAPI audio capture stream wrapper.
// Supports both normal capture (microphone) and loopback capture.
class AudioCaptureStream {
public:
    AudioCaptureStream() = default;
    ~AudioCaptureStream();

    AudioCaptureStream(const AudioCaptureStream&) = delete;
    AudioCaptureStream& operator=(const AudioCaptureStream&) = delete;

    // Initialize the capture stream on the given device.
    // If isLoopback is true, captures audio playing on a render device.
    bool initialize(const wchar_t* deviceId, bool isLoopback = false);

    bool start();
    bool stop();
    bool isRunning() const { return m_running; }

    // Read available frames as interleaved float32 stereo.
    // Returns number of frames read.
    size_t readFrames(float* buffer, size_t maxFrames);

    uint32_t sampleRate() const { return m_sampleRate; }
    uint32_t channels() const { return m_channels; }

private:
    bool convertToFloat(const BYTE* src, float* dst, uint32_t frameCount,
                        const WAVEFORMATEX* format);

    Microsoft::WRL::ComPtr<IMMDevice> m_device;
    Microsoft::WRL::ComPtr<IAudioClient> m_audioClient;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> m_captureClient;

    uint32_t m_sampleRate = 0;
    uint32_t m_channels = 0;
    uint32_t m_bitsPerSample = 0;
    bool m_isFloat = false;
    bool m_isLoopback = false;
    bool m_running = false;

    // Diagnostic counters
    uint32_t m_packetCount = 0;
    uint32_t m_frameCount = 0;
    uint32_t m_silentCount = 0;
    uint32_t m_emptyCount = 0;
};

} // namespace BlueDrop
