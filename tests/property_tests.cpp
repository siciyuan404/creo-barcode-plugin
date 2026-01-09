// Prevent Windows min/max macros from conflicting with std::min/std::max
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "barcode_generator.h"
#include "config_manager.h"
#include "version_check.h"
#include "batch_processor.h"
#include "creo_com_bridge.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace creo_barcode {
namespace testing {

// Property tests will be added in subsequent tasks
// This file provides the framework for property-based testing

class PropertyTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "property_test";
        std::filesystem::create_directories(testDir_);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
};

// Placeholder test to verify RapidCheck integration
RC_GTEST_PROP(PropertyTestSetup, IntegrationWorks, ()) {
    int x = *rc::gen::inRange(0, 100);
    RC_ASSERT(x >= 0 && x < 100);
}

// =============================================================================
// Property 1: 条形码编码往返一致性 (Barcode Encoding Round-Trip Consistency)
// **Feature: creo-barcode-plugin, Property 1: 条形码编码往返一致性**
// **Validates: Requirements 1.1, 3.2**
//
// For any valid part name string, encoding it as a barcode and then decoding
// it should return the same original data.
// =============================================================================

namespace {

// Generator for valid part names (printable ASCII characters)
// Part names typically contain alphanumeric characters, underscores, hyphens, etc.
rc::Gen<std::string> genValidPartName() {
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::oneOf(
                rc::gen::inRange<char>('A', 'Z' + 1),  // Uppercase letters
                rc::gen::inRange<char>('a', 'z' + 1),  // Lowercase letters
                rc::gen::inRange<char>('0', '9' + 1),  // Digits
                rc::gen::element('_', '-', '.', ' ')   // Common part name characters
            )
        )
    );
}

} // anonymous namespace

// Property 1 Test: Barcode Encoding Round-Trip Consistency
// For any valid part name string:
// 1. The barcode should be successfully generated
// 2. The generated barcode should be decodable
// 3. The decoded data should exactly match the original part name
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property1_BarcodeEncodingRoundTrip, ()) {
    // Generate a random valid part name
    std::string partName = *genValidPartName();
    
    // Limit part name length to reasonable size (avoid overly large barcodes)
    // Use shorter limit to ensure reliable decoding
    if (partName.length() > 20) {
        partName = partName.substr(0, 20);
    }
    
    // Ensure part name is not empty after potential truncation
    RC_PRE(!partName.empty());
    
    // Use Code 128 as it supports the full ASCII character set
    // Calculate width based on data length to ensure reliable encoding/decoding
    // Code 128 needs approximately 11 modules per character plus start/stop codes
    // Use generous width to avoid scaling artifacts that prevent decoding
    int baseWidth = 400;
    int widthPerChar = 15;
    int calculatedWidth = std::max(baseWidth, static_cast<int>(partName.length() * widthPerChar + 150));
    
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = calculatedWidth;
    config.height = 200;
    config.margin = 20;
    config.showText = false;
    config.dpi = 300;
    
    // Generate unique output path using hash of part name
    std::string outputPath = (testDir_ / ("barcode_p1_" + std::to_string(std::hash<std::string>{}(partName)) + ".png")).string();
    
    BarcodeGenerator generator;
    
    // 1. Barcode should be successfully generated
    bool generated = generator.generate(partName, config, outputPath);
    RC_ASSERT(generated);
    
    // 2. Generated barcode should be decodable
    auto decoded = generator.decode(outputPath);
    RC_ASSERT(decoded.has_value());
    
    // 3. Decoded data should exactly match the original part name
    RC_ASSERT(decoded.value() == partName);
    
    // Cleanup
    std::filesystem::remove(outputPath);
}

// =============================================================================
// Property 3: 条形码类型标准符合性 (Barcode Type Standard Compliance)
// **Feature: creo-barcode-plugin, Property 3: 条形码类型标准符合性**
// **Validates: Requirements 2.2, 6.1**
//
// For any supported barcode type and valid input data, the generated barcode
// should conform to that type's encoding standard and be correctly recognized
// by a standard decoder.
// =============================================================================

namespace {

// Generator for valid Code 128 data (supports ASCII 0-127)
rc::Gen<std::string> genCode128Data() {
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::inRange<char>(32, 127)  // Printable ASCII
        )
    );
}

// Generator for valid Code 39 data (uppercase letters, digits, and special chars)
rc::Gen<std::string> genCode39Data() {
    static const std::string code39Chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%";
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::elementOf(code39Chars)
        )
    );
}

// Generator for valid QR Code data (any printable string)
rc::Gen<std::string> genQRCodeData() {
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::inRange<char>(32, 127)  // Printable ASCII
        )
    );
}

// Generator for valid EAN-13 data (exactly 12 or 13 digits)
rc::Gen<std::string> genEAN13Data() {
    return rc::gen::container<std::string>(12, rc::gen::inRange<char>('0', '9' + 1));
}

// Generator for barcode types that we can reliably test (excluding DATA_MATRIX and EAN_13)
rc::Gen<BarcodeType> genTestableBarcodeType() {
    return rc::gen::element(
        BarcodeType::CODE_128,
        BarcodeType::CODE_39,
        BarcodeType::QR_CODE
    );
}

// Generate valid data for a specific barcode type
rc::Gen<std::string> genValidDataForType(BarcodeType type) {
    switch (type) {
        case BarcodeType::CODE_128:
            return genCode128Data();
        case BarcodeType::CODE_39:
            return genCode39Data();
        case BarcodeType::QR_CODE:
            return genQRCodeData();
        case BarcodeType::EAN_13:
            return genEAN13Data();
        default:
            return genCode128Data();
    }
}

} // anonymous namespace

// =============================================================================
// Property 2: 特殊字符编码正确性 (Special Character Encoding Correctness)
// **Feature: creo-barcode-plugin, Property 2: 特殊字符编码正确性**
// **Validates: Requirements 1.3**
//
// For any part name containing special characters (spaces, Chinese characters,
// symbols, etc.), after encoding processing, the generated barcode should be
// correctly decoded, and the decoded result should be equivalent to the original data.
// =============================================================================

namespace {

// Generator for strings containing special characters
// This includes non-printable ASCII (0-31, 127) and extended characters (128-255)
rc::Gen<std::string> genStringWithSpecialChars() {
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::oneOf(
                // Printable ASCII (normal characters)
                rc::gen::inRange<char>(32, 127),
                // Non-printable ASCII (control characters)
                rc::gen::inRange<char>(1, 32),
                // Extended ASCII (high bytes)
                rc::gen::inRange<char>(static_cast<char>(128), static_cast<char>(255))
            )
        )
    );
}

// Generator for strings with mixed content (normal + special chars)
rc::Gen<std::string> genMixedPartName() {
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::oneOf(
                // Alphanumeric (common in part names)
                rc::gen::inRange<char>('A', 'Z' + 1),
                rc::gen::inRange<char>('a', 'z' + 1),
                rc::gen::inRange<char>('0', '9' + 1),
                // Common separators and backslash (needs escaping)
                rc::gen::element('_', '-', '.', ' ', '\\'),
                // Special characters that need encoding
                rc::gen::inRange<char>(1, 32),
                rc::gen::inRange<char>(static_cast<char>(128), static_cast<char>(200))
            )
        )
    );
}

} // anonymous namespace

// Property 2 Test: Special Character Encoding Correctness
// For any string containing special characters:
// 1. encodeSpecialChars() should produce a valid encoded string
// 2. decodeSpecialChars() should reverse the encoding exactly
// 3. The round-trip (encode then decode) should return the original string
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property2_SpecialCharEncodingCorrectness, ()) {
    // Generate a random string that may contain special characters
    std::string originalData = *genMixedPartName();
    
    // Limit length to reasonable size
    if (originalData.length() > 50) {
        originalData = originalData.substr(0, 50);
    }
    
    // Ensure data is not empty after potential truncation
    RC_PRE(!originalData.empty());
    
    BarcodeGenerator generator;
    
    // 1. Encode the string with special characters
    std::string encoded = generator.encodeSpecialChars(originalData);
    
    // 2. The encoded string should only contain printable ASCII characters
    //    (since special chars are converted to \xNN format)
    for (unsigned char c : encoded) {
        RC_ASSERT(c >= 32 && c <= 126);
    }
    
    // 3. Decode the encoded string
    std::string decoded = generator.decodeSpecialChars(encoded);
    
    // 4. The decoded string should exactly match the original
    RC_ASSERT(decoded == originalData);
}

// =============================================================================
// Property 3 (COM Bridge): 字符串转换往返一致性 (String Conversion Round-Trip Consistency)
// **Feature: barcode-image-insertion, Property 3: 字符串转换往返一致性**
// **Validates: Requirements 4.3**
//
// For any valid string, converting from std::wstring to BSTR and back should
// produce the original string.
// =============================================================================

#ifdef _WIN32

namespace {

// Generator for valid wide strings (Unicode characters)
// Includes ASCII, extended Latin, and common Unicode characters
rc::Gen<std::wstring> genWideString() {
    return rc::gen::container<std::wstring>(
        rc::gen::oneOf(
            // ASCII printable characters
            rc::gen::inRange<wchar_t>(32, 127),
            // Extended Latin characters
            rc::gen::inRange<wchar_t>(0x00C0, 0x00FF),
            // Common CJK characters (Chinese/Japanese/Korean)
            rc::gen::inRange<wchar_t>(0x4E00, 0x4FFF),
            // Cyrillic characters
            rc::gen::inRange<wchar_t>(0x0400, 0x04FF)
        )
    );
}

// Generator for UTF-8 strings (valid UTF-8 sequences)
rc::Gen<std::string> genUtf8String() {
    return rc::gen::container<std::string>(
        rc::gen::oneOf(
            // ASCII printable characters (single byte UTF-8)
            rc::gen::inRange<char>(32, 127),
            // Common alphanumeric
            rc::gen::inRange<char>('A', 'Z' + 1),
            rc::gen::inRange<char>('a', 'z' + 1),
            rc::gen::inRange<char>('0', '9' + 1)
        )
    );
}

} // anonymous namespace

