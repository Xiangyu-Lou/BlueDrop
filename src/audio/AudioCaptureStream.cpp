#include "AudioCaptureStream.h"
#include "system/Logger.h"

#include <functiondiscoverykeys_devpkey.h>
#include <cstring>

namespace BlueDrop {

AudioCaptureStream::~AudioCaptureStream()
{
    stop();
    LOG_DEBUGF("CaptureStream destroyed (loopback=%s packets=%u frames=%u silent=%u emptyPolls=%u)",
               m_isLoopback ? "yes" : "no",
               m_packetCount, m_frameCount, m_silentCount, m_emptyCount);
}

bool AudioCaptureStream::initialize(const wchar_t* deviceId, bool isLoopback)
{
    m_isLoopback = isLoopback;
    LOG_INFOF("CaptureStream::initialize() loopback=%s deviceId=%.60S",
              isLoopback ? "yes" : "no", deviceId);

    // Get device enumerator
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("CaptureStream: CoCreateInstance(MMDeviceEnumerator) failed: 0x%08X", hr);
        return false;
    }

    // Get device by ID
    hr = enumerator->GetDevice(deviceId, m_device.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERRORF("CaptureStream: GetDevice failed: 0x%08X", hr);
        return false;
    }

    // Activate audio client
    hr = m_device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(m_audioClient.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("CaptureStream: Activate(IAudioClient) failed: 0x%08X", hr);
        return false;
    }

    // Get mix format
    WAVEFORMATEX* mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        LOG_ERRORF("CaptureStream: GetMixFormat failed: 0x%08X", hr);
        return false;
    }

    m_sampleRate = mixFormat->nSamplesPerSec;
    m_channels = mixFormat->nChannels;
    m_bitsPerSample = mixFormat->wBitsPerSample;

    LOG_INFOF("CaptureStream: mix format = %uHz %uch %ubit tag=0x%04X",
              m_sampleRate, m_channels, m_bitsPerSample, mixFormat->wFormatTag);

    // Check if format is float
    if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        m_isFloat = true;
    } else if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        m_isFloat = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
        LOG_INFOF("CaptureStream: EXTENSIBLE subformat isFloat=%s validBits=%u",
                  m_isFloat ? "yes" : "no", ext->Samples.wValidBitsPerSample);
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

    if (FAILED(hr)) {
        LOG_ERRORF("CaptureStream: IAudioClient::Initialize failed: 0x%08X", hr);
        return false;
    }

    // Get capture client
    hr = m_audioClient->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(m_captureClient.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("CaptureStream: GetService(IAudioCaptureClient) failed: 0x%08X", hr);
        return false;
    }

    LOG_INFOF("CaptureStream: initialized OK — %uHz %uch %ubit isFloat=%s loopback=%s",
              m_sampleRate, m_channels, m_bitsPerSample,
              m_isFloat ? "yes" : "no", isLoopback ? "yes" : "no");
    return true;
}

bool AudioCaptureStream::start()
{
    if (m_running || !m_audioClient) return false;

    m_packetCount = 0;
    m_frameCount = 0;
    m_silentCount = 0;
    m_emptyCount = 0;

    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) {
        LOG_ERRORF("CaptureStream: Start() failed: 0x%08X", hr);
        return false;
    }

    m_running = true;
    LOG_INFOF("CaptureStream: started (loopback=%s %uHz %uch)",
              m_isLoopback ? "yes" : "no", m_sampleRate, m_channels);
    return true;
}

bool AudioCaptureStream::stop()
{
    if (!m_running || !m_audioClient) return false;

    m_audioClient->Stop();
    m_running = false;
    LOG_INFOF("CaptureStream: stopped (packets=%u frames=%u silent=%u emptyPolls=%u)",
              m_packetCount, m_frameCount, m_silentCount, m_emptyCount);
    return true;
}

size_t AudioCaptureStream::readFrames(float* buffer, size_t maxFrames)
{
    if (!m_running || !m_captureClient) return 0;

    size_t totalFrames = 0;

    while (totalFrames < maxFrames) {
        UINT32 packetLength = 0;
        HRESULT hr = m_captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) {
            LOG_ERRORF("CaptureStream: GetNextPacketSize failed: 0x%08X", hr);
            break;
        }
        if (packetLength == 0) {
            m_emptyCount++;
            // Log sparse empty-poll warnings to diagnose no-data situations
            if (m_emptyCount % 500 == 1) {
                LOG_DEBUGF("CaptureStream: no data (emptyPolls=%u)", m_emptyCount);
            }
            break;
        }

        BYTE* data = nullptr;
        UINT32 numFrames = 0;
        DWORD flags = 0;

        hr = m_captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
        if (FAILED(hr)) {
            LOG_ERRORF("CaptureStream: GetBuffer failed: 0x%08X", hr);
            break;
        }

        size_t framesToCopy = std::min(static_cast<size_t>(numFrames), maxFrames - totalFrames);

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            m_silentCount++;
            // Fill with silence
            std::memset(buffer + totalFrames * 2, 0, framesToCopy * 2 * sizeof(float));
        } else {
            // Convert to interleaved float32 stereo
            convertToFloat(data, buffer + totalFrames * 2,
                          static_cast<uint32_t>(framesToCopy),
                          nullptr);
        }

        if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
            LOG_WARNF("CaptureStream: DATA_DISCONTINUITY at packet=%u (gap/drop detected)",
                      m_packetCount);
        }

        hr = m_captureClient->ReleaseBuffer(numFrames);
        if (FAILED(hr)) {
            LOG_ERRORF("CaptureStream: ReleaseBuffer failed: 0x%08X", hr);
        }

        m_packetCount++;
        m_frameCount += numFrames;
        totalFrames += framesToCopy;

        // Periodic stats every 2000 packets
        if (m_packetCount % 2000 == 0) {
            LOG_INFOF("CaptureStream stats: packets=%u frames=%u silent=%u emptyPolls=%u",
                      m_packetCount, m_frameCount, m_silentCount, m_emptyCount);
        }
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
