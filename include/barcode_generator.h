#ifndef BARCODE_GENERATOR_H
#define BARCODE_GENERATOR_H

#include <string>
#include <optional>
#include "error_codes.h"

namespace creo_barcode {

enum class BarcodeType {
    CODE_128,
    CODE_39,
    QR_CODE,
    DATA_MATRIX,
    EAN_13
};

struct BarcodeConfig {
    BarcodeType type = BarcodeType::CODE_128;
    int width = 200;
    int height = 80;
    int margin = 10;
    bool showText = true;
    int dpi = 300;
};

class BarcodeGenerator {
public:
    BarcodeGenerator() = default;
    ~BarcodeGenerator() = default;
    
    // Generate barcode image
    bool generate(const std::string& data,
                  const BarcodeConfig& config,
                  const std::string& outputPath);
    
    // Validate barcode data for specific type
    bool validateData(const std::string& data, BarcodeType type);
    
    // Encode special characters for barcode compatibility
    std::string encodeSpecialChars(const std::string& input);
    
    // Decode special characters (reverse of encodeSpecialChars)
    std::string decodeSpecialChars(const std::string& input);
    
    // Decode barcode from image (for verification)
    std::optional<std::string> decode(const std::string& imagePath);
    
    // Get image dimensions
    bool getImageSize(const std::string& imagePath, int& width, int& height);
    
    // Get last error
    ErrorInfo getLastError() const { return lastError_; }
    
private:
    bool generateCode128(const std::string& data, const BarcodeConfig& config, const std::string& outputPath);
    bool generateCode39(const std::string& data, const BarcodeConfig& config, const std::string& outputPath);
    bool generateQRCode(const std::string& data, const BarcodeConfig& config, const std::string& outputPath);
    
    ErrorInfo lastError_;
};

// Utility functions
std::string barcodeTypeToString(BarcodeType type);
std::optional<BarcodeType> stringToBarcodeType(const std::string& str);

} // namespace creo_barcode

#endif // BARCODE_GENERATOR_H