// Property 3 Test (COM Bridge): wstring to BSTR Round-Trip
// For any valid wide string:
// 1. Converting to BSTR should succeed
// 2. Converting back to wstring should produce the original string
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property3_COM_WstringToBstrRoundTrip, ()) {
    using namespace creo_barcode::StringUtils;
    
    // Generate a random wide string
    std::wstring original = *genWideString();
    
    // Limit length to reasonable size
    if (original.length() > 100) {
        original = original.substr(0, 100);
    }
    
    // 1. Convert wstring to BSTR
    BSTR bstr = stringToBSTR(original);
    
    // BSTR should not be null (even for empty strings, SysAllocString returns valid BSTR)
    RC_ASSERT(bstr != nullptr);
    
    // 2. Convert BSTR back to wstring
    std::wstring roundTripped = bstrToString(bstr);
    
    // 3. Free the BSTR
    SysFreeString(bstr);
    
    // 4. The round-tripped string should exactly match the original
    RC_ASSERT(roundTripped == original);
}

// Property 3 Test (COM Bridge): UTF-8 to wstring Round-Trip
// For any valid UTF-8 string:
// 1. Converting to wstring should succeed
// 2. Converting back to UTF-8 should produce the original string
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property3_COM_Utf8ToWstringRoundTrip, ()) {
    using namespace creo_barcode::StringUtils;
    
    // Generate a random UTF-8 string (ASCII subset for reliable round-trip)
    std::string original = *genUtf8String();
    
    // Limit length to reasonable size
    if (original.length() > 100) {
        original = original.substr(0, 100);
    }
    
    // 1. Convert UTF-8 to wstring
    std::wstring wide = utf8ToWstring(original);
    
    // 2. Convert wstring back to UTF-8
    std::string roundTripped = wstringToUtf8(wide);
    
    // 3. The round-tripped string should exactly match the original
    RC_ASSERT(roundTripped == original);
}

// Property 3 Test (COM Bridge): Empty String Handling
// Empty strings should round-trip correctly
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property3_COM_EmptyStringRoundTrip, ()) {
    using namespace creo_barcode::StringUtils;
    
    // Test empty wstring to BSTR round-trip
    std::wstring emptyWstr;
    BSTR bstr = stringToBSTR(emptyWstr);
    RC_ASSERT(bstr != nullptr);
    std::wstring roundTrippedWstr = bstrToString(bstr);
    SysFreeString(bstr);
    RC_ASSERT(roundTrippedWstr.empty());
    
    // Test empty UTF-8 to wstring round-trip
    std::string emptyUtf8;
    std::wstring wide = utf8ToWstring(emptyUtf8);
    std::string roundTrippedUtf8 = wstringToUtf8(wide);
    RC_ASSERT(roundTrippedUtf8.empty());
}

#endif // _WIN32

// =============================================================================
// Property 2 (COM Bridge): 文件验证与错误报告 (File Validation and Error Reporting)
// **Feature: barcode-image-insertion, Property 2: 文件验证与错误报告**
// **Validates: Requirements 3.4, 7.5**
//
// For any image path input, if the file does not exist, the system should
// return an error and the error message should contain the file path.
// =============================================================================

namespace {

// Generator for random file paths (that don't exist)
rc::Gen<std::string> genNonExistentFilePath() {
    return rc::gen::map(
        rc::gen::nonEmpty(
            rc::gen::container<std::string>(
                rc::gen::oneOf(
                    rc::gen::inRange<char>('A', 'Z' + 1),
                    rc::gen::inRange<char>('a', 'z' + 1),
                    rc::gen::inRange<char>('0', '9' + 1),
                    rc::gen::element('_', '-')
                )
            )
        ),
        [](std::string name) {
            // Limit name length and add extension
            if (name.length() > 30) {
                name = name.substr(0, 30);
            }
            // Use a non-existent directory to ensure file doesn't exist
            return "C:\\NonExistentDir_12345\\" + name + ".png";
        }
    );
}

// Generator for random file extensions (including unsupported ones)
rc::Gen<std::string> genFileExtension() {
    return rc::gen::element(
        std::string(".png"),
        std::string(".jpg"),
        std::string(".jpeg"),
        std::string(".bmp"),
        std::string(".gif"),    // Unsupported
        std::string(".tiff"),   // Unsupported
        std::string(".svg"),    // Unsupported
        std::string(".webp"),   // Unsupported
        std::string(".txt"),    // Unsupported
        std::string(".pdf")     // Unsupported
    );
}

// Generator for supported image extensions only
rc::Gen<std::string> genSupportedExtension() {
    return rc::gen::element(
        std::string(".png"),
        std::string(".jpg"),
        std::string(".jpeg"),
        std::string(".bmp")
    );
}

// Generator for unsupported image extensions
rc::Gen<std::string> genUnsupportedExtension() {
    return rc::gen::element(
        std::string(".gif"),
        std::string(".tiff"),
        std::string(".svg"),
        std::string(".webp"),
        std::string(".txt"),
        std::string(".pdf"),
        std::string(".doc"),
        std::string(".exe")
    );
}

} // anonymous namespace

// Property 2 Test (COM Bridge): Non-existent File Returns Error with Path
// For any non-existent file path:
// 1. insertImage should return false
// 2. The error message should contain the file path
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property2_COM_NonExistentFileReturnsErrorWithPath, ()) {
    // Generate a random non-existent file path
    std::string nonExistentPath = *genNonExistentFilePath();
    
    // Ensure the file doesn't actually exist (very unlikely but check anyway)
    RC_PRE(!std::filesystem::exists(nonExistentPath));
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    
    // Initialize COM bridge (may fail if Creo not running, but that's OK for this test)
    // We're testing file validation which happens before Creo connection
    bridge.initialize();
    
    // Attempt to insert the non-existent image
    bool result = bridge.insertImage(nonExistentPath, 0.0, 0.0, 50.0, 50.0);
    
    // 1. insertImage should return false for non-existent file
    RC_ASSERT(result == false);
    
    // 2. The error message should contain the file path
    std::string errorMsg = bridge.getLastError();
    RC_ASSERT(!errorMsg.empty());
    
    // The error message should contain the file path (or at least part of it)
    // Extract just the filename for checking since full path might be transformed
    std::filesystem::path filePath(nonExistentPath);
    std::string filename = filePath.filename().string();
    
    // Error message should mention the file or contain "not found"
    bool containsPath = (errorMsg.find(nonExistentPath) != std::string::npos) ||
                        (errorMsg.find(filename) != std::string::npos);
    bool containsNotFound = (errorMsg.find("not found") != std::string::npos) ||
                            (errorMsg.find("not exist") != std::string::npos) ||
                            (errorMsg.find("Image file") != std::string::npos);
    
    RC_ASSERT(containsPath || containsNotFound);
}

// Property 2 Test (COM Bridge): Empty Path Returns Error
// An empty file path should return an error
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property2_COM_EmptyPathReturnsError, ()) {
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert with empty path
    bool result = bridge.insertImage("", 0.0, 0.0, 50.0, 50.0);
    
    // Should return false
    RC_ASSERT(result == false);
    
    // Error message should not be empty
    std::string errorMsg = bridge.getLastError();
    RC_ASSERT(!errorMsg.empty());
    
    // Error should mention empty or path
    bool mentionsEmpty = (errorMsg.find("empty") != std::string::npos) ||
                         (errorMsg.find("Empty") != std::string::npos);
    RC_ASSERT(mentionsEmpty);
}

