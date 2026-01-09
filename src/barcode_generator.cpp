#include "barcode_generator.h"
#include <BarcodeFormat.h>
#include <MultiFormatWriter.h>
#include <BitMatrix.h>
#include <ReadBarcode.h>
#include <ImageView.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace creo_barcode {

namespace {

ZXing::BarcodeFormat toZXingFormat(BarcodeType type) {
    switch (type) {
        case BarcodeType::CODE_128: return ZXing::BarcodeFormat::Code128;
        case BarcodeType::CODE_39: return ZXing::BarcodeFormat::Code39;
        case BarcodeType::QR_CODE: return ZXing::BarcodeFormat::QRCode;
        case BarcodeType::DATA_MATRIX: return ZXing::BarcodeFormat::DataMatrix;
        case BarcodeType::EAN_13: return ZXing::BarcodeFormat::EAN13;
        default: return ZXing::BarcodeFormat::Code128;
    }
}

std::vector<uint8_t> scaleImage(const std::vector<uint8_t>& src, 
                                 int srcWidth, int srcHeight,
                                 int dstWidth, int dstHeight) {
    std::vector<uint8_t> dst(dstWidth * dstHeight);
    double xRatio = static_cast<double>(srcWidth) / dstWidth;
    double yRatio = static_cast<double>(srcHeight) / dstHeight;
    for (int y = 0; y < dstHeight; ++y) {
        for (int x = 0; x < dstWidth; ++x) {
            int srcX = static_cast<int>(x * xRatio);
            int srcY = static_cast<int>(y * yRatio);
            dst[y * dstWidth + x] = src[srcY * srcWidth + srcX];
        }
    }
    return dst;
}

} // anonymous namespace


std::string BarcodeGenerator::encodeSpecialChars(const std::string& input) {
    std::ostringstream encoded;
    for (unsigned char c : input) {
        if (c < 32 || c > 126) {
            // Encode non-printable characters as \xNN
            encoded << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        } else if (c == '\\') {
            // Escape backslash to avoid ambiguity with \xNN sequences
            encoded << "\\\\";
        } else {
            encoded << c;
        }
    }
    return encoded.str();
}

std::string BarcodeGenerator::decodeSpecialChars(const std::string& input) {
    std::string decoded;
    size_t i = 0;
    while (i < input.length()) {
        if (input[i] == '\\' && i + 1 < input.length()) {
            if (input[i + 1] == '\\') {
                // Escaped backslash: double backslash becomes single backslash
                decoded += '\\';
                i += 2;
            } else if (input[i + 1] == 'x' && i + 3 < input.length()) {
                // Hex escape: \xNN -> character
                std::string hexStr = input.substr(i + 2, 2);
                try {
                    int value = std::stoi(hexStr, nullptr, 16);
                    decoded += static_cast<char>(value);
                    i += 4;
                } catch (...) {
                    // Invalid hex, keep original character
                    decoded += input[i];
                    ++i;
                }
            } else {
                // Unknown escape, keep original character
                decoded += input[i];
                ++i;
            }
        } else {
            decoded += input[i];
            ++i;
        }
    }
    return decoded;
}

bool BarcodeGenerator::validateData(const std::string& data, BarcodeType type) {
    if (data.empty()) return false;
    
    switch (type) {
        case BarcodeType::CODE_39:
            for (char c : data) {
                char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                if (!std::isalnum(static_cast<unsigned char>(upper)) && 
                    c != ' ' && c != '-' && c != '.' && c != '$' && 
                    c != '/' && c != '+' && c != '%') {
                    return false;
                }
                if (std::isalpha(static_cast<unsigned char>(c)) && std::islower(static_cast<unsigned char>(c))) {
                    return false;
                }
            }
            return true;
            
        case BarcodeType::EAN_13:
            if (data.length() != 12 && data.length() != 13) return false;
            for (char c : data) {
                if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            }
            return true;
            
        case BarcodeType::CODE_128:
        case BarcodeType::QR_CODE:
        case BarcodeType::DATA_MATRIX:
            return true;
            
        default:
            return false;
    }
}


