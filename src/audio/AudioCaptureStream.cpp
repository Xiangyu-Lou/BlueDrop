#include "AudioCaptureStream.h"

#include <functiondiscoverykeys_devpkey.h>
#include <cstring>

namespace BlueDrop {

AudioCaptureStream::~AudioCaptureStream()
{
    stop();
}

bool AudioCaptureStream::initialize(const wchar_t* deviceId, bool isLoopback)
{
    m_isLoopback = isLoopback;

    // Get device enumerator
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) return false;

    // Get device by ID
    hr = enumerator->GetDevice(deviceId, m_device.GetAddressOf());
    if (FAILED(hr)) return false;

    // Activate audio client
    hr = m_device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(m_audioClient.GetAddressOf()));
    if (FAILED(hr)) return false;

    // Get mix format
    WAVEFORMATEX* mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) return false;

    m_sampleRate = mixFormat->nSamplesPerSec;
    m_channels = mixFormat->nChannels;
    m_bitsPerSample = mixFormat->wBitsPerSample;

    // Check if format is float
    if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        m_isFloat = true;
    } else if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        m_isFloat = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    // Initialize audio client
    DWORD flags = 0;
    if (isLoopback) {
        flags = AUDCLNT_STREAMFLAGS_LOOPBACK;
        // Loopback cannot use event callback - use polling
    }

    // 10ms buffer duration
    REFERENCE_TIME bufferDuration = 100000; // 10ms in 100ns units

    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        flags,
        bufferDuration,
        0,
        mixFormat,
        nullptr);

    CoTaskMemFree(mixFormat);

    if (FAILED(hr)) return false;

    // Get capture client
    hr = m_audioClient->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(m_captureClient.GetAddressOf()));
    if (FAILED(hr)) return false;

    return true;
}

bool AudioCaptureStream::start()
{
    if (m_running || !m_audioClient) return false;

    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) return false;

    m_running = true;
    return true;
}

bool AudioCaptureStream::stop()
{
    if (!m_running || !m_audioClient) return false;

    m_audioClient->Stop();
    m_running = false;
    return true;
}

size_t AudioCaptureStream::readFrames(float* buffer, size_t maxFrames)
{
    if (!m_running || !m_captureClient) return 0;

    size_t totalFrames = 0;

    while (totalFrames < maxFrames) {
        UINT32 packetLength = 0;
        HRESULT hr = m_captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr) || packetLength == 0) break;

        BYTE* data = nullptr;
        UINT32 numFrames = 0;
        DWORD flags = 0;

        hr = m_captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
        if (FAILED(hr)) break;

        size_t framesToCopy = std::min(static_cast<size_t>(numFrames), maxFrames - totalFrames);

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            // Fill with silence
            std::memset(buffer + totalFrames * 2, 0, framesToCopy * 2 * sizeof(float));
        } else {
            // Convert to interleaved float32 stereo
            convertToFloat(data, buffer + totalFrames * 2,
                          static_cast<uint32_t>(framesToCopy),
                          nullptr);
        }

        m_captureClient->ReleaseBuffer(numFrames);
        totalFrames += framesToCopy;
    }

    return totalFrames;
}

bool AudioCaptureStream::convertToFloat(const BYTE* src, float* dst,
                                         uint32_t frameCount,
                                         const WAVEFORMATEX*)
{
    if (m_isFloat && m_bitsPerSample == 32) {
        if (m_channels == 2) {
            // Direct copy for float32 stereo
            std::memcpy(dst, src, frameCount * 2 * sizeof(float));
        } else if (m_channels == 1) {
            // Mono to stereo duplication
            const float* fsrc = reinterpret_cast<const float*>(src);
            for (uint32_t i = 0; i < frameCount; i++) {
                dst[i * 2] = fsrc[i];
                dst[i * 2 + 1] = fsrc[i];
            }
        } else {
            // Multi-channel: take first two channels
            const float* fsrc = reinterpret_cast<const float*>(src);
            for (uint32_t i = 0; i < frameCount; i++) {
                dst[i * 2] = fsrc[i * m_channels];
                dst[i * 2 + 1] = fsrc[i * m_channels + 1];
            }
        }
        return true;
    }

    if (!m_isFloat && m_bitsPerSample == 16) {
        const int16_t* isrc = reinterpret_cast<const int16_t*>(src);
        constexpr float scale = 1.0f / 32768.0f;

        if (m_channels == 2) {
            for (uint32_t i = 0; i < frameCount * 2; i++) {
                dst[i] = static_cast<float>(isrc[i]) * scale;
            }
        } else if (m_channels == 1) {
            for (uint32_t i = 0; i < frameCount; i++) {
                float s = static_cast<float>(isrc[i]) * scale;
                dst[i * 2] = s;
                dst[i * 2 + 1] = s;
            }
        }
        return true;
    }

    // Unsupported format - fill with silence
    std::memset(dst, 0, frameCount * 2 * sizeof(float));
    return false;
}

} // namespace BlueDrop