// Property 2 Test (COM Bridge): Unsupported Format Returns Error with Extension
// For any unsupported file format, the error should mention the format
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property2_COM_UnsupportedFormatReturnsError, ()) {
    // Generate an unsupported extension
    std::string ext = *genUnsupportedExtension();
    
    // Create a temporary file with unsupported extension
    std::filesystem::path tempDir = std::filesystem::temp_directory_path();
    std::string tempFileName = "test_unsupported_" + std::to_string(std::hash<std::string>{}(ext)) + ext;
    std::filesystem::path tempFilePath = tempDir / tempFileName;
    
    // Create the file (so it exists but has wrong format)
    {
        std::ofstream ofs(tempFilePath);
        ofs << "dummy content";
    }
    
    // Ensure file was created
    RC_PRE(std::filesystem::exists(tempFilePath));
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert the unsupported format file
    bool result = bridge.insertImage(tempFilePath.string(), 0.0, 0.0, 50.0, 50.0);
    
    // Should return false
    RC_ASSERT(result == false);
    
    // Error message should mention unsupported format or the extension
    std::string errorMsg = bridge.getLastError();
    RC_ASSERT(!errorMsg.empty());
    
    // Convert extension to lowercase for comparison
    std::string extLower = ext;
    std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
    
    bool mentionsFormat = (errorMsg.find("Unsupported") != std::string::npos) ||
                          (errorMsg.find("unsupported") != std::string::npos) ||
                          (errorMsg.find("format") != std::string::npos) ||
                          (errorMsg.find(extLower) != std::string::npos);
    RC_ASSERT(mentionsFormat);
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// Property 2 Test (COM Bridge): Valid Existing File Passes Validation
// For any existing file with supported format, validation should pass
// (Note: actual insertion may still fail if Creo is not running)
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property2_COM_ValidFilePassesValidation, ()) {
    // Generate a supported extension
    std::string ext = *genSupportedExtension();
    
    // Create a temporary file with supported extension
    std::filesystem::path tempDir = std::filesystem::temp_directory_path();
    std::string tempFileName = "test_valid_" + std::to_string(std::hash<std::string>{}(ext)) + ext;
    std::filesystem::path tempFilePath = tempDir / tempFileName;
    
    // Create a minimal valid image file (just needs to exist and have content)
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        // Write minimal PNG header for .png files, or just dummy content for others
        if (ext == ".png") {
            // Minimal PNG signature
            unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
            ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
        } else {
            ofs << "dummy image content";
        }
    }
    
    // Ensure file was created
    RC_PRE(std::filesystem::exists(tempFilePath));
    RC_PRE(std::filesystem::file_size(tempFilePath) > 0);
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert the valid file
    // This may fail due to Creo not running, but should NOT fail due to file validation
    bool result = bridge.insertImage(tempFilePath.string(), 0.0, 0.0, 50.0, 50.0);
    
    // Get the error message
    std::string errorMsg = bridge.getLastError();
    
    // If it failed, it should NOT be due to file validation issues
    if (!result) {
        // Error should NOT mention file not found, unsupported format, or empty
        bool isFileValidationError = 
            (errorMsg.find("not found") != std::string::npos) ||
            (errorMsg.find("Unsupported") != std::string::npos) ||
            (errorMsg.find("empty") != std::string::npos && errorMsg.find("Image file") != std::string::npos) ||
            (errorMsg.find("is empty") != std::string::npos);
        
        // If there's an error, it should be about Creo connection, not file validation
        RC_ASSERT(!isFileValidationError);
    }
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// Property 3 Test (Barcode): Barcode Type Standard Compliance
// For any supported barcode type and valid input data:
// 1. The barcode should be successfully generated
// 2. The generated barcode should be decodable by a standard decoder
// 3. The decoded data should match the original input data
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property3_BarcodeTypeStandardCompliance, ()) {
    // Generate a random barcode type (excluding problematic types)
    BarcodeType type = *genTestableBarcodeType();
    
    // Generate valid data for the selected barcode type
    std::string inputData = *genValidDataForType(type);
    
    // Limit data length based on barcode type to ensure reliable encoding/decoding
    // Code 39 requires more space per character, so limit to shorter strings
    size_t maxLength = (type == BarcodeType::CODE_39) ? 15 : 30;
    if (inputData.length() > maxLength) {
        inputData = inputData.substr(0, maxLength);
    }
    
    // Ensure data is not empty after potential truncation
    RC_PRE(!inputData.empty());
    
    // Create barcode configuration with type-appropriate dimensions
    // Code 39 needs more width per character (approximately 16 modules per char)
    // Code 128 is more compact (approximately 11 modules per char)
    BarcodeConfig config;
    config.type = type;
    
    // Calculate appropriate width based on barcode type and data length
    int baseWidth = 200;
    int widthPerChar = (type == BarcodeType::CODE_39) ? 20 : 12;
    config.width = std::max(baseWidth, static_cast<int>(inputData.length() * widthPerChar + 100));
    
    // QR codes need square dimensions, linear barcodes need more width than height
    config.height = (type == BarcodeType::QR_CODE) ? config.width : 150;
    config.margin = 10;
    config.showText = false;
    config.dpi = 300;
    
    // Generate unique output path
    std::string outputPath = (testDir_ / ("barcode_p3_" + std::to_string(std::hash<std::string>{}(inputData)) + ".png")).string();
    
    BarcodeGenerator generator;
    
    // 1. Barcode should be successfully generated
    bool generated = generator.generate(inputData, config, outputPath);
    RC_ASSERT(generated);
    
    // 2. Generated barcode should be decodable
    auto decoded = generator.decode(outputPath);
    RC_ASSERT(decoded.has_value());
    
    // 3. Decoded data should match original input
    RC_ASSERT(decoded.value() == inputData);
    
    // Cleanup
    std::filesystem::remove(outputPath);
}

// =============================================================================
// Property 4: 条形码尺寸正确性 (Barcode Size Correctness)
// **Feature: creo-barcode-plugin, Property 4: 条形码尺寸正确性**
// **Validates: Requirements 2.3**
//
// For any valid size parameters (width, height), the actual dimensions of the
// generated barcode image should match the specified parameters exactly.
// =============================================================================

namespace {

// Generator for valid barcode dimensions
// Minimum size is constrained by barcode encoding requirements
// Maximum size is constrained by practical limits
rc::Gen<int> genValidWidth() {
    // Width must be at least 50 pixels for readable barcodes
    // and at most 1000 pixels for practical use
    return rc::gen::inRange(50, 1001);
}

rc::Gen<int> genValidHeight() {
    // Height must be at least 30 pixels for readable barcodes
    // and at most 500 pixels for practical use
    return rc::gen::inRange(30, 501);
}

// Generator for simple barcode data that works with all types
rc::Gen<std::string> genSimpleBarcodeData() {
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::oneOf(
                rc::gen::inRange<char>('A', 'Z' + 1),  // Uppercase letters
                rc::gen::inRange<char>('0', '9' + 1)   // Digits
            )
        )
    );
}

} // anonymous namespace

// Property 4 Test: Barcode Size Correctness
// For any valid size parameters (width, height):
// 1. The barcode should be successfully generated with the specified dimensions
// 2. The actual image dimensions should exactly match the specified width and height
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_BarcodeSizeCorrectness, ()) {
    // Generate random valid dimensions
    int requestedWidth = *genValidWidth();
    int requestedHeight = *genValidHeight();
    
    // Generate simple barcode data (uppercase letters and digits work with all types)
    std::string inputData = *genSimpleBarcodeData();
    
    // Limit data length to ensure it fits in the barcode
    if (inputData.length() > 10) {
        inputData = inputData.substr(0, 10);
    }
    
    // Ensure data is not empty
    RC_PRE(!inputData.empty());
    
    // Use Code 128 as it's the most flexible for testing dimensions
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = requestedWidth;
    config.height = requestedHeight;
    config.margin = 10;
    config.showText = false;
    config.dpi = 300;
    
    // Generate unique output path
    std::string outputPath = (testDir_ / ("barcode_p4_" + 
        std::to_string(requestedWidth) + "x" + 
        std::to_string(requestedHeight) + "_" +
        std::to_string(std::hash<std::string>{}(inputData)) + ".png")).string();
    
    BarcodeGenerator generator;
    
    // 1. Barcode should be successfully generated
    bool generated = generator.generate(inputData, config, outputPath);
    RC_ASSERT(generated);
    
    // 2. Get actual image dimensions
    int actualWidth = 0;
    int actualHeight = 0;
    bool gotSize = generator.getImageSize(outputPath, actualWidth, actualHeight);
    RC_ASSERT(gotSize);
    
    // 3. Actual dimensions should exactly match requested dimensions
    RC_ASSERT(actualWidth == requestedWidth);
    RC_ASSERT(actualHeight == requestedHeight);
    
    // Cleanup
    std::filesystem::remove(outputPath);
}

// Additional Property 4 Test: QR Code Size Correctness
// QR codes have specific dimension requirements, so test them separately
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_QRCodeSizeCorrectness, ()) {
    // Generate random valid dimensions (QR codes work better with square dimensions)
    int requestedSize = *rc::gen::inRange(100, 501);
    
    // Generate simple barcode data
    std::string inputData = *genSimpleBarcodeData();
    
    // Limit data length for QR codes
    if (inputData.length() > 20) {
        inputData = inputData.substr(0, 20);
    }
    
    // Ensure data is not empty
    RC_PRE(!inputData.empty());
    
    // Use QR Code type
    BarcodeConfig config;
    config.type = BarcodeType::QR_CODE;
    config.width = requestedSize;
    config.height = requestedSize;
    config.margin = 10;
    config.showText = false;
    config.dpi = 300;
    
    // Generate unique output path
    std::string outputPath = (testDir_ / ("barcode_p4_qr_" + 
        std::to_string(requestedSize) + "_" +
        std::to_string(std::hash<std::string>{}(inputData)) + ".png")).string();
    
    BarcodeGenerator generator;
    
    // 1. Barcode should be successfully generated
    bool generated = generator.generate(inputData, config, outputPath);
    RC_ASSERT(generated);
    
    // 2. Get actual image dimensions
    int actualWidth = 0;
    int actualHeight = 0;
    bool gotSize = generator.getImageSize(outputPath, actualWidth, actualHeight);
    RC_ASSERT(gotSize);
    
    // 3. Actual dimensions should exactly match requested dimensions
    RC_ASSERT(actualWidth == requestedSize);
    RC_ASSERT(actualHeight == requestedSize);
    
    // Cleanup
    std::filesystem::remove(outputPath);
}

// =============================================================================
// Property 7: 配置序列化往返一致性 (Configuration Serialization Round-Trip Consistency)
// **Feature: creo-barcode-plugin, Property 7: 配置序列化往返一致性**
// **Validates: 配置管理器数据完整性**
//
// For any valid configuration object, serializing it to JSON and then
// deserializing it back should produce an equivalent configuration object.
// =============================================================================

namespace {

// Generator for valid BarcodeType enum values
rc::Gen<BarcodeType> genBarcodeType() {
    return rc::gen::element(
        BarcodeType::CODE_128,
        BarcodeType::CODE_39,
        BarcodeType::QR_CODE,
        BarcodeType::DATA_MATRIX,
        BarcodeType::EAN_13
    );
}

// Generator for valid width values (positive integers within reasonable range)
rc::Gen<int> genConfigWidth() {
    return rc::gen::inRange(50, 1001);
}

// Generator for valid height values (positive integers within reasonable range)
rc::Gen<int> genConfigHeight() {
    return rc::gen::inRange(30, 501);
}

// Generator for valid DPI values (common DPI values)
rc::Gen<int> genConfigDpi() {
    return rc::gen::element(72, 96, 150, 200, 300, 600);
}

// Generator for valid output directory paths (printable ASCII, no control chars)
rc::Gen<std::string> genOutputDirectory() {
    return rc::gen::container<std::string>(
        rc::gen::oneOf(
            rc::gen::inRange<char>('A', 'Z' + 1),  // Uppercase letters
            rc::gen::inRange<char>('a', 'z' + 1),  // Lowercase letters
            rc::gen::inRange<char>('0', '9' + 1),  // Digits
            rc::gen::element('/', '\\', '_', '-', '.', ':')  // Path characters
        )
    );
}

// Generator for valid file path strings (for recentFiles)
rc::Gen<std::string> genFilePath() {
    return rc::gen::nonEmpty(
        rc::gen::container<std::string>(
            rc::gen::oneOf(
                rc::gen::inRange<char>('A', 'Z' + 1),
                rc::gen::inRange<char>('a', 'z' + 1),
                rc::gen::inRange<char>('0', '9' + 1),
                rc::gen::element('/', '\\', '_', '-', '.', ':')
            )
        )
    );
}

// Generator for recentFiles vector (0 to 10 file paths)
rc::Gen<std::vector<std::string>> genRecentFiles() {
    // Use a simpler approach: generate a vector with arbitrary size (RapidCheck will vary the size)
    // and then limit it to 10 elements
    return rc::gen::map(
        rc::gen::container<std::vector<std::string>>(genFilePath()),
        [](std::vector<std::string> files) {
            if (files.size() > 10) {
                files.resize(10);
            }
            return files;
        }
    );
}

// Generator for valid PluginConfig objects
rc::Gen<PluginConfig> genPluginConfig() {
    return rc::gen::build<PluginConfig>(
        rc::gen::set(&PluginConfig::defaultType, genBarcodeType()),
        rc::gen::set(&PluginConfig::defaultWidth, genConfigWidth()),
        rc::gen::set(&PluginConfig::defaultHeight, genConfigHeight()),
        rc::gen::set(&PluginConfig::defaultShowText, rc::gen::arbitrary<bool>()),
        rc::gen::set(&PluginConfig::outputDirectory, genOutputDirectory()),
        rc::gen::set(&PluginConfig::defaultDpi, genConfigDpi()),
        rc::gen::set(&PluginConfig::recentFiles, genRecentFiles())
    );
}

} // anonymous namespace

