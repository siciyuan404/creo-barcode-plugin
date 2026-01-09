/**
 * @file test_data_sync_checker.cpp
 * @brief Unit tests for DataSyncChecker class
 * 
 * Tests for Requirements 3.1, 3.2, 3.3:
 * - Barcode and part name consistency check
 * - Update prompt functionality
 * - Warning display functionality
 */

#include <gtest/gtest.h>
#include "data_sync_checker.h"
#include "barcode_generator.h"

using namespace creo_barcode;

class DataSyncCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        checker = std::make_unique<DataSyncChecker>();
        generator = std::make_unique<BarcodeGenerator>();
    }
    
    void TearDown() override {
        checker.reset();
        generator.reset();
    }
    
    std::unique_ptr<DataSyncChecker> checker;
    std::unique_ptr<BarcodeGenerator> generator;
};

// Test: Check sync with matching data
TEST_F(DataSyncCheckerTest, CheckSync_MatchingData_ReturnsInSync) {
    BarcodeInstance instance;
    instance.decodedData = "PART_001";
    
    SyncCheckResult result = checker->checkSync("PART_001", instance);
    
    EXPECT_EQ(result.status, SyncStatus::IN_SYNC);
    EXPECT_TRUE(result.isInSync());
    EXPECT_FALSE(result.needsUpdate());
    EXPECT_EQ(result.currentPartName, "PART_001");
    EXPECT_EQ(result.barcodeData, "PART_001");
}

// Test: Check sync with mismatched data
TEST_F(DataSyncCheckerTest, CheckSync_MismatchedData_ReturnsOutOfSync) {
    BarcodeInstance instance;
    instance.decodedData = "OLD_PART_001";
    
    SyncCheckResult result = checker->checkSync("NEW_PART_001", instance);
    
    EXPECT_EQ(result.status, SyncStatus::OUT_OF_SYNC);
    EXPECT_FALSE(result.isInSync());
    EXPECT_TRUE(result.needsUpdate());
    EXPECT_TRUE(result.warningDisplayed);
    EXPECT_EQ(result.currentPartName, "NEW_PART_001");
    EXPECT_EQ(result.barcodeData, "OLD_PART_001");
}

// Test: Check sync with empty barcode data
TEST_F(DataSyncCheckerTest, CheckSync_EmptyBarcodeData_ReturnsBarcodeNotFound) {
    BarcodeInstance instance;
    instance.decodedData = "";
    
    SyncCheckResult result = checker->checkSync("PART_001", instance);
    
    EXPECT_EQ(result.status, SyncStatus::BARCODE_NOT_FOUND);
}

// Test: Check sync with empty part name
TEST_F(DataSyncCheckerTest, CheckSync_EmptyPartName_ReturnsUnknown) {
    BarcodeInstance instance;
    instance.decodedData = "PART_001";
    
    SyncCheckResult result = checker->checkSync("", instance);
    
    EXPECT_EQ(result.status, SyncStatus::UNKNOWN);
}

// Test: Prompt update when in sync (should return false)
TEST_F(DataSyncCheckerTest, PromptUpdate_InSync_ReturnsFalse) {
    SyncCheckResult result;
    result.status = SyncStatus::IN_SYNC;
    
    bool callbackCalled = false;
    auto callback = [&callbackCalled](const std::string&, const std::string&) {
        callbackCalled = true;
        return true;
    };
    
    bool shouldUpdate = checker->promptUpdate(result, callback);
    
    EXPECT_FALSE(shouldUpdate);
    EXPECT_FALSE(callbackCalled);
}

// Test: Prompt update when out of sync with user confirmation
TEST_F(DataSyncCheckerTest, PromptUpdate_OutOfSync_UserConfirms_ReturnsTrue) {
    SyncCheckResult result;
    result.status = SyncStatus::OUT_OF_SYNC;
    result.barcodeData = "OLD_DATA";
    result.currentPartName = "NEW_DATA";
    
    std::string receivedOldData, receivedNewData;
    auto callback = [&](const std::string& oldData, const std::string& newData) {
        receivedOldData = oldData;
        receivedNewData = newData;
        return true;
    };
    
    bool shouldUpdate = checker->promptUpdate(result, callback);
    
    EXPECT_TRUE(shouldUpdate);
    EXPECT_EQ(receivedOldData, "OLD_DATA");
    EXPECT_EQ(receivedNewData, "NEW_DATA");
}

// Test: Prompt update when out of sync with user decline
TEST_F(DataSyncCheckerTest, PromptUpdate_OutOfSync_UserDeclines_ReturnsFalse) {
    SyncCheckResult result;
    result.status = SyncStatus::OUT_OF_SYNC;
    result.barcodeData = "OLD_DATA";
    result.currentPartName = "NEW_DATA";
    
    auto callback = [](const std::string&, const std::string&) {
        return false;
    };
    
    bool shouldUpdate = checker->promptUpdate(result, callback);
    
    EXPECT_FALSE(shouldUpdate);
}

// Test: Prompt update with no callback
TEST_F(DataSyncCheckerTest, PromptUpdate_NoCallback_ReturnsFalse) {
    SyncCheckResult result;
    result.status = SyncStatus::OUT_OF_SYNC;
    
    bool shouldUpdate = checker->promptUpdate(result, nullptr);
    
    EXPECT_FALSE(shouldUpdate);
}

