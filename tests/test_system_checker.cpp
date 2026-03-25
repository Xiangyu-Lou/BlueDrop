#include <gtest/gtest.h>
#include <Windows.h>

// We test SystemChecker logic directly since it depends on the real system.
// These tests verify the module doesn't crash and returns sensible values.

// Include implementation directly to avoid complex CMake target linking
#include "../src/system/SystemChecker.h"
#include "../src/system/SystemChecker.cpp"

using namespace BlueDrop;

class SystemCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    void TearDown() override {
        CoUninitialize();
    }
};

TEST_F(SystemCheckerTest, OsVersionCheckDoesNotCrash) {
    SystemCheckResult result;
    SystemChecker::checkOsVersion(result);

    // On any modern dev machine, this should return a valid version string
    EXPECT_FALSE(result.osVersionString.isEmpty());
    EXPECT_GT(result.osBuildNumber, 0u);
}

TEST_F(SystemCheckerTest, OsVersionIsWin10_2004OrHigher) {
    SystemCheckResult result;
    SystemChecker::checkOsVersion(result);

    // Dev machine should be Win10 2004+
    EXPECT_TRUE(result.osVersionOk)
        << "OS Build: " << result.osBuildNumber
        << " (" << result.osVersionString.toStdString() << ")";
}

TEST_F(SystemCheckerTest, BluetoothCheckDoesNotCrash) {
    SystemCheckResult result;
    // Just verify it doesn't crash - BT adapter may or may not be present
    SystemChecker::checkBluetoothAdapter(result);

    if (result.bluetoothAvailable) {
        // If BT is found, adapter name should be non-empty
        EXPECT_FALSE(result.bluetoothAdapterName.isEmpty());
    }
}

TEST_F(SystemCheckerTest, VBCableCheckDoesNotCrash) {
    SystemCheckResult result;
    // Just verify it doesn't crash - VB-Cable may or may not be installed
    SystemChecker::checkVBCable(result);

    if (result.vbCableInstalled) {
        EXPECT_FALSE(result.vbCableDeviceName.isEmpty());
    }
}

TEST_F(SystemCheckerTest, RunAllChecksDoesNotCrash) {
    auto result = SystemChecker::runAllChecks();

    // OS version should always be valid on dev machine
    EXPECT_TRUE(result.osVersionOk);
    EXPECT_FALSE(result.osVersionString.isEmpty());

    // allPassed depends on hardware - just verify the method works
    [[maybe_unused]] bool passed = result.allPassed();
}

TEST_F(SystemCheckerTest, FailureMessageSetOnFirstFailure) {
    SystemCheckResult result;
    // Simulate: OS passes, but if BT or VBCable fail, message should be set
    auto fullResult = SystemChecker::runAllChecks();

    if (!fullResult.allPassed()) {
        EXPECT_FALSE(fullResult.failureMessage.isEmpty())
            << "failureMessage should be set when a check fails";
    }
}