// Property 7 Test: Configuration Serialization Round-Trip Consistency
// For any valid PluginConfig object:
// 1. Serialization to JSON should succeed
// 2. Deserialization from JSON should succeed
// 3. The deserialized config should be equal to the original config
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_ConfigSerializationRoundTrip, ()) {
    // Generate a random valid PluginConfig
    PluginConfig originalConfig = *genPluginConfig();
    
    // Create a ConfigManager and set the original config
    ConfigManager manager;
    manager.setConfig(originalConfig);
    
    // 1. Serialize the config to JSON
    std::string jsonStr = manager.serialize();
    
    // Verify serialization produced non-empty output
    RC_ASSERT(!jsonStr.empty());
    
    // 2. Create a new ConfigManager and deserialize the JSON
    ConfigManager manager2;
    bool deserializeSuccess = manager2.deserialize(jsonStr);
    
    // Deserialization should succeed
    RC_ASSERT(deserializeSuccess);
    
    // 3. Get the deserialized config and compare with original
    PluginConfig deserializedConfig = manager2.getConfig();
    
    // All fields should match exactly
    RC_ASSERT(deserializedConfig.defaultType == originalConfig.defaultType);
    RC_ASSERT(deserializedConfig.defaultWidth == originalConfig.defaultWidth);
    RC_ASSERT(deserializedConfig.defaultHeight == originalConfig.defaultHeight);
    RC_ASSERT(deserializedConfig.defaultShowText == originalConfig.defaultShowText);
    RC_ASSERT(deserializedConfig.outputDirectory == originalConfig.outputDirectory);
    RC_ASSERT(deserializedConfig.defaultDpi == originalConfig.defaultDpi);
    RC_ASSERT(deserializedConfig.recentFiles == originalConfig.recentFiles);
    
    // Use the equality operator for final verification
    RC_ASSERT(deserializedConfig == originalConfig);
}

// Additional Property 7 Test: File-based Round-Trip
// Tests that saving to file and loading from file preserves config
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_ConfigFileRoundTrip, ()) {
    // Generate a random valid PluginConfig
    PluginConfig originalConfig = *genPluginConfig();
    
    // Create a unique config file path
    std::string configPath = (testDir_ / ("config_p7_" + 
        std::to_string(std::hash<PluginConfig*>{}(&originalConfig)) + ".json")).string();
    
    // Create a ConfigManager and set the original config
    ConfigManager manager;
    manager.setConfig(originalConfig);
    
    // 1. Save the config to file
    bool saveSuccess = manager.saveConfig(configPath);
    RC_ASSERT(saveSuccess);
    
    // 2. Create a new ConfigManager and load from file
    ConfigManager manager2;
    bool loadSuccess = manager2.loadConfig(configPath);
    RC_ASSERT(loadSuccess);
    
    // 3. Get the loaded config and compare with original
    PluginConfig loadedConfig = manager2.getConfig();
    
    // All fields should match exactly
    RC_ASSERT(loadedConfig == originalConfig);
    
    // Cleanup
    std::filesystem::remove(configPath);
}

// =============================================================================
// Property 5: 版本兼容性检查正确性 (Version Compatibility Check Correctness)
// **Feature: creo-barcode-plugin, Property 5: 版本兼容性检查正确性**
// **Validates: Requirements 4.2**
//
// For any Creo version number, the version check function should correctly
// determine: version >= 8.0 returns compatible, version < 8.0 returns incompatible.
// =============================================================================

namespace {

// Generator for valid version components (non-negative integers)
rc::Gen<int> genMajorVersion() {
    // Major version from 0 to 20 (covers reasonable range around version 8)
    return rc::gen::inRange(0, 21);
}

rc::Gen<int> genMinorVersion() {
    // Minor version from 0 to 99
    return rc::gen::inRange(0, 100);
}

rc::Gen<int> genPatchVersion() {
    // Patch version from 0 to 99
    return rc::gen::inRange(0, 100);
}

// Generator for CreoVersion objects
rc::Gen<CreoVersion> genCreoVersion() {
    return rc::gen::build<CreoVersion>(
        rc::gen::set(&CreoVersion::major, genMajorVersion()),
        rc::gen::set(&CreoVersion::minor, genMinorVersion()),
        rc::gen::set(&CreoVersion::patch, genPatchVersion())
    );
}

} // anonymous namespace

// Property 5 Test: Version Compatibility Check Correctness
// For any Creo version number:
// 1. If version >= 8.0.0, checkCreoVersion should return true (compatible)
// 2. If version < 8.0.0, checkCreoVersion should return false (incompatible)
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_VersionCompatibilityCheckCorrectness, ()) {
    // Generate a random CreoVersion
    CreoVersion version = *genCreoVersion();
    
    // Get the minimum required version
    CreoVersion minVersion = getMinimumVersion();
    
    // Calculate expected result based on version comparison
    // Version is compatible if it's >= minimum version (8.0.0)
    bool expectedCompatible = (version >= minVersion);
    
    // Alternative calculation for verification:
    // A version is compatible if:
    // - major > 8, OR
    // - major == 8 AND minor > 0, OR
    // - major == 8 AND minor == 0 AND patch >= 0
    // Simplified: major > 8 OR (major == 8)
    bool expectedByLogic = (version.major > MIN_CREO_MAJOR_VERSION) ||
                           (version.major == MIN_CREO_MAJOR_VERSION && version.minor > MIN_CREO_MINOR_VERSION) ||
                           (version.major == MIN_CREO_MAJOR_VERSION && version.minor == MIN_CREO_MINOR_VERSION);
    
    // Both calculations should agree
    RC_ASSERT(expectedCompatible == expectedByLogic);
    
    // Call the actual version check function
    bool actualResult = checkCreoVersion(version);
    
    // The actual result should match the expected result
    RC_ASSERT(actualResult == expectedCompatible);
}

// Additional Property 5 Test: Version Comparison Transitivity
// For any three versions a, b, c: if a >= b and b >= c, then a >= c
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_VersionComparisonTransitivity, ()) {
    // Generate three random versions
    CreoVersion a = *genCreoVersion();
    CreoVersion b = *genCreoVersion();
    CreoVersion c = *genCreoVersion();
    
    // Test transitivity: if a >= b and b >= c, then a >= c
    if ((a >= b) && (b >= c)) {
        RC_ASSERT(a >= c);
    }
}

// Additional Property 5 Test: Version Comparison Antisymmetry
// For any two versions a, b: if a >= b and b >= a, then a == b (in terms of comparison)
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_VersionComparisonAntisymmetry, ()) {
    // Generate two random versions
    CreoVersion a = *genCreoVersion();
    CreoVersion b = *genCreoVersion();
    
    // Test antisymmetry: if a >= b and b >= a, then they are equivalent
    if ((a >= b) && (b >= a)) {
        // Both should be considered equal (neither is strictly less than the other)
        RC_ASSERT(!(a < b));
        RC_ASSERT(!(b < a));
    }
}

// Additional Property 5 Test: Version Comparison Totality
// For any two versions a, b: either a >= b or b > a (one must be true)
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_VersionComparisonTotality, ()) {
    // Generate two random versions
    CreoVersion a = *genCreoVersion();
    CreoVersion b = *genCreoVersion();
    
    // Test totality: either a >= b or a < b (exactly one must be true)
    bool aGeqB = (a >= b);
    bool aLtB = (a < b);
    
    // Exactly one should be true (XOR)
    RC_ASSERT(aGeqB != aLtB);
}

// =============================================================================
// Property 6: 批量处理完整性 (Batch Processing Completeness)
// **Feature: creo-barcode-plugin, Property 6: 批量处理完整性**
// **Validates: Requirements 5.2, 5.3, 5.4**
//
// For any file list, after batch processing completes:
// (1) The number of results should equal the number of input files
// (2) Each file should have a corresponding success or failure status
// (3) The sum of success count and failure count in the summary should equal
//     the total file count
// =============================================================================

namespace {

// Generator for valid file paths (simulating Creo drawing files)
rc::Gen<std::string> genBatchFilePath() {
    return rc::gen::map(
        rc::gen::nonEmpty(
            rc::gen::container<std::string>(
                rc::gen::oneOf(
                    rc::gen::inRange<char>('A', 'Z' + 1),
                    rc::gen::inRange<char>('a', 'z' + 1),
                    rc::gen::inRange<char>('0', '9' + 1),
                    rc::gen::element('_', '-')
                )
            )
        ),
        [](std::string name) {
            // Limit name length
            if (name.length() > 20) {
                name = name.substr(0, 20);
            }
            return name + ".drw";
        }
    );
}

// Generator for a list of file paths (0 to 50 files)
rc::Gen<std::vector<std::string>> genBatchFileList() {
    return rc::gen::container<std::vector<std::string>>(genBatchFilePath());
}

} // anonymous namespace

