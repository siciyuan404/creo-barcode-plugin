/**
 * @file settings_dialog.cpp
 * @brief Implementation of SettingsDialog class
 * 
 * This file implements the settings dialog functionality for the
 * Creo Barcode Plugin, including parameter validation and UI handling.
 * 
 * Requirements: 2.1, 2.4
 */

#include "settings_dialog.h"
#include "logger.h"

#include <algorithm>
#include <sstream>

namespace creo_barcode {

// Dialog name constant
static const char* DIALOG_NAME = "barcode_settings";

SettingsDialog::SettingsDialog() {
    LOG_INFO("SettingsDialog created");
}

SettingsDialog::~SettingsDialog() {
    LOG_INFO("SettingsDialog destroyed");
}

void SettingsDialog::setError(ErrorCode code, const std::string& message, const std::string& details) {
    lastError_ = ErrorInfo(code, message, details);
    if (code != ErrorCode::SUCCESS) {
        LOG_ERROR(message + (details.empty() ? "" : ": " + details));
    }
}

std::vector<std::string> SettingsDialog::getBarcodeTypeNames() {
    return {
        "Code 128",
        "Code 39",
        "QR Code",
        "Data Matrix",
        "EAN-13"
    };
}

ValidationResult SettingsDialog::validateWidth(int width) {
    if (width < MIN_WIDTH) {
        std::ostringstream oss;
        oss << "Width must be at least " << MIN_WIDTH << " pixels";
        return ValidationResult::failure(oss.str(), "width");
    }
    if (width > MAX_WIDTH) {
        std::ostringstream oss;
        oss << "Width must not exceed " << MAX_WIDTH << " pixels";
        return ValidationResult::failure(oss.str(), "width");
    }
    return ValidationResult::success();
}

ValidationResult SettingsDialog::validateHeight(int height) {
    if (height < MIN_HEIGHT) {
        std::ostringstream oss;
        oss << "Height must be at least " << MIN_HEIGHT << " pixels";
        return ValidationResult::failure(oss.str(), "height");
    }
    if (height > MAX_HEIGHT) {
        std::ostringstream oss;
        oss << "Height must not exceed " << MAX_HEIGHT << " pixels";
        return ValidationResult::failure(oss.str(), "height");
    }
    return ValidationResult::success();
}

ValidationResult SettingsDialog::validateMargin(int margin) {
    if (margin < MIN_MARGIN) {
        std::ostringstream oss;
        oss << "Margin must be at least " << MIN_MARGIN << " pixels";
        return ValidationResult::failure(oss.str(), "margin");
    }
    if (margin > MAX_MARGIN) {
        std::ostringstream oss;
        oss << "Margin must not exceed " << MAX_MARGIN << " pixels";
        return ValidationResult::failure(oss.str(), "margin");
    }
    return ValidationResult::success();
}

ValidationResult SettingsDialog::validateDpi(int dpi) {
    if (dpi < MIN_DPI) {
        std::ostringstream oss;
        oss << "DPI must be at least " << MIN_DPI;
        return ValidationResult::failure(oss.str(), "dpi");
    }
    if (dpi > MAX_DPI) {
        std::ostringstream oss;
        oss << "DPI must not exceed " << MAX_DPI;
        return ValidationResult::failure(oss.str(), "dpi");
    }
    return ValidationResult::success();
}

ValidationResult SettingsDialog::validateConfig(const BarcodeConfig& config) {
    // Validate width
    ValidationResult result = validateWidth(config.width);
    if (!result.valid) {
        return result;
    }
    
    // Validate height
    result = validateHeight(config.height);
    if (!result.valid) {
        return result;
    }
    
    // Validate margin
    result = validateMargin(config.margin);
    if (!result.valid) {
        return result;
    }
    
    // Validate DPI
    result = validateDpi(config.dpi);
    if (!result.valid) {
        return result;
    }
    
    // Validate barcode type
    int typeValue = static_cast<int>(config.type);
    if (typeValue < 0 || typeValue > static_cast<int>(BarcodeType::EAN_13)) {
        return ValidationResult::failure("Invalid barcode type", "type");
    }
    
    return ValidationResult::success();
}

// Non-Creo implementation (for barcode_core library)

DialogResult SettingsDialog::show(const BarcodeConfig& initialConfig) {
    DialogResult result;
    result.config = initialConfig;
    result.accepted = false;
    
    LOG_INFO("Settings dialog requested (standalone mode)");
    
    // In standalone mode, we can't show a real dialog
    // This is primarily for testing - the result can be set programmatically
    
    // Validate the initial config
    ValidationResult validation = validateConfig(initialConfig);
    if (!validation.valid) {
        setError(ErrorCode::INVALID_DATA, validation.errorMessage, validation.fieldName);
    }
    
    return result;
}

} // namespace creo_barcode
