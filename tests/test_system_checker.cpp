#include <gtest/gtest.h>
#include <Windows.h>
#include "system/SystemChecker.h"

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
    EXPECT_FALSE(result.osVersionString.isEmpty());
    EXPECT_GT(result.osBuildNumber, 0u);
}

TEST_F(SystemCheckerTest, OsVersionIsWin10_2004OrHigher) {
    SystemCheckResult result;
    SystemChecker::checkOsVersion(result);
    EXPECT_TRUE(result.osVersionOk)
        << "OS Build: " << result.osBuildNumber
        << " (" << result.osVersionString.toStdString() << ")";
}

TEST_F(SystemCheckerTest, BluetoothCheckDoesNotCrash) {
    SystemCheckResult result;
    SystemChecker::checkBluetoothAdapter(result);
    if (result.bluetoothAvailable) {
        EXPECT_FALSE(result.bluetoothAdapterName.isEmpty());
    }
}

TEST_F(SystemCheckerTest, VBCableCheckDoesNotCrash) {
    SystemCheckResult result;
    SystemChecker::checkVBCable(result);
    if (result.vbCableInstalled) {
        EXPECT_FALSE(result.vbCableDeviceName.isEmpty());
    }
}

TEST_F(SystemCheckerTest, RunAllChecksDoesNotCrash) {
    auto result = SystemChecker::runAllChecks();
    EXPECT_TRUE(result.osVersionOk);
    EXPECT_FALSE(result.osVersionString.isEmpty());
    [[maybe_unused]] bool passed = result.allPassed();
}

TEST_F(SystemCheckerTest, FailureMessageSetOnFirstFailure) {
    auto fullResult = SystemChecker::runAllChecks();
    if (!fullResult.allPassed()) {
        EXPECT_FALSE(fullResult.failureMessage.isEmpty())
            << "failureMessage should be set when a check fails";
    }
}