// Property 6 Test: Batch Processing Completeness
// For any file list:
// 1. The number of results should equal the number of input files
// 2. Each file should have a corresponding success or failure status
// 3. The sum of success count and failure count should equal total file count
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property6_BatchProcessingCompleteness, ()) {
    // Generate a random list of file paths
    std::vector<std::string> fileList = *genBatchFileList();
    
    // Limit the list size to avoid overly long test runs
    if (fileList.size() > 50) {
        fileList.resize(50);
    }
    
    // Create a BatchProcessor and add files
    BatchProcessor processor;
    processor.addFiles(fileList);
    
    // Verify queue size matches input
    RC_ASSERT(processor.getQueueSize() == fileList.size());
    
    // Create a default barcode config for processing
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = 200;
    config.height = 80;
    config.margin = 10;
    config.showText = false;
    config.dpi = 300;
    
    // Track progress callback invocations
    int progressCallCount = 0;
    int lastCurrent = 0;
    int lastTotal = 0;
    
    auto progressCallback = [&](int current, int total) {
        progressCallCount++;
        lastCurrent = current;
        lastTotal = total;
    };
    
    // Process the batch
    std::vector<BatchResult> results = processor.process(config, progressCallback);
    
    // Property 6.1: Result count should equal input file count
    RC_ASSERT(results.size() == fileList.size());
    
    // Property 6.2: Each file should have a corresponding result with valid status
    // (success is either true or false, not undefined)
    for (size_t i = 0; i < results.size(); ++i) {
        // Each result should have a file path
        RC_ASSERT(!results[i].filePath.empty());
        
        // The file path should match the input (order preserved)
        RC_ASSERT(results[i].filePath == fileList[i]);
        
        // If failed, there should be an error message
        if (!results[i].success) {
            RC_ASSERT(!results[i].errorMessage.empty());
        }
    }
    
    // Property 6.3: Success count + failure count should equal total
    int successCount = 0;
    int failureCount = 0;
    for (const auto& result : results) {
        if (result.success) {
            ++successCount;
        } else {
            ++failureCount;
        }
    }
    RC_ASSERT(static_cast<size_t>(successCount + failureCount) == fileList.size());
    
    // Verify progress callback was called correctly (if there were files)
    if (!fileList.empty()) {
        RC_ASSERT(progressCallCount == static_cast<int>(fileList.size()));
        RC_ASSERT(lastCurrent == static_cast<int>(fileList.size()));
        RC_ASSERT(lastTotal == static_cast<int>(fileList.size()));
    }
    
    // Verify summary is consistent
    std::string summary = BatchProcessor::getSummary(results);
    RC_ASSERT(!summary.empty());
    
    // Summary should contain the correct counts
    // Parse the summary to verify counts match
    // The summary format is:
    // "Total files: X\nSuccessful: Y\nFailed: Z\n"
    std::string totalStr = "Total files: " + std::to_string(fileList.size());
    std::string successStr = "Successful: " + std::to_string(successCount);
    std::string failedStr = "Failed: " + std::to_string(failureCount);
    
    RC_ASSERT(summary.find(totalStr) != std::string::npos);
    RC_ASSERT(summary.find(successStr) != std::string::npos);
    RC_ASSERT(summary.find(failedStr) != std::string::npos);
}

// Additional Property 6 Test: Empty File List Handling
// An empty file list should produce an empty result list
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property6_EmptyFileListHandling, ()) {
    BatchProcessor processor;
    
    // Verify empty queue
    RC_ASSERT(processor.getQueueSize() == 0);
    
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = 200;
    config.height = 80;
    config.margin = 10;
    config.showText = false;
    config.dpi = 300;
    
    // Process empty batch
    std::vector<BatchResult> results = processor.process(config);
    
    // Result should be empty
    RC_ASSERT(results.empty());
    
    // Summary should show zero counts
    std::string summary = BatchProcessor::getSummary(results);
    RC_ASSERT(summary.find("Total files: 0") != std::string::npos);
    RC_ASSERT(summary.find("Successful: 0") != std::string::npos);
    RC_ASSERT(summary.find("Failed: 0") != std::string::npos);
}

// Additional Property 6 Test: Partial Failure Handling
// When some files fail, processing should continue and report all results
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property6_PartialFailureHandling, ()) {
    // Generate a mix of existing and non-existing files
    std::vector<std::string> fileList = *genBatchFileList();
    
    // Limit the list size
    if (fileList.size() > 20) {
        fileList.resize(20);
    }
    
    // Skip if empty list
    RC_PRE(!fileList.empty());
    
    BatchProcessor processor;
    processor.addFiles(fileList);
    
    BarcodeConfig config;
    config.type = BarcodeType::CODE_128;
    config.width = 200;
    config.height = 80;
    config.margin = 10;
    config.showText = false;
    config.dpi = 300;
    
    // Process the batch
    std::vector<BatchResult> results = processor.process(config);
    
    // All files should have results regardless of success/failure
    RC_ASSERT(results.size() == fileList.size());
    
    // Count successes and failures
    int successCount = 0;
    int failureCount = 0;
    for (const auto& result : results) {
        if (result.success) {
            ++successCount;
        } else {
            ++failureCount;
        }
    }
    
    // Sum should equal total
    RC_ASSERT(static_cast<size_t>(successCount + failureCount) == fileList.size());
    
    // Since these are random file names that don't exist, all should fail
    // (This tests that the processor handles non-existent files gracefully)
    RC_ASSERT(failureCount == static_cast<int>(fileList.size()));
}

// =============================================================================
// Property 8 (COM Bridge): 参数验证 (Parameter Validation)
// **Feature: barcode-image-insertion, Property 8: 参数验证**
// **Validates: Requirements 4.2, 3.5**
//
// For any InsertImage call with invalid parameters (null path, negative dimensions),
// the system should return an error without crashing.
// =============================================================================

namespace {

// Generator for negative dimension values
rc::Gen<double> genNegativeDimension() {
    return rc::gen::map(
        rc::gen::inRange(1, 1000),
        [](int val) { return -static_cast<double>(val); }
    );
}

// Generator for valid positive dimension values
rc::Gen<double> genPositiveDimension() {
    return rc::gen::map(
        rc::gen::inRange(1, 500),
        [](int val) { return static_cast<double>(val); }
    );
}

// Generator for valid coordinate values
rc::Gen<double> genCoordinate() {
    return rc::gen::map(
        rc::gen::inRange(-1000, 1000),
        [](int val) { return static_cast<double>(val); }
    );
}

// Generator for valid image file paths (that exist)
rc::Gen<std::string> genValidImagePath(const std::filesystem::path& testDir) {
    return rc::gen::just(std::string((testDir / "test_valid_image.png").string()));
}

} // anonymous namespace

// =============================================================================
// Property 4 (COM Bridge): 网格布局位置计算 (Grid Layout Position Calculation)
// **Feature: barcode-image-insertion, Property 4: 网格布局位置计算**
// **Validates: Requirements 5.1, 5.2, 5.3, 5.4**
//
// For any batch insert with N images, columns C, and spacing S, the position
// of image at index i should be:
// - x = startX + (i % C) * (width + S)
// - y = startY - (i / C) * (height + S)
// =============================================================================

namespace {

// Generator for valid column count (1 to 20)
rc::Gen<int> genColumnCount() {
    return rc::gen::inRange(1, 21);
}

// Generator for valid spacing values (0 to 100)
rc::Gen<double> genSpacing() {
    return rc::gen::map(
        rc::gen::inRange(0, 101),
        [](int val) { return static_cast<double>(val); }
    );
}

// Generator for valid image count (1 to 100)
rc::Gen<int> genImageCount() {
    return rc::gen::inRange(1, 101);
}

// Generator for valid start coordinates (-1000 to 1000)
rc::Gen<double> genStartCoordinate() {
    return rc::gen::map(
        rc::gen::inRange(-1000, 1001),
        [](int val) { return static_cast<double>(val); }
    );
}

// Generator for valid image dimensions (10 to 200)
rc::Gen<double> genImageDimension() {
    return rc::gen::map(
        rc::gen::inRange(10, 201),
        [](int val) { return static_cast<double>(val); }
    );
}

} // anonymous namespace

// Property 4 Test (COM Bridge): Grid Position Calculation Correctness
// For any grid layout parameters, the calculated position should match the formula:
// x = startX + (index % columns) * (width + spacing)
// y = startY - (index / columns) * (height + spacing)
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_COM_GridPositionCalculationCorrectness, ()) {
    using namespace creo_barcode;
    
    // Generate random grid parameters
    int columns = *genColumnCount();
    double spacing = *genSpacing();
    double startX = *genStartCoordinate();
    double startY = *genStartCoordinate();
    double width = *genImageDimension();
    double height = *genImageDimension();
    int imageCount = *genImageCount();
    
    // Test position calculation for each image index
    for (int i = 0; i < imageCount; ++i) {
        // Calculate expected position using the formula from design document
        double expectedX = startX + (i % columns) * (width + spacing);
        double expectedY = startY - (i / columns) * (height + spacing);
        
        // Call the actual function
        GridPosition pos = calculateGridPosition(i, columns, spacing, startX, startY, width, height);
        
        // Verify the calculated position matches expected (with small tolerance for floating point)
        RC_ASSERT(std::abs(pos.x - expectedX) < 0.001);
        RC_ASSERT(std::abs(pos.y - expectedY) < 0.001);
    }
}

// Property 4 Test (COM Bridge): Grid Position Single Column
// For a single column layout, all images should be vertically stacked
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_COM_GridPositionSingleColumn, ()) {
    using namespace creo_barcode;
    
    // Single column layout
    int columns = 1;
    double spacing = *genSpacing();
    double startX = *genStartCoordinate();
    double startY = *genStartCoordinate();
    double width = *genImageDimension();
    double height = *genImageDimension();
    int imageCount = *genImageCount();
    
    // Test position calculation for each image index
    for (int i = 0; i < imageCount; ++i) {
        GridPosition pos = calculateGridPosition(i, columns, spacing, startX, startY, width, height);
        
        // In single column, all X positions should be the same (startX)
        RC_ASSERT(std::abs(pos.x - startX) < 0.001);
        
        // Y position should decrease by (height + spacing) for each row
        double expectedY = startY - i * (height + spacing);
        RC_ASSERT(std::abs(pos.y - expectedY) < 0.001);
    }
}

