#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <algorithm>

namespace BlueDrop {

// Lock-free Single-Producer Single-Consumer ring buffer for audio samples.
// Size must be a power of 2.
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity = 65536)
        : m_capacity(nextPowerOf2(capacity))
        , m_mask(m_capacity - 1)
        , m_buffer(new float[m_capacity])
    {
        std::memset(m_buffer.get(), 0, m_capacity * sizeof(float));
    }

    // Write samples into the buffer (producer side).
    // Returns number of samples actually written.
    size_t write(const float* data, size_t count) {
        size_t writePos = m_writePos.load(std::memory_order_relaxed);
        size_t readPos = m_readPos.load(std::memory_order_acquire);

        size_t available = m_capacity - (writePos - readPos);
        size_t toWrite = std::min(count, available);

        if (toWrite == 0) return 0;

        size_t writeIndex = writePos & m_mask;
        size_t firstChunk = std::min(toWrite, m_capacity - writeIndex);
        size_t secondChunk = toWrite - firstChunk;

        std::memcpy(m_buffer.get() + writeIndex, data, firstChunk * sizeof(float));
        if (secondChunk > 0) {
            std::memcpy(m_buffer.get(), data + firstChunk, secondChunk * sizeof(float));
        }

        m_writePos.store(writePos + toWrite, std::memory_order_release);
        return toWrite;
    }

    // Read samples from the buffer (consumer side).
    // Returns number of samples actually read.
    size_t read(float* data, size_t count) {
        size_t readPos = m_readPos.load(std::memory_order_relaxed);
        size_t writePos = m_writePos.load(std::memory_order_acquire);

        size_t available = writePos - readPos;
        size_t toRead = std::min(count, available);

        if (toRead == 0) return 0;

        size_t readIndex = readPos & m_mask;
        size_t firstChunk = std::min(toRead, m_capacity - readIndex);
        size_t secondChunk = toRead - firstChunk;

        std::memcpy(data, m_buffer.get() + readIndex, firstChunk * sizeof(float));
        if (secondChunk > 0) {
            std::memcpy(data + firstChunk, m_buffer.get(), secondChunk * sizeof(float));
        }

        m_readPos.store(readPos + toRead, std::memory_order_release);
        return toRead;
    }

    size_t availableRead() const {
        return m_writePos.load(std::memory_order_acquire) - m_readPos.load(std::memory_order_relaxed);
    }

    size_t availableWrite() const {
        return m_capacity - availableRead();
    }

    size_t capacity() const { return m_capacity; }

    void clear() {
        m_readPos.store(0, std::memory_order_relaxed);
        m_writePos.store(0, std::memory_order_relaxed);
    }

private:
    static size_t nextPowerOf2(size_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        v++;
        return v < 64 ? 64 : v;
    }

    size_t m_capacity;
    size_t m_mask;
    std::unique_ptr<float[]> m_buffer;
    alignas(64) std::atomic<size_t> m_writePos{0};
    alignas(64) std::atomic<size_t> m_readPos{0};
};

} // namespace BlueDrop
