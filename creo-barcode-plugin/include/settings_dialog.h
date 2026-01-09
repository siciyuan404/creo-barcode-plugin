/**
 * @file settings_dialog.h
 * @brief Settings dialog for Creo Barcode Plugin
 * 
 * This file defines the SettingsDialog class which handles:
 * - Barcode settings UI
 * - Parameter input and validation
 * - User preference management
 * 
 * Requirements: 2.1, 2.4
 */

#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <string>
#include <vector>
#include <functional>
#include "barcode_generator.h"
#include "error_codes.h"

namespace creo_barcode {

/**
 * @brief Validation result for settings input
 */
struct ValidationResult {
    bool valid = true;
    std::string errorMessage;
    std::string fieldName;
    
    ValidationResult() = default;
    ValidationResult(bool v, const std::string& msg = "", const std::string& field = "")
        : valid(v), errorMessage(msg), fieldName(field) {}
    
    static ValidationResult success() { return ValidationResult(true); }
    static ValidationResult failure(const std::string& msg, const std::string& field = "") {
        return ValidationResult(false, msg, field);
    }
};

/**
 * @brief Settings dialog result
 */
struct DialogResult {
    bool accepted = false;
    BarcodeConfig config;
};

/**
 * @brief SettingsDialog class handles barcode settings UI
 * 
 * This class is responsible for:
 * - Displaying barcode configuration options
 * - Validating user input
 * - Returning configured settings
 */
class SettingsDialog {
public:
    // Minimum and maximum values for settings
    static constexpr int MIN_WIDTH = 50;
    static constexpr int MAX_WIDTH = 1000;
    static constexpr int MIN_HEIGHT = 20;
    static constexpr int MAX_HEIGHT = 500;
    static constexpr int MIN_MARGIN = 0;
    static constexpr int MAX_MARGIN = 50;
    static constexpr int MIN_DPI = 72;
    static constexpr int MAX_DPI = 600;
    
    SettingsDialog();
    ~SettingsDialog();
    
    /**
     * @brief Show the settings dialog
     * @param initialConfig Initial configuration to display
     * @return DialogResult with user's choices
     */
    DialogResult show(const BarcodeConfig& initialConfig);
    
    /**
     * @brief Validate barcode configuration
     * @param config Configuration to validate
     * @return ValidationResult indicating success or failure
     */
    static ValidationResult validateConfig(const BarcodeConfig& config);
    
    /**
     * @brief Validate width parameter
     * @param width Width value to validate
     * @return ValidationResult indicating success or failure
     */
    static ValidationResult validateWidth(int width);
    
    /**
     * @brief Validate height parameter
     * @param height Height value to validate
     * @return ValidationResult indicating success or failure
     */
    static ValidationResult validateHeight(int height);
    
    /**
     * @brief Validate margin parameter
     * @param margin Margin value to validate
     * @return ValidationResult indicating success or failure
     */
    static ValidationResult validateMargin(int margin);
    
    /**
     * @brief Validate DPI parameter
     * @param dpi DPI value to validate
     * @return ValidationResult indicating success or failure
     */
    static ValidationResult validateDpi(int dpi);
    
    /**
     * @brief Get available barcode types as strings
     * @return Vector of barcode type names
     */
    static std::vector<std::string> getBarcodeTypeNames();
    
    /**
     * @brief Get the last error
     * @return ErrorInfo with error details
     */
    ErrorInfo getLastError() const { return lastError_; }
    
private:
    ErrorInfo lastError_;
    BarcodeConfig currentConfig_;
    
    void setError(ErrorCode code, const std::string& message, const std::string& details = "");
};

} // namespace creo_barcode

#endif // SETTINGS_DIALOG_H
