/**
 * @file test_settings_dialog.cpp
 * @brief Unit tests for SettingsDialog class
 * 
 * Tests parameter validation for barcode settings.
 * Requirements: 2.1, 2.4
 */

#include <gtest/gtest.h>
#include "settings_dialog.h"

using namespace creo_barcode;

class SettingsDialogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default valid config
        validConfig_.type = BarcodeType::CODE_128;
        validConfig_.width = 200;
        validConfig_.height = 80;
        validConfig_.margin = 10;
        validConfig_.showText = true;
        validConfig_.dpi = 300;
    }
    
    BarcodeConfig validConfig_;
};

// Width validation tests
TEST_F(SettingsDialogTest, ValidateWidth_ValidValue) {
    auto result = SettingsDialog::validateWidth(200);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(SettingsDialogTest, ValidateWidth_MinValue) {
    auto result = SettingsDialog::validateWidth(SettingsDialog::MIN_WIDTH);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateWidth_MaxValue) {
    auto result = SettingsDialog::validateWidth(SettingsDialog::MAX_WIDTH);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateWidth_BelowMin) {
    auto result = SettingsDialog::validateWidth(SettingsDialog::MIN_WIDTH - 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "width");
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(SettingsDialogTest, ValidateWidth_AboveMax) {
    auto result = SettingsDialog::validateWidth(SettingsDialog::MAX_WIDTH + 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "width");
}

// Height validation tests
TEST_F(SettingsDialogTest, ValidateHeight_ValidValue) {
    auto result = SettingsDialog::validateHeight(80);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateHeight_MinValue) {
    auto result = SettingsDialog::validateHeight(SettingsDialog::MIN_HEIGHT);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateHeight_MaxValue) {
    auto result = SettingsDialog::validateHeight(SettingsDialog::MAX_HEIGHT);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateHeight_BelowMin) {
    auto result = SettingsDialog::validateHeight(SettingsDialog::MIN_HEIGHT - 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "height");
}

TEST_F(SettingsDialogTest, ValidateHeight_AboveMax) {
    auto result = SettingsDialog::validateHeight(SettingsDialog::MAX_HEIGHT + 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "height");
}

// Margin validation tests
TEST_F(SettingsDialogTest, ValidateMargin_ValidValue) {
    auto result = SettingsDialog::validateMargin(10);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateMargin_MinValue) {
    auto result = SettingsDialog::validateMargin(SettingsDialog::MIN_MARGIN);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateMargin_MaxValue) {
    auto result = SettingsDialog::validateMargin(SettingsDialog::MAX_MARGIN);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateMargin_BelowMin) {
    auto result = SettingsDialog::validateMargin(SettingsDialog::MIN_MARGIN - 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "margin");
}

TEST_F(SettingsDialogTest, ValidateMargin_AboveMax) {
    auto result = SettingsDialog::validateMargin(SettingsDialog::MAX_MARGIN + 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "margin");
}

// DPI validation tests
TEST_F(SettingsDialogTest, ValidateDpi_ValidValue) {
    auto result = SettingsDialog::validateDpi(300);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateDpi_MinValue) {
    auto result = SettingsDialog::validateDpi(SettingsDialog::MIN_DPI);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateDpi_MaxValue) {
    auto result = SettingsDialog::validateDpi(SettingsDialog::MAX_DPI);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateDpi_BelowMin) {
    auto result = SettingsDialog::validateDpi(SettingsDialog::MIN_DPI - 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "dpi");
}

TEST_F(SettingsDialogTest, ValidateDpi_AboveMax) {
    auto result = SettingsDialog::validateDpi(SettingsDialog::MAX_DPI + 1);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "dpi");
}

// Full config validation tests
TEST_F(SettingsDialogTest, ValidateConfig_ValidConfig) {
    auto result = SettingsDialog::validateConfig(validConfig_);
    EXPECT_TRUE(result.valid);
}

TEST_F(SettingsDialogTest, ValidateConfig_InvalidWidth) {
    validConfig_.width = 10; // Below minimum
    auto result = SettingsDialog::validateConfig(validConfig_);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "width");
}

TEST_F(SettingsDialogTest, ValidateConfig_InvalidHeight) {
    validConfig_.height = 5; // Below minimum
    auto result = SettingsDialog::validateConfig(validConfig_);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "height");
}

TEST_F(SettingsDialogTest, ValidateConfig_InvalidMargin) {
    validConfig_.margin = -1; // Below minimum
    auto result = SettingsDialog::validateConfig(validConfig_);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "margin");
}

TEST_F(SettingsDialogTest, ValidateConfig_InvalidDpi) {
    validConfig_.dpi = 50; // Below minimum
    auto result = SettingsDialog::validateConfig(validConfig_);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.fieldName, "dpi");
}

TEST_F(SettingsDialogTest, ValidateConfig_AllBarcodeTypes) {
    // Test all valid barcode types
    std::vector<BarcodeType> types = {
        BarcodeType::CODE_128,
        BarcodeType::CODE_39,
        BarcodeType::QR_CODE,
        BarcodeType::DATA_MATRIX,
        BarcodeType::EAN_13
    };
    
    for (auto type : types) {
        validConfig_.type = type;
        auto result = SettingsDialog::validateConfig(validConfig_);
        EXPECT_TRUE(result.valid) << "Failed for barcode type: " << static_cast<int>(type);
    }
}

// Barcode type names test
TEST_F(SettingsDialogTest, GetBarcodeTypeNames_ReturnsAllTypes) {
    auto names = SettingsDialog::getBarcodeTypeNames();
    EXPECT_EQ(names.size(), 5);
    EXPECT_EQ(names[0], "Code 128");
    EXPECT_EQ(names[1], "Code 39");
    EXPECT_EQ(names[2], "QR Code");
    EXPECT_EQ(names[3], "Data Matrix");
    EXPECT_EQ(names[4], "EAN-13");
}

// Dialog show test (non-Creo mode)
TEST_F(SettingsDialogTest, Show_NonCreoMode_ReturnsNotAccepted) {
    SettingsDialog dialog;
    auto result = dialog.show(validConfig_);
    
    // In non-Creo mode, dialog cannot be shown interactively
    EXPECT_FALSE(result.accepted);
    // Config should be preserved
    EXPECT_EQ(result.config.type, validConfig_.type);
    EXPECT_EQ(result.config.width, validConfig_.width);
    EXPECT_EQ(result.config.height, validConfig_.height);
}

// ValidationResult helper tests
TEST_F(SettingsDialogTest, ValidationResult_Success) {
    auto result = ValidationResult::success();
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(SettingsDialogTest, ValidationResult_Failure) {
    auto result = ValidationResult::failure("Test error", "testField");
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.errorMessage, "Test error");
    EXPECT_EQ(result.fieldName, "testField");
}