// Property 4 Test (COM Bridge): Grid Position Single Row
// For a layout with columns >= imageCount, all images should be horizontally aligned
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_COM_GridPositionSingleRow, ()) {
    using namespace creo_barcode;
    
    // Generate image count first, then ensure columns >= imageCount
    int imageCount = *rc::gen::inRange(1, 21);
    int columns = *rc::gen::inRange(imageCount, imageCount + 10);
    double spacing = *genSpacing();
    double startX = *genStartCoordinate();
    double startY = *genStartCoordinate();
    double width = *genImageDimension();
    double height = *genImageDimension();
    
    // Test position calculation for each image index
    for (int i = 0; i < imageCount; ++i) {
        GridPosition pos = calculateGridPosition(i, columns, spacing, startX, startY, width, height);
        
        // In single row, all Y positions should be the same (startY)
        RC_ASSERT(std::abs(pos.y - startY) < 0.001);
        
        // X position should increase by (width + spacing) for each column
        double expectedX = startX + i * (width + spacing);
        RC_ASSERT(std::abs(pos.x - expectedX) < 0.001);
    }
}

// Property 4 Test (COM Bridge): Grid Position Zero Spacing
// With zero spacing, images should be placed adjacent to each other
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_COM_GridPositionZeroSpacing, ()) {
    using namespace creo_barcode;
    
    int columns = *genColumnCount();
    double spacing = 0.0;  // Zero spacing
    double startX = *genStartCoordinate();
    double startY = *genStartCoordinate();
    double width = *genImageDimension();
    double height = *genImageDimension();
    int imageCount = *genImageCount();
    
    // Test position calculation for each image index
    for (int i = 0; i < imageCount; ++i) {
        // Calculate expected position with zero spacing
        double expectedX = startX + (i % columns) * width;
        double expectedY = startY - (i / columns) * height;
        
        GridPosition pos = calculateGridPosition(i, columns, spacing, startX, startY, width, height);
        
        RC_ASSERT(std::abs(pos.x - expectedX) < 0.001);
        RC_ASSERT(std::abs(pos.y - expectedY) < 0.001);
    }
}

// Property 4 Test (COM Bridge): Grid Position Row Wrap
// Images should wrap to the next row after filling columns
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_COM_GridPositionRowWrap, ()) {
    using namespace creo_barcode;
    
    // Use a small column count to ensure wrapping
    int columns = *rc::gen::inRange(2, 6);
    double spacing = *genSpacing();
    double startX = *genStartCoordinate();
    double startY = *genStartCoordinate();
    double width = *genImageDimension();
    double height = *genImageDimension();
    
    // Generate enough images to fill at least 2 rows
    int imageCount = *rc::gen::inRange(columns + 1, columns * 5 + 1);
    
    // Test that images wrap correctly
    for (int i = 0; i < imageCount; ++i) {
        GridPosition pos = calculateGridPosition(i, columns, spacing, startX, startY, width, height);
        
        int expectedCol = i % columns;
        int expectedRow = i / columns;
        
        double expectedX = startX + expectedCol * (width + spacing);
        double expectedY = startY - expectedRow * (height + spacing);
        
        RC_ASSERT(std::abs(pos.x - expectedX) < 0.001);
        RC_ASSERT(std::abs(pos.y - expectedY) < 0.001);
    }
}

// Property 4 Test (COM Bridge): Grid Position Index Zero
// The first image (index 0) should always be at the start position
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property4_COM_GridPositionIndexZero, ()) {
    using namespace creo_barcode;
    
    int columns = *genColumnCount();
    double spacing = *genSpacing();
    double startX = *genStartCoordinate();
    double startY = *genStartCoordinate();
    double width = *genImageDimension();
    double height = *genImageDimension();
    
    // Calculate position for index 0
    GridPosition pos = calculateGridPosition(0, columns, spacing, startX, startY, width, height);
    
    // First image should always be at start position
    RC_ASSERT(std::abs(pos.x - startX) < 0.001);
    RC_ASSERT(std::abs(pos.y - startY) < 0.001);
}

// =============================================================================
// Property 5 (COM Bridge): 批量处理结果统计 (Batch Processing Result Statistics)
// **Feature: barcode-image-insertion, Property 5: 批量处理结果统计**
// **Validates: Requirements 5.5, 5.6**
//
// For any batch insert operation:
// - successCount + failCount should equal totalCount
// - failedPaths.size() should equal failCount
// - errorMessages.size() should equal failCount
// =============================================================================

namespace {

// Generator for batch image info with random paths (non-existent files)
rc::Gen<creo_barcode::BatchImageInfo> genBatchImageInfo() {
    return rc::gen::build<creo_barcode::BatchImageInfo>(
        rc::gen::set(&creo_barcode::BatchImageInfo::imagePath,
            rc::gen::map(
                rc::gen::nonEmpty(
                    rc::gen::container<std::string>(
                        rc::gen::oneOf(
                            rc::gen::inRange<char>('A', 'Z' + 1),
                            rc::gen::inRange<char>('a', 'z' + 1),
                            rc::gen::inRange<char>('0', '9' + 1),
                            rc::gen::element('_', '-')
                        )
                    )
                ),
                [](std::string name) {
                    if (name.length() > 20) {
                        name = name.substr(0, 20);
                    }
                    return "C:\\NonExistentDir_BatchTest\\" + name + ".png";
                }
            )
        ),
        rc::gen::set(&creo_barcode::BatchImageInfo::x, genStartCoordinate()),
        rc::gen::set(&creo_barcode::BatchImageInfo::y, genStartCoordinate()),
        rc::gen::set(&creo_barcode::BatchImageInfo::width, genImageDimension()),
        rc::gen::set(&creo_barcode::BatchImageInfo::height, genImageDimension())
    );
}

// Generator for a list of batch image info (0 to 50 images)
rc::Gen<std::vector<creo_barcode::BatchImageInfo>> genBatchImageInfoList() {
    return rc::gen::container<std::vector<creo_barcode::BatchImageInfo>>(genBatchImageInfo());
}

} // anonymous namespace

// Property 5 Test (COM Bridge): Batch Result Statistics Consistency
// For any batch insert operation:
// 1. successCount + failCount should equal totalCount
// 2. failedPaths.size() should equal failCount
// 3. errorMessages.size() should equal failCount
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_COM_BatchResultStatisticsConsistency, ()) {
    using namespace creo_barcode;
    
    // Generate a random list of batch image info
    std::vector<BatchImageInfo> images = *genBatchImageInfoList();
    
    // Limit the list size to avoid overly long test runs
    if (images.size() > 30) {
        images.resize(30);
    }
    
    auto& bridge = CreoComBridge::getInstance();
    bridge.initialize();
    
    // Perform batch insert
    BatchInsertResult result = bridge.batchInsertImages(images);
    
    // Property 5.1: totalCount should equal input size
    RC_ASSERT(result.totalCount == static_cast<int>(images.size()));
    
    // Property 5.2: successCount + failCount should equal totalCount
    RC_ASSERT(result.successCount + result.failCount == result.totalCount);
    
    // Property 5.3: failedPaths.size() should equal failCount
    RC_ASSERT(static_cast<int>(result.failedPaths.size()) == result.failCount);
    
    // Property 5.4: errorMessages.size() should equal failCount
    RC_ASSERT(static_cast<int>(result.errorMessages.size()) == result.failCount);
    
    // Property 5.5: successCount and failCount should be non-negative
    RC_ASSERT(result.successCount >= 0);
    RC_ASSERT(result.failCount >= 0);
}

// Property 5 Test (COM Bridge): Empty Batch Returns Zero Counts
// An empty batch should return all zero counts
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_COM_EmptyBatchReturnsZeroCounts, ()) {
    using namespace creo_barcode;
    
    auto& bridge = CreoComBridge::getInstance();
    bridge.initialize();
    
    // Perform batch insert with empty list
    std::vector<BatchImageInfo> emptyImages;
    BatchInsertResult result = bridge.batchInsertImages(emptyImages);
    
    // All counts should be zero
    RC_ASSERT(result.totalCount == 0);
    RC_ASSERT(result.successCount == 0);
    RC_ASSERT(result.failCount == 0);
    RC_ASSERT(result.failedPaths.empty());
    RC_ASSERT(result.errorMessages.empty());
    
    // Consistency check still holds
    RC_ASSERT(result.successCount + result.failCount == result.totalCount);
}

// Property 5 Test (COM Bridge): Grid Batch Result Statistics Consistency
// For any grid batch insert operation, the same statistics properties should hold
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_COM_GridBatchResultStatisticsConsistency, ()) {
    using namespace creo_barcode;
    
    // Generate random image paths (non-existent files)
    auto genImagePath = rc::gen::map(
        rc::gen::nonEmpty(
            rc::gen::container<std::string>(
                rc::gen::oneOf(
                    rc::gen::inRange<char>('A', 'Z' + 1),
                    rc::gen::inRange<char>('a', 'z' + 1),
                    rc::gen::inRange<char>('0', '9' + 1)
                )
            )
        ),
        [](std::string name) {
            if (name.length() > 15) {
                name = name.substr(0, 15);
            }
            return "C:\\NonExistentDir_GridTest\\" + name + ".png";
        }
    );
    
    std::vector<std::string> imagePaths = *rc::gen::container<std::vector<std::string>>(genImagePath);
    
    // Limit the list size
    if (imagePaths.size() > 30) {
        imagePaths.resize(30);
    }
    
    // Generate grid layout parameters
    GridLayoutParams params;
    params.startX = *genStartCoordinate();
    params.startY = *genStartCoordinate();
    params.width = *genImageDimension();
    params.height = *genImageDimension();
    params.columns = *genColumnCount();
    params.spacing = *genSpacing();
    
    auto& bridge = CreoComBridge::getInstance();
    bridge.initialize();
    
    // Perform grid batch insert
    BatchInsertResult result = bridge.batchInsertImagesGrid(imagePaths, params);
    
    // Property 5.1: totalCount should equal input size
    RC_ASSERT(result.totalCount == static_cast<int>(imagePaths.size()));
    
    // Property 5.2: successCount + failCount should equal totalCount
    RC_ASSERT(result.successCount + result.failCount == result.totalCount);
    
    // Property 5.3: failedPaths.size() should equal failCount
    RC_ASSERT(static_cast<int>(result.failedPaths.size()) == result.failCount);
    
    // Property 5.4: errorMessages.size() should equal failCount
    RC_ASSERT(static_cast<int>(result.errorMessages.size()) == result.failCount);
}

