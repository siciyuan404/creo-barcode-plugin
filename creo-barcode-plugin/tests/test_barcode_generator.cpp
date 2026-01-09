#include <gtest/gtest.h>
#include "barcode_generator.h"
#include <filesystem>

namespace creo_barcode {
namespace testing {

class BarcodeGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test outputs
        testDir_ = std::filesystem::temp_directory_path() / "barcode_test";
        std::filesystem::create_directories(testDir_);
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
    BarcodeGenerator generator_;
};

// ============================================================================
// Data Validation Tests
// ============================================================================

TEST_F(BarcodeGeneratorTest, ValidateDataCode128AcceptsAscii) {
    EXPECT_TRUE(generator_.validateData("TEST123", BarcodeType::CODE_128));
    EXPECT_TRUE(generator_.validateData("Hello World!", BarcodeType::CODE_128));
}

TEST_F(BarcodeGeneratorTest, ValidateDataCode39AcceptsValidChars) {
    EXPECT_TRUE(generator_.validateData("TEST123", BarcodeType::CODE_39));
    EXPECT_TRUE(generator_.validateData("HELLO-WORLD", BarcodeType::CODE_39));
}

TEST_F(BarcodeGeneratorTest, ValidateDataCode39RejectsInvalidChars) {
    EXPECT_FALSE(generator_.validateData("test123", BarcodeType::CODE_39)); // lowercase
    EXPECT_FALSE(generator_.validateData("TEST@123", BarcodeType::CODE_39)); // @ symbol
}

TEST_F(BarcodeGeneratorTest, ValidateDataEAN13RequiresDigits) {
    EXPECT_TRUE(generator_.validateData("123456789012", BarcodeType::EAN_13));
    EXPECT_FALSE(generator_.validateData("12345678901", BarcodeType::EAN_13)); // too short
    EXPECT_FALSE(generator_.validateData("12345678901A", BarcodeType::EAN_13)); // contains letter
}

TEST_F(BarcodeGeneratorTest, ValidateDataRejectsEmpty) {
    EXPECT_FALSE(generator_.validateData("", BarcodeType::CODE_128));
    EXPECT_FALSE(generator_.validateData("", BarcodeType::QR_CODE));
}

TEST_F(BarcodeGeneratorTest, EncodeSpecialCharsHandlesAscii) {
    EXPECT_EQ(generator_.encodeSpecialChars("TEST123"), "TEST123");
    EXPECT_EQ(generator_.encodeSpecialChars("Hello World"), "Hello World");
}

// ============================================================================
// Invalid Input Handling Tests (需求 6.1)
// ============================================================================

TEST_F(BarcodeGeneratorTest, GenerateRejectsEmptyData) {
    BarcodeConfig config;
    std::string outputPath = (testDir_ / "empty.png").string();
    EXPECT_FALSE(generator_.generate("", config, outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateRejectsInvalidSize) {
    BarcodeConfig config;
    config.width = 0;
    std::string outputPath = (testDir_ / "invalid.png").string();
    EXPECT_FALSE(generator_.generate("TEST", config, outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateRejectsNegativeWidth) {
    BarcodeConfig config;
    config.width = -100;
    config.height = 80;
    std::string outputPath = (testDir_ / "negative_width.png").string();
    EXPECT_FALSE(generator_.generate("TEST123", config, outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateRejectsNegativeHeight) {
    BarcodeConfig config;
    config.width = 200;
    config.height = -50;
    std::string outputPath = (testDir_ / "negative_height.png").string();
    EXPECT_FALSE(generator_.generate("TEST123", config, outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateRejectsInvalidDataForCode39) {
    BarcodeConfig config;
    config.type = BarcodeType::CODE_39;
    std::string outputPath = (testDir_ / "invalid_code39.png").string();
    // lowercase letters are invalid for Code 39
    EXPECT_FALSE(generator_.generate("lowercase", config, outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateRejectsInvalidDataForEAN13) {
    BarcodeConfig config;
    config.type = BarcodeType::EAN_13;
    std::string outputPath = (testDir_ / "invalid_ean13.png").string();
    // EAN-13 requires exactly 12 or 13 digits
    EXPECT_FALSE(generator_.generate("12345", config, outputPath));
}

// ============================================================================
// Barcode Type Generation Tests (需求 2.2)
// ============================================================================

TEST_F(BarcodeGeneratorTest, GenerateCode128CreatesFile) {
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = 200;
    config.height = 80;
    std::string outputPath = (testDir_ / "code128.png").string();
    
    EXPECT_TRUE(generator_.generate("TEST123", config, outputPath));
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateCode39CreatesFile) {
    BarcodeConfig config;
    config.type = BarcodeType::CODE_39;
    config.width = 200;
    config.height = 80;
    std::string outputPath = (testDir_ / "code39.png").string();
    
    EXPECT_TRUE(generator_.generate("TEST123", config, outputPath));
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateQRCodeCreatesFile) {
    BarcodeConfig config;
    config.type = BarcodeType::QR_CODE;
    config.width = 200;
    config.height = 200;
    std::string outputPath = (testDir_ / "qrcode.png").string();
    
    EXPECT_TRUE(generator_.generate("TEST123", config, outputPath));
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateDataMatrixCreatesFile) {
    BarcodeConfig config;
    config.type = BarcodeType::DATA_MATRIX;
    config.width = 200;
    config.height = 200;
    std::string outputPath = (testDir_ / "datamatrix.png").string();
    
    EXPECT_TRUE(generator_.generate("TEST123", config, outputPath));
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

TEST_F(BarcodeGeneratorTest, GenerateEAN13CreatesFile) {
    BarcodeConfig config;
    config.type = BarcodeType::EAN_13;
    config.width = 200;
    config.height = 80;
    std::string outputPath = (testDir_ / "ean13.png").string();
    
    // EAN-13 requires 12 digits (checksum calculated) or 13 digits
    EXPECT_TRUE(generator_.generate("123456789012", config, outputPath));
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

// ============================================================================
// Barcode Decode Verification Tests (需求 6.1 - 标准符合性)
// ============================================================================

TEST_F(BarcodeGeneratorTest, GeneratedCode128IsDecodable) {
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = 300;
    config.height = 100;
    std::string outputPath = (testDir_ / "code128_decode.png").string();
    std::string testData = "PART12345";
    
    ASSERT_TRUE(generator_.generate(testData, config, outputPath));
    
    auto decoded = generator_.decode(outputPath);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), testData);
}

TEST_F(BarcodeGeneratorTest, GeneratedQRCodeIsDecodable) {
    BarcodeConfig config;
    config.type = BarcodeType::QR_CODE;
    config.width = 300;
    config.height = 300;
    std::string outputPath = (testDir_ / "qrcode_decode.png").string();
    std::string testData = "PART-ABC-123";
    
    ASSERT_TRUE(generator_.generate(testData, config, outputPath));
    
    auto decoded = generator_.decode(outputPath);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), testData);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(BarcodeGeneratorTest, BarcodeTypeToStringConvertsCorrectly) {
    EXPECT_EQ(barcodeTypeToString(BarcodeType::CODE_128), "CODE_128");
    EXPECT_EQ(barcodeTypeToString(BarcodeType::CODE_39), "CODE_39");
    EXPECT_EQ(barcodeTypeToString(BarcodeType::QR_CODE), "QR_CODE");
    EXPECT_EQ(barcodeTypeToString(BarcodeType::DATA_MATRIX), "DATA_MATRIX");
    EXPECT_EQ(barcodeTypeToString(BarcodeType::EAN_13), "EAN_13");
}

TEST_F(BarcodeGeneratorTest, StringToBarcodeTypeConvertsCorrectly) {
    EXPECT_EQ(stringToBarcodeType("CODE_128"), BarcodeType::CODE_128);
    EXPECT_EQ(stringToBarcodeType("CODE_39"), BarcodeType::CODE_39);
    EXPECT_EQ(stringToBarcodeType("QR_CODE"), BarcodeType::QR_CODE);
    EXPECT_EQ(stringToBarcodeType("DATA_MATRIX"), BarcodeType::DATA_MATRIX);
    EXPECT_EQ(stringToBarcodeType("EAN_13"), BarcodeType::EAN_13);
}

TEST_F(BarcodeGeneratorTest, StringToBarcodeTypeReturnsNulloptForInvalid) {
    EXPECT_FALSE(stringToBarcodeType("INVALID").has_value());
    EXPECT_FALSE(stringToBarcodeType("").has_value());
}

TEST_F(BarcodeGeneratorTest, GetImageSizeReturnsCorrectDimensions) {
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = 250;
    config.height = 100;
    std::string outputPath = (testDir_ / "size_test.png").string();
    
    ASSERT_TRUE(generator_.generate("TEST", config, outputPath));
    
    int width = 0, height = 0;
    EXPECT_TRUE(generator_.getImageSize(outputPath, width, height));
    EXPECT_EQ(width, 250);
    EXPECT_EQ(height, 100);
}

} // namespace testing
} // namespace creo_barcode
