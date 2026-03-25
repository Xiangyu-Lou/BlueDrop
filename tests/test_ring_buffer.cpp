#include <gtest/gtest.h>
#include "audio/RingBuffer.h"

#include <thread>
#include <vector>
#include <numeric>

using namespace BlueDrop;

TEST(RingBufferTest, CapacityIsPowerOf2) {
    RingBuffer buf(1000);
    EXPECT_EQ(buf.capacity(), 1024u);

    RingBuffer buf2(4096);
    EXPECT_EQ(buf2.capacity(), 4096u);
}

TEST(RingBufferTest, InitiallyEmpty) {
    RingBuffer buf(256);
    EXPECT_EQ(buf.availableRead(), 0u);
    EXPECT_EQ(buf.availableWrite(), 256u);
}

TEST(RingBufferTest, WriteAndRead) {
    RingBuffer buf(256);
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4] = {};

    size_t written = buf.write(data, 4);
    EXPECT_EQ(written, 4u);
    EXPECT_EQ(buf.availableRead(), 4u);

    size_t read = buf.read(output, 4);
    EXPECT_EQ(read, 4u);
    EXPECT_FLOAT_EQ(output[0], 1.0f);
    EXPECT_FLOAT_EQ(output[1], 2.0f);
    EXPECT_FLOAT_EQ(output[2], 3.0f);
    EXPECT_FLOAT_EQ(output[3], 4.0f);
}

TEST(RingBufferTest, WrapAround) {
    RingBuffer buf(64);

    std::vector<float> data(60, 1.0f);
    std::vector<float> output(60);

    // Fill most of the buffer
    buf.write(data.data(), 60);
    buf.read(output.data(), 60);

    // Now write again - this should wrap around
    std::vector<float> data2(30, 2.0f);
    buf.write(data2.data(), 30);

    EXPECT_EQ(buf.availableRead(), 30u);

    std::vector<float> output2(30);
    buf.read(output2.data(), 30);

    for (size_t i = 0; i < 30; i++) {
        EXPECT_FLOAT_EQ(output2[i], 2.0f) << "Mismatch at index " << i;
    }
}

TEST(RingBufferTest, OverflowProtection) {
    RingBuffer buf(64);

    std::vector<float> data(100, 1.0f);
    size_t written = buf.write(data.data(), 100);
    EXPECT_EQ(written, 64u); // Can only write up to capacity
}

TEST(RingBufferTest, UnderflowProtection) {
    RingBuffer buf(64);

    float output[10] = {};
    size_t read = buf.read(output, 10);
    EXPECT_EQ(read, 0u); // Nothing to read
}

TEST(RingBufferTest, Clear) {
    RingBuffer buf(64);
    float data[] = {1.0f, 2.0f};
    buf.write(data, 2);
    EXPECT_EQ(buf.availableRead(), 2u);

    buf.clear();
    EXPECT_EQ(buf.availableRead(), 0u);
    EXPECT_EQ(buf.availableWrite(), 64u);
}

TEST(RingBufferTest, ConcurrentProducerConsumer) {
    constexpr size_t TOTAL = 100000;
    RingBuffer buf(4096);

    std::vector<float> produced(TOTAL);
    std::iota(produced.begin(), produced.end(), 0.0f);

    std::vector<float> consumed(TOTAL, -1.0f);

    // Producer thread
    std::thread producer([&]() {
        size_t offset = 0;
        while (offset < TOTAL) {
            size_t chunk = std::min<size_t>(128, TOTAL - offset);
            size_t written = buf.write(produced.data() + offset, chunk);
            offset += written;
            if (written == 0) {
                std::this_thread::yield();
            }
        }
    });

    // Consumer thread
    std::thread consumer([&]() {
        size_t offset = 0;
        while (offset < TOTAL) {
            size_t chunk = std::min<size_t>(128, TOTAL - offset);
            size_t readCount = buf.read(consumed.data() + offset, chunk);
            offset += readCount;
            if (readCount == 0) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify all data transferred correctly
    for (size_t i = 0; i < TOTAL; i++) {
        EXPECT_FLOAT_EQ(consumed[i], produced[i]) << "Mismatch at index " << i;
    }
}