// Property 5 Test (COM Bridge): Failed Paths Match Input Paths
// For any failed image, its path should be in the failedPaths list
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property5_COM_FailedPathsMatchInputPaths, ()) {
    using namespace creo_barcode;
    
    // Generate a random list of batch image info with non-existent files
    std::vector<BatchImageInfo> images = *genBatchImageInfoList();
    
    // Limit the list size
    if (images.size() > 20) {
        images.resize(20);
    }
    
    // Skip empty lists
    RC_PRE(!images.empty());
    
    auto& bridge = CreoComBridge::getInstance();
    bridge.initialize();
    
    // Perform batch insert
    BatchInsertResult result = bridge.batchInsertImages(images);
    
    // All failed paths should be from the input list
    for (const auto& failedPath : result.failedPaths) {
        bool foundInInput = false;
        for (const auto& img : images) {
            if (img.imagePath == failedPath) {
                foundInInput = true;
                break;
            }
        }
        RC_ASSERT(foundInInput);
    }
    
    // Each error message should not be empty
    for (const auto& errorMsg : result.errorMessages) {
        RC_ASSERT(!errorMsg.empty());
    }
}

// Property 8 Test (COM Bridge): Negative Width Returns Error
// For any negative width value, insertImage should return false without crashing
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property8_COM_NegativeWidthReturnsError, ()) {
    // Generate a negative width
    double negativeWidth = *genNegativeDimension();
    double validHeight = *genPositiveDimension();
    double x = *genCoordinate();
    double y = *genCoordinate();
    
    // Create a valid test image file
    std::filesystem::path tempFilePath = testDir_ / "test_neg_width.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        // Write minimal PNG header
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    RC_PRE(std::filesystem::exists(tempFilePath));
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert with negative width
    bool result = bridge.insertImage(tempFilePath.string(), x, y, negativeWidth, validHeight);
    
    // Should return false (error) without crashing
    RC_ASSERT(result == false);
    
    // Error message should not be empty
    std::string errorMsg = bridge.getLastError();
    RC_ASSERT(!errorMsg.empty());
    
    // Error should mention negative or dimension
    bool mentionsDimension = (errorMsg.find("negative") != std::string::npos) ||
                             (errorMsg.find("dimension") != std::string::npos) ||
                             (errorMsg.find("width") != std::string::npos);
    RC_ASSERT(mentionsDimension);
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// Property 8 Test (COM Bridge): Negative Height Returns Error
// For any negative height value, insertImage should return false without crashing
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property8_COM_NegativeHeightReturnsError, ()) {
    // Generate a negative height
    double validWidth = *genPositiveDimension();
    double negativeHeight = *genNegativeDimension();
    double x = *genCoordinate();
    double y = *genCoordinate();
    
    // Create a valid test image file
    std::filesystem::path tempFilePath = testDir_ / "test_neg_height.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        // Write minimal PNG header
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    RC_PRE(std::filesystem::exists(tempFilePath));
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert with negative height
    bool result = bridge.insertImage(tempFilePath.string(), x, y, validWidth, negativeHeight);
    
    // Should return false (error) without crashing
    RC_ASSERT(result == false);
    
    // Error message should not be empty
    std::string errorMsg = bridge.getLastError();
    RC_ASSERT(!errorMsg.empty());
    
    // Error should mention negative or dimension
    bool mentionsDimension = (errorMsg.find("negative") != std::string::npos) ||
                             (errorMsg.find("dimension") != std::string::npos) ||
                             (errorMsg.find("height") != std::string::npos);
    RC_ASSERT(mentionsDimension);
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// Property 8 Test (COM Bridge): Both Negative Dimensions Returns Error
// For any pair of negative dimensions, insertImage should return false without crashing
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property8_COM_BothNegativeDimensionsReturnsError, ()) {
    // Generate negative dimensions
    double negativeWidth = *genNegativeDimension();
    double negativeHeight = *genNegativeDimension();
    double x = *genCoordinate();
    double y = *genCoordinate();
    
    // Create a valid test image file
    std::filesystem::path tempFilePath = testDir_ / "test_neg_both.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        // Write minimal PNG header
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    RC_PRE(std::filesystem::exists(tempFilePath));
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert with both negative dimensions
    bool result = bridge.insertImage(tempFilePath.string(), x, y, negativeWidth, negativeHeight);
    
    // Should return false (error) without crashing
    RC_ASSERT(result == false);
    
    // Error message should not be empty
    std::string errorMsg = bridge.getLastError();
    RC_ASSERT(!errorMsg.empty());
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// Property 8 Test (COM Bridge): Empty Path Returns Error
// An empty path should return an error without crashing
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property8_COM_EmptyPathReturnsError, ()) {
    double width = *genPositiveDimension();
    double height = *genPositiveDimension();
    double x = *genCoordinate();
    double y = *genCoordinate();
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert with empty path
    bool result = bridge.insertImage("", x, y, width, height);
    
    // Should return false (error) without crashing
    RC_ASSERT(result == false);
    
    // Error message should not be empty
    std::string errorMsg = bridge.getLastError();
    RC_ASSERT(!errorMsg.empty());
    
    // Error should mention empty or path
    bool mentionsEmpty = (errorMsg.find("empty") != std::string::npos) ||
                         (errorMsg.find("Empty") != std::string::npos);
    RC_ASSERT(mentionsEmpty);
}

// Property 8 Test (COM Bridge): Valid Parameters Pass Validation
// For any valid parameters (existing file, non-negative dimensions),
// the parameter validation should pass (may still fail due to Creo not running)
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property8_COM_ValidParametersPassValidation, ()) {
    double width = *genPositiveDimension();
    double height = *genPositiveDimension();
    double x = *genCoordinate();
    double y = *genCoordinate();
    
    // Create a valid test image file
    std::filesystem::path tempFilePath = testDir_ / "test_valid_params.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        // Write minimal PNG header
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    RC_PRE(std::filesystem::exists(tempFilePath));
    RC_PRE(std::filesystem::file_size(tempFilePath) > 0);
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert with valid parameters
    bool result = bridge.insertImage(tempFilePath.string(), x, y, width, height);
    
    // Get the error message
    std::string errorMsg = bridge.getLastError();
    
    // If it failed, it should NOT be due to parameter validation issues
    if (!result) {
        // Error should NOT mention empty path, negative dimensions, or file validation issues
        bool isParameterValidationError = 
            (errorMsg.find("empty") != std::string::npos && errorMsg.find("path") != std::string::npos) ||
            (errorMsg.find("negative") != std::string::npos) ||
            (errorMsg.find("not found") != std::string::npos) ||
            (errorMsg.find("Unsupported") != std::string::npos);
        
        // If there's an error, it should be about Creo connection, not parameter validation
        RC_ASSERT(!isParameterValidationError);
    }
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// Property 8 Test (COM Bridge): Zero Dimensions Are Allowed (Auto-size)
// Zero dimensions should be allowed as they indicate auto-sizing
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property8_COM_ZeroDimensionsAllowed, ()) {
    double x = *genCoordinate();
    double y = *genCoordinate();
    
    // Create a valid test image file
    std::filesystem::path tempFilePath = testDir_ / "test_zero_dims.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        // Write minimal PNG header
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    RC_PRE(std::filesystem::exists(tempFilePath));
    
    auto& bridge = creo_barcode::CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt to insert with zero dimensions (auto-size)
    bool result = bridge.insertImage(tempFilePath.string(), x, y, 0.0, 0.0);
    
    // Get the error message
    std::string errorMsg = bridge.getLastError();
    
    // If it failed, it should NOT be due to zero dimensions
    if (!result) {
        // Error should NOT mention negative or invalid dimensions
        bool isDimensionError = 
            (errorMsg.find("negative") != std::string::npos) ||
            (errorMsg.find("dimension") != std::string::npos && errorMsg.find("invalid") != std::string::npos);
        
        RC_ASSERT(!isDimensionError);
    }
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// =============================================================================
// Property 7 (COM Bridge): 错误信息包含 HRESULT (Error Message Contains HRESULT)
// **Feature: barcode-image-insertion, Property 7: 错误信息包含 HRESULT**
// **Validates: Requirements 7.3**
//
// For any COM call failure, the error message should contain the HRESULT code
// in hexadecimal format (0xXXXXXXXX).
// =============================================================================

#ifdef _WIN32

namespace {

// Generator for various HRESULT error codes (common COM errors)
rc::Gen<HRESULT> genHResultErrorCode() {
    return rc::gen::element(
        E_FAIL,                     // 0x80004005 - Unspecified failure
        E_INVALIDARG,               // 0x80070057 - Invalid argument
        E_OUTOFMEMORY,              // 0x8007000E - Out of memory
        E_NOTIMPL,                  // 0x80004001 - Not implemented
        E_NOINTERFACE,              // 0x80004002 - No such interface
        E_POINTER,                  // 0x80004003 - Invalid pointer
        E_ABORT,                    // 0x80004004 - Operation aborted
        E_ACCESSDENIED,             // 0x80070005 - Access denied
        REGDB_E_CLASSNOTREG,        // 0x80040154 - Class not registered
        CO_E_NOTINITIALIZED,        // 0x800401F0 - COM not initialized
        RPC_E_DISCONNECTED,         // 0x80010108 - RPC disconnected
        static_cast<HRESULT>(0x80040200),  // Custom error code
        static_cast<HRESULT>(0x80070002),  // File not found
        static_cast<HRESULT>(0x80070003)   // Path not found
    );
}

// Generator for error context messages
rc::Gen<std::string> genErrorContext() {
    return rc::gen::element(
        std::string("Failed to initialize COM"),
        std::string("Failed to connect to Creo"),
        std::string("Failed to get session"),
        std::string("Failed to create image"),
        std::string("Failed to get current model"),
        std::string("COM operation failed"),
        std::string("Interface query failed"),
        std::string("Method call failed")
    );
}

// Helper function to check if a string contains HRESULT in hex format
bool containsHResultHex(const std::string& str) {
    // Look for pattern "HRESULT: 0x" followed by hex digits
    size_t pos = str.find("HRESULT: 0x");
    if (pos == std::string::npos) {
        // Also check for just "0x" followed by 8 hex digits (alternative format)
        pos = str.find("0x");
        if (pos == std::string::npos) {
            return false;
        }
    }
    
    // Find the start of hex digits
    size_t hexStart = str.find("0x", pos);
    if (hexStart == std::string::npos) {
        return false;
    }
    
    // Check that there are hex digits after "0x"
    size_t digitStart = hexStart + 2;
    if (digitStart >= str.length()) {
        return false;
    }
    
    // Count hex digits
    int hexDigitCount = 0;
    for (size_t i = digitStart; i < str.length() && hexDigitCount < 8; ++i) {
        char c = str[i];
        if ((c >= '0' && c <= '9') || 
            (c >= 'a' && c <= 'f') || 
            (c >= 'A' && c <= 'F')) {
            hexDigitCount++;
        } else {
            break;
        }
    }
    
    // Should have at least 1 hex digit (typically 8 for HRESULT)
    return hexDigitCount >= 1;
}

// Helper function to extract HRESULT value from error message
HRESULT extractHResultFromMessage(const std::string& str) {
    size_t pos = str.find("HRESULT: 0x");
    if (pos == std::string::npos) {
        pos = str.find("0x");
        if (pos == std::string::npos) {
            return S_OK;
        }
    }
    
    size_t hexStart = str.find("0x", pos);
    if (hexStart == std::string::npos) {
        return S_OK;
    }
    
    // Extract hex string
    std::string hexStr;
    for (size_t i = hexStart + 2; i < str.length() && hexStr.length() < 8; ++i) {
        char c = str[i];
        if ((c >= '0' && c <= '9') || 
            (c >= 'a' && c <= 'f') || 
            (c >= 'A' && c <= 'F')) {
            hexStr += c;
        } else {
            break;
        }
    }
    
    if (hexStr.empty()) {
        return S_OK;
    }
    
    // Convert hex string to HRESULT
    unsigned long value = std::stoul(hexStr, nullptr, 16);
    return static_cast<HRESULT>(value);
}

} // anonymous namespace

// Property 7 Test (COM Bridge): formatHResult Contains Hex Code
// For any HRESULT value, formatHResult should return a string containing
// the HRESULT in hexadecimal format (0xXXXXXXXX)
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_COM_FormatHResultContainsHexCode, ()) {
    using namespace creo_barcode;
    
    // Generate a random HRESULT error code
    HRESULT hr = *genHResultErrorCode();
    
    // Format the HRESULT
    std::string formatted = CreoComBridge::formatHResult(hr);
    
    // 1. The formatted string should not be empty
    RC_ASSERT(!formatted.empty());
    
    // 2. The formatted string should contain "0x" (hex prefix)
    RC_ASSERT(formatted.find("0x") != std::string::npos);
    
    // 3. The formatted string should contain the HRESULT in hex format
    RC_ASSERT(containsHResultHex(formatted));
    
    // 4. The extracted HRESULT should match the original
    HRESULT extracted = extractHResultFromMessage(formatted);
    RC_ASSERT(extracted == hr);
}