bool BarcodeGenerator::generate(const std::string& data,
                                const BarcodeConfig& config,
                                const std::string& outputPath) {
    if (data.empty()) {
        lastError_ = ErrorInfo(ErrorCode::INVALID_DATA, "Empty data");
        return false;
    }
    
    if (config.width <= 0 || config.height <= 0) {
        lastError_ = ErrorInfo(ErrorCode::INVALID_SIZE, "Invalid dimensions");
        return false;
    }
    
    if (!validateData(data, config.type)) {
        lastError_ = ErrorInfo(ErrorCode::INVALID_DATA, "Data not valid for barcode type");
        return false;
    }
    
    try {
        auto format = toZXingFormat(config.type);
        auto writer = ZXing::MultiFormatWriter(format);
        
        int genWidth = config.width;
        int genHeight = config.height;
        
        auto matrix = writer.encode(data, genWidth, genHeight);
        int matrixWidth = matrix.width();
        int matrixHeight = matrix.height();
        
        std::vector<uint8_t> pixels(matrixWidth * matrixHeight);
        for (int y = 0; y < matrixHeight; ++y) {
            for (int x = 0; x < matrixWidth; ++x) {
                pixels[y * matrixWidth + x] = matrix.get(x, y) ? 0 : 255;
            }
        }
        
        std::vector<uint8_t> finalPixels;
        int finalWidth = config.width;
        int finalHeight = config.height;
        
        if (matrixWidth != config.width || matrixHeight != config.height) {
            finalPixels = scaleImage(pixels, matrixWidth, matrixHeight, config.width, config.height);
        } else {
            finalPixels = std::move(pixels);
        }
        
        if (!stbi_write_png(outputPath.c_str(), finalWidth, finalHeight, 1, 
                           finalPixels.data(), finalWidth)) {
            lastError_ = ErrorInfo(ErrorCode::BARCODE_GENERATION_FAILED, "Failed to write image");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        lastError_ = ErrorInfo(ErrorCode::BARCODE_GENERATION_FAILED, e.what());
        return false;
    }
}

std::optional<std::string> BarcodeGenerator::decode(const std::string& imagePath) {
    int width, height, channels;
    unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 1);
    
    if (!data) {
        lastError_ = ErrorInfo(ErrorCode::FILE_NOT_FOUND, "Failed to load image");
        return std::nullopt;
    }
    
    try {
        auto image = ZXing::ImageView(data, width, height, ZXing::ImageFormat::Lum);
        auto result = ZXing::ReadBarcode(image);
        
        stbi_image_free(data);
        
        if (result.isValid()) {
            return result.text();
        }
        
        lastError_ = ErrorInfo(ErrorCode::DECODE_FAILED, "No barcode found");
        return std::nullopt;
    } catch (const std::exception& e) {
        stbi_image_free(data);
        lastError_ = ErrorInfo(ErrorCode::DECODE_FAILED, e.what());
        return std::nullopt;
    }
}


bool BarcodeGenerator::getImageSize(const std::string& imagePath, int& width, int& height) {
    int channels;
    unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        return false;
    }
    stbi_image_free(data);
    return true;
}

bool BarcodeGenerator::generateCode128(const std::string& data, 
                                       const BarcodeConfig& config, 
                                       const std::string& outputPath) {
    BarcodeConfig cfg = config;
    cfg.type = BarcodeType::CODE_128;
    return generate(data, cfg, outputPath);
}

bool BarcodeGenerator::generateCode39(const std::string& data, 
                                      const BarcodeConfig& config, 
                                      const std::string& outputPath) {
    BarcodeConfig cfg = config;
    cfg.type = BarcodeType::CODE_39;
    return generate(data, cfg, outputPath);
}

bool BarcodeGenerator::generateQRCode(const std::string& data, 
                                      const BarcodeConfig& config, 
                                      const std::string& outputPath) {
    BarcodeConfig cfg = config;
    cfg.type = BarcodeType::QR_CODE;
    return generate(data, cfg, outputPath);
}

std::string barcodeTypeToString(BarcodeType type) {
    switch (type) {
        case BarcodeType::CODE_128: return "CODE_128";
        case BarcodeType::CODE_39: return "CODE_39";
        case BarcodeType::QR_CODE: return "QR_CODE";
        case BarcodeType::DATA_MATRIX: return "DATA_MATRIX";
        case BarcodeType::EAN_13: return "EAN_13";
        default: return "UNKNOWN";
    }
}

std::optional<BarcodeType> stringToBarcodeType(const std::string& str) {
    if (str == "CODE_128") return BarcodeType::CODE_128;
    if (str == "CODE_39") return BarcodeType::CODE_39;
    if (str == "QR_CODE") return BarcodeType::QR_CODE;
    if (str == "DATA_MATRIX") return BarcodeType::DATA_MATRIX;
    if (str == "EAN_13") return BarcodeType::EAN_13;
    return std::nullopt;
}

} // namespace creo_barcode
