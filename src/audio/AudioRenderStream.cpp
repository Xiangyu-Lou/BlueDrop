#include "AudioRenderStream.h"
#include "system/Logger.h"

#include <algorithm>
#include <cstring>

namespace BlueDrop {

AudioRenderStream::~AudioRenderStream()
{
    stop();
    LOG_DEBUGF("RenderStream destroyed (writes=%u framesWritten=%u bufferFull=%u)",
               m_writeCount, m_totalFramesWritten, m_fullCount);
}

bool AudioRenderStream::initialize(const wchar_t* deviceId, REFERENCE_TIME bufferDuration)
{
    LOG_INFOF("RenderStream::initialize() bufferDuration=%lld (%.0fms) deviceId=%.60S",
              bufferDuration, bufferDuration / 10000.0, deviceId);

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: CoCreateInstance(MMDeviceEnumerator) failed: 0x%08X", hr);
        return false;
    }

    hr = enumerator->GetDevice(deviceId, m_device.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: GetDevice failed: 0x%08X", hr);
        return false;
    }

    hr = m_device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(m_audioClient.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: Activate(IAudioClient) failed: 0x%08X", hr);
        return false;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: GetMixFormat failed: 0x%08X", hr);
        return false;
    }

    m_sampleRate = mixFormat->nSamplesPerSec;
    m_channels = mixFormat->nChannels;

    LOG_INFOF("RenderStream: mix format = %uHz %uch %ubit tag=0x%04X",
              m_sampleRate, m_channels, mixFormat->wBitsPerSample, mixFormat->wFormatTag);

    if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        m_isFloat = true;
    } else if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        m_isFloat = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferDuration,
        0,
        mixFormat,
        nullptr);

    CoTaskMemFree(mixFormat);
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: IAudioClient::Initialize failed: 0x%08X", hr);
        return false;
    }

    hr = m_audioClient->GetBufferSize(&m_bufferFrames);
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: GetBufferSize failed: 0x%08X", hr);
        return false;
    }

    hr = m_audioClient->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(m_renderClient.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: GetService(IAudioRenderClient) failed: 0x%08X", hr);
        return false;
    }

    LOG_INFOF("RenderStream: initialized OK — %uHz %uch bufferFrames=%u (%.0fms) isFloat=%s",
              m_sampleRate, m_channels, m_bufferFrames,
              m_sampleRate > 0 ? m_bufferFrames * 1000.0f / m_sampleRate : 0.f,
              m_isFloat ? "yes" : "no");
    return true;
}

bool AudioRenderStream::start()
{
    if (m_running || !m_audioClient) return false;

    m_writeCount = 0;
    m_totalFramesWritten = 0;
    m_fullCount = 0;

    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: Start() failed: 0x%08X", hr);
        return false;
    }

    m_running = true;
    LOG_INFOF("RenderStream: started (%uHz %uch bufferFrames=%u)",
              m_sampleRate, m_channels, m_bufferFrames);
    return true;
}

bool AudioRenderStream::stop()
{
    if (!m_running || !m_audioClient) return false;

    m_audioClient->Stop();
    m_running = false;
    LOG_INFOF("RenderStream: stopped (writes=%u framesWritten=%u bufferFull=%u)",
              m_writeCount, m_totalFramesWritten, m_fullCount);
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
    if (toWrite == 0) {
        m_fullCount++;
        if (m_fullCount % 50 == 1) {
            LOG_WARNF("RenderStream: buffer full, write skipped (frameCount=%zu available=%zu fullCount=%u)",
                      frameCount, available, m_fullCount);
        }
        return 0;
    }

    BYTE* buffer = nullptr;
    HRESULT hr = m_renderClient->GetBuffer(static_cast<UINT32>(toWrite), &buffer);
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: GetBuffer failed: 0x%08X", hr);
        return 0;
    }

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

    hr = m_renderClient->ReleaseBuffer(static_cast<UINT32>(toWrite), 0);
    if (FAILED(hr)) {
        LOG_ERRORF("RenderStream: ReleaseBuffer failed: 0x%08X", hr);
    }

    m_writeCount++;
    m_totalFramesWritten += static_cast<uint32_t>(toWrite);

    // Periodic stats every 2000 writes
    if (m_writeCount % 2000 == 0) {
        LOG_INFOF("RenderStream stats: writes=%u framesWritten=%u bufferFull=%u",
                  m_writeCount, m_totalFramesWritten, m_fullCount);
    }

    return toWrite;
}

} // namespace BlueDrop