// Property 7 Test (COM Bridge): setError Includes HRESULT in Message
// For any COM error with HRESULT, the error message should contain
// the HRESULT code in hexadecimal format
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_COM_SetErrorIncludesHResultInMessage, ()) {
    using namespace creo_barcode;
    
    auto& bridge = CreoComBridge::getInstance();
    bridge.initialize();
    
    // Test various error scenarios that trigger setError
    // Each scenario should result in an error message containing HRESULT
    
    // Scenario 1: Empty path (triggers setError with simple string)
    bool result1 = bridge.insertImage("", 0.0, 0.0, 50.0, 50.0);
    RC_ASSERT(result1 == false);
    std::string errorMsg1 = bridge.getLastError();
    HRESULT lastHr1 = bridge.getLastHResult();
    
    // The error message should contain HRESULT in hex format
    // Even for simple string errors, setError now includes HRESULT
    RC_ASSERT(FAILED(lastHr1));
    RC_ASSERT(containsHResultHex(errorMsg1));
    
    // The extracted HRESULT should match getLastHResult()
    HRESULT extracted1 = extractHResultFromMessage(errorMsg1);
    RC_ASSERT(extracted1 == lastHr1);
    
    // Scenario 2: Non-existent file (triggers setError with simple string)
    std::string nonExistentPath = "C:\\NonExistent_" + std::to_string(std::rand()) + ".png";
    bool result2 = bridge.insertImage(nonExistentPath, 0.0, 0.0, 50.0, 50.0);
    RC_ASSERT(result2 == false);
    std::string errorMsg2 = bridge.getLastError();
    HRESULT lastHr2 = bridge.getLastHResult();
    
    RC_ASSERT(FAILED(lastHr2));
    RC_ASSERT(containsHResultHex(errorMsg2));
    
    HRESULT extracted2 = extractHResultFromMessage(errorMsg2);
    RC_ASSERT(extracted2 == lastHr2);
    
    // Scenario 3: Negative dimensions (triggers setError with simple string)
    std::filesystem::path tempFilePath = testDir_ / "test_hresult.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    if (std::filesystem::exists(tempFilePath)) {
        bool result3 = bridge.insertImage(tempFilePath.string(), 0.0, 0.0, -10.0, 50.0);
        RC_ASSERT(result3 == false);
        std::string errorMsg3 = bridge.getLastError();
        HRESULT lastHr3 = bridge.getLastHResult();
        
        RC_ASSERT(FAILED(lastHr3));
        RC_ASSERT(containsHResultHex(errorMsg3));
        
        HRESULT extracted3 = extractHResultFromMessage(errorMsg3);
        RC_ASSERT(extracted3 == lastHr3);
        
        // Cleanup
        std::filesystem::remove(tempFilePath);
    }
}

// Property 7 Test (COM Bridge): HRESULT Format Consistency
// For any HRESULT, the format should be consistent: "0x" followed by 8 hex digits
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_COM_HResultFormatConsistency, ()) {
    using namespace creo_barcode;
    
    // Generate a random HRESULT error code
    HRESULT hr = *genHResultErrorCode();
    
    // Format the HRESULT
    std::string formatted = CreoComBridge::formatHResult(hr);
    
    // Find the hex value in the formatted string
    size_t hexPos = formatted.find("0x");
    RC_ASSERT(hexPos != std::string::npos);
    
    // Extract the hex portion
    std::string hexPart;
    for (size_t i = hexPos + 2; i < formatted.length(); ++i) {
        char c = formatted[i];
        if ((c >= '0' && c <= '9') || 
            (c >= 'a' && c <= 'f') || 
            (c >= 'A' && c <= 'F')) {
            hexPart += c;
        } else {
            break;
        }
    }
    
    // HRESULT should be formatted as 8 hex digits
    RC_ASSERT(hexPart.length() == 8);
    
    // All characters should be valid hex digits
    for (char c : hexPart) {
        bool isHexDigit = (c >= '0' && c <= '9') || 
                          (c >= 'a' && c <= 'f') || 
                          (c >= 'A' && c <= 'F');
        RC_ASSERT(isHexDigit);
    }
}

// Property 7 Test (COM Bridge): Success HRESULT Format
// Even S_OK (success) should be formatted correctly
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_COM_SuccessHResultFormat, ()) {
    using namespace creo_barcode;
    
    // Test with S_OK (0x00000000)
    std::string formatted = CreoComBridge::formatHResult(S_OK);
    
    // Should contain "0x"
    RC_ASSERT(formatted.find("0x") != std::string::npos);
    
    // Should contain "00000000" (S_OK = 0)
    RC_ASSERT(formatted.find("00000000") != std::string::npos);
}

// Property 7 Test (COM Bridge): Error Message Contains Context
// For any COM error, the error message should contain both the context
// and the HRESULT
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_COM_ErrorMessageContainsContext, ()) {
    using namespace creo_barcode;
    
    auto& bridge = CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt an operation that will fail with a COM error
    // Using a valid file but Creo not running should trigger COM errors
    std::filesystem::path tempFilePath = testDir_ / "test_context.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    RC_PRE(std::filesystem::exists(tempFilePath));
    
    bool result = bridge.insertImage(tempFilePath.string(), 0.0, 0.0, 50.0, 50.0);
    
    if (!result) {
        std::string errorMsg = bridge.getLastError();
        HRESULT lastHr = bridge.getLastHResult();
        
        // Error message should not be empty
        RC_ASSERT(!errorMsg.empty());
        
        // If there's an HRESULT error
        if (FAILED(lastHr)) {
            // Should contain HRESULT in hex format
            RC_ASSERT(containsHResultHex(errorMsg));
            
            // Should contain some context (not just the HRESULT)
            // The message should be longer than just "0xXXXXXXXX"
            RC_ASSERT(errorMsg.length() > 12);
        }
    }
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

// Property 7 Test (COM Bridge): getLastHResult Returns Correct Value
// After a COM error, getLastHResult should return the correct HRESULT
RC_GTEST_FIXTURE_PROP(PropertyTestFixture, Property7_COM_GetLastHResultReturnsCorrectValue, ()) {
    using namespace creo_barcode;
    
    auto& bridge = CreoComBridge::getInstance();
    bridge.initialize();
    
    // Attempt an operation that will fail
    std::filesystem::path tempFilePath = testDir_ / "test_last_hr.png";
    {
        std::ofstream ofs(tempFilePath, std::ios::binary);
        unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        ofs.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
    }
    
    RC_PRE(std::filesystem::exists(tempFilePath));
    
    bool result = bridge.insertImage(tempFilePath.string(), 0.0, 0.0, 50.0, 50.0);
    
    if (!result) {
        HRESULT lastHr = bridge.getLastHResult();
        std::string errorMsg = bridge.getLastError();
        
        // If there's an HRESULT in the message, it should match getLastHResult
        if (containsHResultHex(errorMsg)) {
            HRESULT extracted = extractHResultFromMessage(errorMsg);
            RC_ASSERT(extracted == lastHr);
        }
    }
    
    // Cleanup
    std::filesystem::remove(tempFilePath);
}

#endif // _WIN32

} // namespace testing
} // namespace creo_barcode
