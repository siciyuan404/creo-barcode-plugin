#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <string>

namespace creo_barcode {

enum class ErrorCode {
    SUCCESS = 0,
    VERSION_INCOMPATIBLE,
    NO_DRAWING_OPEN,
    NO_MODEL_ASSOCIATED,
    BARCODE_GENERATION_FAILED,
    IMAGE_INSERT_FAILED,
    CONFIG_LOAD_FAILED,
    CONFIG_SAVE_FAILED,
    FILE_NOT_FOUND,
    INVALID_BARCODE_TYPE,
    INVALID_DATA,
    BATCH_PARTIAL_FAILURE,
    DECODE_FAILED,
    INVALID_SIZE,
    DATA_OUT_OF_SYNC,
    SYNC_CHECK_FAILED
};

struct ErrorInfo {
    ErrorCode code;
    std::string message;
    std::string details;
    
    ErrorInfo() : code(ErrorCode::SUCCESS) {}
    ErrorInfo(ErrorCode c, const std::string& msg, const std::string& det = "")
        : code(c), message(msg), details(det) {}
    
    bool isSuccess() const { return code == ErrorCode::SUCCESS; }
};

inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "Success";
        case ErrorCode::VERSION_INCOMPATIBLE: return "Version incompatible";
        case ErrorCode::NO_DRAWING_OPEN: return "No drawing open";
        case ErrorCode::NO_MODEL_ASSOCIATED: return "No model associated";
        case ErrorCode::BARCODE_GENERATION_FAILED: return "Barcode generation failed";
        case ErrorCode::IMAGE_INSERT_FAILED: return "Image insert failed";
        case ErrorCode::CONFIG_LOAD_FAILED: return "Config load failed";
        case ErrorCode::CONFIG_SAVE_FAILED: return "Config save failed";
        case ErrorCode::FILE_NOT_FOUND: return "File not found";
        case ErrorCode::INVALID_BARCODE_TYPE: return "Invalid barcode type";
        case ErrorCode::INVALID_DATA: return "Invalid data";
        case ErrorCode::BATCH_PARTIAL_FAILURE: return "Batch partial failure";
        case ErrorCode::DECODE_FAILED: return "Decode failed";
        case ErrorCode::INVALID_SIZE: return "Invalid size";
        case ErrorCode::DATA_OUT_OF_SYNC: return "Data out of sync";
        case ErrorCode::SYNC_CHECK_FAILED: return "Sync check failed";
        default: return "Unknown error";
    }
}

} // namespace creo_barcode

#endif // ERROR_CODES_H
