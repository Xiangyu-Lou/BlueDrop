#include <gtest/gtest.h>
#include <Windows.h>
#include "bluetooth/BluetoothTypes.h"
#include "bluetooth/BluetoothManager.h"

using namespace BlueDrop;

class BluetoothManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    void TearDown() override {
        CoUninitialize();
    }
};

TEST_F(BluetoothManagerTest, GetDeviceSelectorReturnsNonEmpty) {
    auto selector = BluetoothManager::getDeviceSelector();
    // This should return a valid AQS string on any Win10 2004+ machine
    EXPECT_FALSE(selector.isEmpty())
        << "GetDeviceSelector() returned empty string - "
        << "AudioPlaybackConnection API may not be available";
}

TEST_F(BluetoothManagerTest, InitialStateIsDisconnected) {
    BluetoothManager manager;
    EXPECT_EQ(manager.state(), BluetoothConnectionState::Disconnected);
    EXPECT_TRUE(manager.connectedDeviceName().isEmpty());
}

TEST_F(BluetoothManagerTest, StartListeningChangesState) {
    BluetoothManager manager;
    manager.startListening();

    // Should transition to WaitingPair
    EXPECT_EQ(manager.state(), BluetoothConnectionState::WaitingPair);

    manager.stopListening();
    EXPECT_EQ(manager.state(), BluetoothConnectionState::Disconnected);
}

TEST_F(BluetoothManagerTest, StopListeningDoesNotCrash) {
    BluetoothManager manager;

    // Stop without start should be safe
    manager.stopListening();
    EXPECT_EQ(manager.state(), BluetoothConnectionState::Disconnected);

    // Start then stop
    manager.startListening();
    manager.stopListening();
    EXPECT_EQ(manager.state(), BluetoothConnectionState::Disconnected);
}

TEST_F(BluetoothManagerTest, DoubleStartIsSafe) {
    BluetoothManager manager;
    manager.startListening();
    manager.startListening(); // Should not crash or duplicate watchers
    EXPECT_EQ(manager.state(), BluetoothConnectionState::WaitingPair);
    manager.stopListening();
}

TEST_F(BluetoothManagerTest, DisconnectWithoutConnectionIsSafe) {
    BluetoothManager manager;
    // Disconnect when nothing is connected should be safe
    manager.disconnect();
    EXPECT_EQ(manager.state(), BluetoothConnectionState::Disconnected);
}

TEST_F(BluetoothManagerTest, ConnectionStateEnumValues) {
    // Verify enum values are distinct
    EXPECT_NE(BluetoothConnectionState::Disconnected, BluetoothConnectionState::WaitingPair);
    EXPECT_NE(BluetoothConnectionState::WaitingPair, BluetoothConnectionState::Connecting);
    EXPECT_NE(BluetoothConnectionState::Connecting, BluetoothConnectionState::Connected);
    EXPECT_NE(BluetoothConnectionState::Connected, BluetoothConnectionState::Reconnecting);
}
