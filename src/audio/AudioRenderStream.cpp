#include "AudioRenderStream.h"

#include <algorithm>
#include <cstring>

namespace BlueDrop {

AudioRenderStream::~AudioRenderStream()
{
    stop();
}

bool AudioRenderStream::initialize(const wchar_t* deviceId)
{
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = enumerator->GetDevice(deviceId, m_device.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(m_audioClient.GetAddressOf()));
    if (FAILED(hr)) return false;

    WAVEFORMATEX* mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) return false;

    m_sampleRate = mixFormat->nSamplesPerSec;
    m_channels = mixFormat->nChannels;

    if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        m_isFloat = true;
    } else if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        m_isFloat = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    // 10ms buffer
    REFERENCE_TIME bufferDuration = 100000;

    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferDuration,
        0,
        mixFormat,
        nullptr);

    CoTaskMemFree(mixFormat);
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetBufferSize(&m_bufferFrames);
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(m_renderClient.GetAddressOf()));
    if (FAILED(hr)) return false;

    return true;
}

bool AudioRenderStream::start()
{
    if (m_running || !m_audioClient) return false;

    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) return false;

    m_running = true;
    return true;
}

bool AudioRenderStream::stop()
{
    if (!m_running || !m_audioClient) return false;

    m_audioClient->Stop();
    m_running = false;
    return true;
}

size_t AudioRenderStream::availableFrames() const
{
    if (!m_audioClient) return 0;

    UINT32 padding = 0;
    HRESULT hr = m_audioClient->GetCurrentPadding(&padding);
    if (FAILED(hr)) return 0;

    return m_bufferFrames - padding;
}

size_t AudioRenderStream::writeFrames(const float* data, size_t frameCount)
{
    if (!m_running || !m_renderClient) return 0;

    size_t available = availableFrames();
    size_t toWrite = std::min(frameCount, available);
    if (toWrite == 0) return 0;

    BYTE* buffer = nullptr;
    HRESULT hr = m_renderClient->GetBuffer(static_cast<UINT32>(toWrite), &buffer);
    if (FAILED(hr)) return 0;

    if (m_isFloat && m_channels == 2) {
        // Direct copy for float32 stereo
        std::memcpy(buffer, data, toWrite * 2 * sizeof(float));
    } else if (m_isFloat && m_channels > 2) {
        // Fill extra channels with zeros
        float* dst = reinterpret_cast<float*>(buffer);
        for (size_t i = 0; i < toWrite; i++) {
            dst[i * m_channels] = data[i * 2];
            dst[i * m_channels + 1] = data[i * 2 + 1];
            for (uint32_t ch = 2; ch < m_channels; ch++) {
                dst[i * m_channels + ch] = 0.0f;
            }
        }
    } else {
        // Unsupported format - write silence
        std::memset(buffer, 0, toWrite * m_channels * sizeof(float));
    }

    m_renderClient->ReleaseBuffer(static_cast<UINT32>(toWrite), 0);
    return toWrite;
}

} // namespace BlueDrop