// Test: Display warning callback is called
TEST_F(DataSyncCheckerTest, DisplayWarning_CallbackIsCalled) {
    BarcodeInstance instance;
    instance.imagePath = "/path/to/barcode.png";
    instance.decodedData = "OLD_DATA";
    instance.posX = 100.0;
    instance.posY = 200.0;
    
    bool callbackCalled = false;
    std::string receivedMessage;
    BarcodeInstance receivedInstance;
    
    auto callback = [&](const std::string& message, const BarcodeInstance& inst) {
        callbackCalled = true;
        receivedMessage = message;
        receivedInstance = inst;
    };
    
    checker->displayWarning(instance, callback);
    
    EXPECT_TRUE(callbackCalled);
    EXPECT_FALSE(receivedMessage.empty());
    EXPECT_EQ(receivedInstance.imagePath, instance.imagePath);
    EXPECT_EQ(receivedInstance.decodedData, instance.decodedData);
}

// Test: Compare data with direct match
TEST_F(DataSyncCheckerTest, CompareData_DirectMatch_ReturnsTrue) {
    bool result = checker->compareData("PART_001", "PART_001", *generator);
    EXPECT_TRUE(result);
}

// Test: Compare data with no match
TEST_F(DataSyncCheckerTest, CompareData_NoMatch_ReturnsFalse) {
    bool result = checker->compareData("PART_001", "PART_002", *generator);
    EXPECT_FALSE(result);
}

// Test: Compare data with empty strings
TEST_F(DataSyncCheckerTest, CompareData_EmptyStrings_ReturnsFalse) {
    EXPECT_FALSE(checker->compareData("", "PART_001", *generator));
    EXPECT_FALSE(checker->compareData("PART_001", "", *generator));
    EXPECT_FALSE(checker->compareData("", "", *generator));
}

// Test: Compare data with encoded special characters
TEST_F(DataSyncCheckerTest, CompareData_EncodedSpecialChars_ReturnsTrue) {
    std::string partName = "PART 001";
    std::string encodedData = generator->encodeSpecialChars(partName);
    
    bool result = checker->compareData(partName, encodedData, *generator);
    EXPECT_TRUE(result);
}

// Test: Status message for each status
TEST_F(DataSyncCheckerTest, GetStatusMessage_AllStatuses) {
    EXPECT_FALSE(DataSyncChecker::getStatusMessage(SyncStatus::IN_SYNC).empty());
    EXPECT_FALSE(DataSyncChecker::getStatusMessage(SyncStatus::OUT_OF_SYNC).empty());
    EXPECT_FALSE(DataSyncChecker::getStatusMessage(SyncStatus::BARCODE_NOT_FOUND).empty());
    EXPECT_FALSE(DataSyncChecker::getStatusMessage(SyncStatus::DECODE_ERROR).empty());
    EXPECT_FALSE(DataSyncChecker::getStatusMessage(SyncStatus::UNKNOWN).empty());
}

// Test: SyncStatus to string conversion
TEST_F(DataSyncCheckerTest, SyncStatusToString_AllStatuses) {
    EXPECT_EQ(syncStatusToString(SyncStatus::IN_SYNC), "IN_SYNC");
    EXPECT_EQ(syncStatusToString(SyncStatus::OUT_OF_SYNC), "OUT_OF_SYNC");
    EXPECT_EQ(syncStatusToString(SyncStatus::BARCODE_NOT_FOUND), "BARCODE_NOT_FOUND");
    EXPECT_EQ(syncStatusToString(SyncStatus::DECODE_ERROR), "DECODE_ERROR");
    EXPECT_EQ(syncStatusToString(SyncStatus::UNKNOWN), "UNKNOWN");
}

// Test: Default callbacks can be set
TEST_F(DataSyncCheckerTest, SetDefaultCallbacks) {
    bool updateCallbackCalled = false;
    bool warningCallbackCalled = false;
    
    checker->setUpdateConfirmCallback([&](const std::string&, const std::string&) {
        updateCallbackCalled = true;
        return true;
    });
    
    checker->setWarningDisplayCallback([&](const std::string&, const BarcodeInstance&) {
        warningCallbackCalled = true;
    });
    
    // Test update callback
    SyncCheckResult result;
    result.status = SyncStatus::OUT_OF_SYNC;
    checker->promptUpdate(result, nullptr);
    EXPECT_TRUE(updateCallbackCalled);
    
    // Test warning callback
    BarcodeInstance instance;
    instance.decodedData = "TEST";
    checker->displayWarning(instance, nullptr);
    EXPECT_TRUE(warningCallbackCalled);
}

// Test: SyncCheckResult helper methods
TEST_F(DataSyncCheckerTest, SyncCheckResult_HelperMethods) {
    SyncCheckResult result;
    
    result.status = SyncStatus::IN_SYNC;
    EXPECT_TRUE(result.isInSync());
    EXPECT_FALSE(result.needsUpdate());
    
    result.status = SyncStatus::OUT_OF_SYNC;
    EXPECT_FALSE(result.isInSync());
    EXPECT_TRUE(result.needsUpdate());
    
    result.status = SyncStatus::BARCODE_NOT_FOUND;
    EXPECT_FALSE(result.isInSync());
    EXPECT_FALSE(result.needsUpdate());
}
