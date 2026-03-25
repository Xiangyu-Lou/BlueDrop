#pragma once

#include <cstddef>
#include <vector>

namespace BlueDrop {

// Simple linear interpolation resampler for converting between sample rates.
// Works on interleaved stereo float samples.
class Resampler {
public:
    Resampler(uint32_t sourceRate, uint32_t targetRate, uint32_t channels = 2)
        : m_sourceRate(sourceRate)
        , m_targetRate(targetRate)
        , m_channels(channels)
        , m_phase(0.0)
        , m_ratio(static_cast<double>(sourceRate) / static_cast<double>(targetRate))
    {
        m_lastFrame.resize(channels, 0.0f);
    }

    // Process interleaved input samples, produce interleaved output.
    // Returns the number of output frames written.
    size_t process(const float* input, size_t inputFrames,
                   float* output, size_t maxOutputFrames) {
        if (m_sourceRate == m_targetRate) {
            size_t frames = std::min(inputFrames, maxOutputFrames);
            std::memcpy(output, input, frames * m_channels * sizeof(float));
            return frames;
        }

        size_t outFrames = 0;

        while (outFrames < maxOutputFrames) {
            size_t srcIndex = static_cast<size_t>(m_phase);
            double frac = m_phase - static_cast<double>(srcIndex);

            if (srcIndex + 1 > inputFrames) break;

            for (uint32_t ch = 0; ch < m_channels; ch++) {
                float s0, s1;

                if (srcIndex == 0 && m_phase < 1.0) {
                    s0 = m_lastFrame[ch];
                } else if (srcIndex > 0) {
                    s0 = input[(srcIndex - 1) * m_channels + ch];
                } else {
                    s0 = m_lastFrame[ch];
                }

                if (srcIndex < inputFrames) {
                    s1 = input[srcIndex * m_channels + ch];
                } else {
                    s1 = s0;
                }

                output[outFrames * m_channels + ch] =
                    static_cast<float>(s0 + (s1 - s0) * frac);
            }

            outFrames++;
            m_phase += m_ratio;
        }

        // Save last frame for next block
        if (inputFrames > 0) {
            for (uint32_t ch = 0; ch < m_channels; ch++) {
                m_lastFrame[ch] = input[(inputFrames - 1) * m_channels + ch];
            }
        }

        m_phase -= static_cast<double>(inputFrames);
        if (m_phase < 0.0) m_phase = 0.0;

        return outFrames;
    }

    void reset() {
        m_phase = 0.0;
        std::fill(m_lastFrame.begin(), m_lastFrame.end(), 0.0f);
    }

    bool needsResampling() const { return m_sourceRate != m_targetRate; }
    uint32_t sourceRate() const { return m_sourceRate; }
    uint32_t targetRate() const { return m_targetRate; }

private:
    uint32_t m_sourceRate;
    uint32_t m_targetRate;
    uint32_t m_channels;
    double m_phase;
    double m_ratio;
    std::vector<float> m_lastFrame;
};

} // namespace BlueDrop
