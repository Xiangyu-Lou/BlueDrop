#include <gtest/gtest.h>
#include <Windows.h>
#include "audio/AudioEngine.h"
#include "audio/Resampler.h"

using namespace BlueDrop;

class AudioEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    void TearDown() override {
        CoUninitialize();
    }
};

TEST_F(AudioEngineTest, InitialState) {
    AudioEngine engine;
    EXPECT_FALSE(engine.isRunning());
    EXPECT_FLOAT_EQ(engine.phoneVolume(), 1.0f);
    EXPECT_FLOAT_EQ(engine.micVolume(), 1.0f);
    EXPECT_FLOAT_EQ(engine.phoneMixRatio(), 1.0f);
    EXPECT_FALSE(engine.phoneMuted());
    EXPECT_FALSE(engine.micMuted());
}

TEST_F(AudioEngineTest, VolumeControls) {
    AudioEngine engine;

    engine.setPhoneVolume(0.5f);
    EXPECT_FLOAT_EQ(engine.phoneVolume(), 0.5f);

    engine.setMicVolume(1.5f);
    EXPECT_FLOAT_EQ(engine.micVolume(), 1.5f);

    engine.setPhoneMixRatio(0.7f);
    EXPECT_FLOAT_EQ(engine.phoneMixRatio(), 0.7f);

    // Clamping
    engine.setPhoneVolume(3.0f);
    EXPECT_FLOAT_EQ(engine.phoneVolume(), 2.0f);

    engine.setPhoneVolume(-1.0f);
    EXPECT_FLOAT_EQ(engine.phoneVolume(), 0.0f);
}

TEST_F(AudioEngineTest, MuteControls) {
    AudioEngine engine;

    engine.setPhoneMuted(true);
    EXPECT_TRUE(engine.phoneMuted());

    engine.setMicMuted(true);
    EXPECT_TRUE(engine.micMuted());

    engine.setPhoneMuted(false);
    EXPECT_FALSE(engine.phoneMuted());
}

TEST_F(AudioEngineTest, StartWithoutDeviceFails) {
    AudioEngine engine;
    // No devices set - should not crash, but emit error
    engine.start();
    // Without virtual cable device, it should not actually start
    EXPECT_FALSE(engine.isRunning());
}

TEST_F(AudioEngineTest, StopWithoutStartIsSafe) {
    AudioEngine engine;
    engine.stop();
    EXPECT_FALSE(engine.isRunning());
}

TEST_F(AudioEngineTest, BoostGainClamping) {
    AudioEngine engine;

    // Normal range
    engine.setBoostGain(1.0f);
    EXPECT_FLOAT_EQ(engine.boostGain(), 1.0f);

    engine.setBoostGain(5.0f);
    EXPECT_FLOAT_EQ(engine.boostGain(), 5.0f);

    // Max 10x
    engine.setBoostGain(10.0f);
    EXPECT_FLOAT_EQ(engine.boostGain(), 10.0f);

    // Above max -> clamped to 10
    engine.setBoostGain(15.0f);
    EXPECT_FLOAT_EQ(engine.boostGain(), 10.0f);

    // Below min -> clamped to 0
    engine.setBoostGain(-1.0f);
    EXPECT_FLOAT_EQ(engine.boostGain(), 0.0f);
}

TEST_F(AudioEngineTest, StopBoostWithoutStartIsSafe) {
    AudioEngine engine;
    // Should not crash or deadlock
    engine.stopBoost();
    EXPECT_FALSE(engine.isBoostRunning());
}

TEST_F(AudioEngineTest, BoostInitialGain) {
    AudioEngine engine;
    EXPECT_FLOAT_EQ(engine.boostGain(), 1.0f);
    EXPECT_FALSE(engine.isBoostRunning());
}

// Resampler tests
TEST(ResamplerTest, PassthroughWhenSameRate) {
    Resampler resampler(48000, 48000, 2);
    EXPECT_FALSE(resampler.needsResampling());

    float input[] = {0.1f, 0.2f, 0.3f, 0.4f}; // 2 stereo frames
    float output[4] = {};

    size_t frames = resampler.process(input, 2, output, 2);
    EXPECT_EQ(frames, 2u);
    EXPECT_FLOAT_EQ(output[0], 0.1f);
    EXPECT_FLOAT_EQ(output[1], 0.2f);
    EXPECT_FLOAT_EQ(output[2], 0.3f);
    EXPECT_FLOAT_EQ(output[3], 0.4f);
}

TEST(ResamplerTest, UpsampleProducesMoreFrames) {
    // 44100 -> 48000 should produce more output frames than input
    Resampler resampler(44100, 48000, 2);
    EXPECT_TRUE(resampler.needsResampling());

    constexpr size_t INPUT_FRAMES = 441;
    constexpr size_t MAX_OUTPUT = 600;
    std::vector<float> input(INPUT_FRAMES * 2, 0.5f);
    std::vector<float> output(MAX_OUTPUT * 2, 0.0f);

    size_t outFrames = resampler.process(input.data(), INPUT_FRAMES, output.data(), MAX_OUTPUT);
    // Expected: 441 * (48000/44100) ≈ 480
    EXPECT_GT(outFrames, INPUT_FRAMES);
    EXPECT_LE(outFrames, MAX_OUTPUT);
}

TEST(ResamplerTest, ResetWorks) {
    Resampler resampler(44100, 48000, 2);

    float input[] = {1.0f, 1.0f};
    float output[4] = {};
    resampler.process(input, 1, output, 2);

    resampler.reset();
    // After reset, internal state should be cleared
    EXPECT_EQ(resampler.sourceRate(), 44100u);
    EXPECT_EQ(resampler.targetRate(), 48000u);
}
