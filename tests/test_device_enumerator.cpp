#include <gtest/gtest.h>
#include <Windows.h>
#include "audio/DeviceEnumerator.h"

using namespace BlueDrop;

class DeviceEnumeratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    void TearDown() override {
        CoUninitialize();
    }
};

TEST_F(DeviceEnumeratorTest, InitializeSucceeds) {
    DeviceEnumerator enumerator;
    EXPECT_TRUE(enumerator.initialize());
}

TEST_F(DeviceEnumeratorTest, InputDevicesNotEmpty) {
    DeviceEnumerator enumerator;
    ASSERT_TRUE(enumerator.initialize());

    auto inputs = enumerator.inputDevices();
    if (!inputs.isEmpty()) {
        EXPECT_FALSE(inputs.first().id.isEmpty());
        EXPECT_FALSE(inputs.first().displayName.isEmpty());
    }
}

TEST_F(DeviceEnumeratorTest, OutputDevicesNotEmpty) {
    DeviceEnumerator enumerator;
    ASSERT_TRUE(enumerator.initialize());

    auto outputs = enumerator.outputDevices();
    ASSERT_FALSE(outputs.isEmpty()) << "No output devices found";
    EXPECT_FALSE(outputs.first().id.isEmpty());
    EXPECT_FALSE(outputs.first().displayName.isEmpty());
}

TEST_F(DeviceEnumeratorTest, DefaultDeviceMarked) {
    DeviceEnumerator enumerator;
    ASSERT_TRUE(enumerator.initialize());

    auto outputs = enumerator.outputDevices();
    if (outputs.isEmpty()) GTEST_SKIP() << "No output devices";

    bool hasDefault = false;
    for (const auto& dev : outputs) {
        if (dev.isDefault) {
            hasDefault = true;
            break;
        }
    }
    EXPECT_TRUE(hasDefault) << "No default output device found";
}

TEST_F(DeviceEnumeratorTest, DeviceTypesCorrect) {
    DeviceEnumerator enumerator;
    ASSERT_TRUE(enumerator.initialize());

    for (const auto& dev : enumerator.inputDevices()) {
        EXPECT_EQ(dev.type, DeviceType::Input);
    }
    for (const auto& dev : enumerator.outputDevices()) {
        EXPECT_EQ(dev.type, DeviceType::Output);
    }
}

TEST_F(DeviceEnumeratorTest, RefreshDoesNotCrash) {
    DeviceEnumerator enumerator;
    ASSERT_TRUE(enumerator.initialize());

    enumerator.refresh();
    enumerator.refresh();
    enumerator.refresh();

    auto outputs = enumerator.outputDevices();
    EXPECT_FALSE(outputs.isEmpty());
}

TEST_F(DeviceEnumeratorTest, VBCableDetection) {
    DeviceEnumerator enumerator;
    ASSERT_TRUE(enumerator.initialize());

    [[maybe_unused]] bool installed = enumerator.isVBCableInstalled();
    [[maybe_unused]] QString id = enumerator.findVBCableInputId();

    if (installed) {
        EXPECT_FALSE(id.isEmpty()) << "VB-Cable detected but no device ID found";
    }
}
