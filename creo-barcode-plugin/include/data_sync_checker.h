/**
 * @file data_sync_checker.h
 * @brief Data synchronization checker for barcode and part name consistency
 * 
 * This module implements the data synchronization check functionality:
 * - Check if barcode data matches current part name
 * - Provide update options when data is inconsistent
 * - Display warning indicators for mismatched data
 * 
 * Requirements: 3.1, 3.2, 3.3
 */

#ifndef DATA_SYNC_CHECKER_H
#define DATA_SYNC_CHECKER_H

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include "error_codes.h"
#include "barcode_generator.h"

namespace creo_barcode {

/**
 * @brief Synchronization status enumeration
 */
enum class SyncStatus {
    IN_SYNC,           // Barcode data matches part name
    OUT_OF_SYNC,       // Barcode data does not match part name
    BARCODE_NOT_FOUND, // No barcode found in drawing
    DECODE_ERROR,      // Could not decode barcode
    UNKNOWN            // Status could not be determined
};

/**
 * @brief Information about a barcode instance in a drawing
 */
struct BarcodeInstance {
    std::string imagePath;           // Path to barcode image file
    std::string encodedData;         // Data encoded in the barcode
    std::string decodedData;         // Decoded data from barcode
    BarcodeType type;                // Type of barcode
    double posX = 0.0;               // X position in drawing
    double posY = 0.0;               // Y position in drawing
    std::string timestamp;           // When barcode was created
    
    BarcodeInstance() : type(BarcodeType::CODE_128) {}
};

/**
 * @brief Result of a synchronization check
 */
struct SyncCheckResult {
    SyncStatus status;               // Overall sync status
    std::string currentPartName;     // Current part name from model
    std::string barcodeData;         // Data stored in barcode
    std::string message;             // Human-readable status message
    bool warningDisplayed = false;   // Whether warning indicator is shown
    
    SyncCheckResult() : status(SyncStatus::UNKNOWN), warningDisplayed(false) {}
    
    bool isInSync() const { return status == SyncStatus::IN_SYNC; }
    bool needsUpdate() const { return status == SyncStatus::OUT_OF_SYNC; }
};

/**
 * @brief Callback type for update confirmation
 * @param oldData The old barcode data
 * @param newData The new part name to encode
 * @return true if user confirms update, false otherwise
 */
using UpdateConfirmCallback = std::function<bool(const std::string& oldData, const std::string& newData)>;

/**
 * @brief Callback type for warning display
 * @param message Warning message to display
 * @param barcodeInstance The barcode instance with the issue
 */
using WarningDisplayCallback = std::function<void(const std::string& message, const BarcodeInstance& instance)>;

/**
 * @brief Data synchronization checker class
 * 
 * This class provides functionality to check and maintain consistency
 * between barcode data and part names in Creo drawings.
 */
class DataSyncChecker {
public:
    DataSyncChecker();
    ~DataSyncChecker() = default;
    
    /**
     * @brief Check if barcode data matches current part name
     * 
     * Requirement 3.3: WHEN 条形码数据与零件名称不一致 THEN 系统 SHALL 在条形码旁显示警告标识
     * 
     * @param currentPartName Current part name from the model
     * @param barcodeInstance Barcode instance to check
     * @return SyncCheckResult containing sync status and details
     */
    SyncCheckResult checkSync(const std::string& currentPartName, 
                              const BarcodeInstance& barcodeInstance);
    
    /**
     * @brief Check synchronization by decoding barcode from image
     * 
     * @param currentPartName Current part name from the model
     * @param barcodePath Path to barcode image file
     * @param generator Barcode generator for decoding
     * @return SyncCheckResult containing sync status and details
     */
    SyncCheckResult checkSyncFromImage(const std::string& currentPartName,
                                       const std::string& barcodePath,
                                       BarcodeGenerator& generator);
    
    /**
     * @brief Prompt user to update barcode when out of sync
     * 
     * Requirement 3.1: WHEN 工程图中的零件名称发生变更 THEN 系统 SHALL 提供更新条形码的选项
     * 
     * @param result The sync check result
     * @param confirmCallback Callback to confirm update with user
     * @return true if user chose to update, false otherwise
     */
    bool promptUpdate(const SyncCheckResult& result, 
                      UpdateConfirmCallback confirmCallback);
    
    /**
     * @brief Display warning indicator for out-of-sync barcode
     * 
     * Requirement 3.3: Display warning indicator when data is inconsistent
     * 
     * @param instance The barcode instance with the issue
     * @param displayCallback Callback to display warning
     */
    void displayWarning(const BarcodeInstance& instance,
                        WarningDisplayCallback displayCallback);
    
    /**
     * @brief Compare two strings for equality (with encoding consideration)
     * 
     * @param partName Original part name
     * @param barcodeData Data from barcode (may be encoded)
     * @param generator Barcode generator for encoding/decoding
     * @return true if data matches, false otherwise
     */
    bool compareData(const std::string& partName,
                     const std::string& barcodeData,
                     BarcodeGenerator& generator);
    
    /**
     * @brief Get human-readable status message
     * 
     * @param status Sync status
     * @return Status message string
     */
    static std::string getStatusMessage(SyncStatus status);
    
    /**
     * @brief Get last error information
     * @return ErrorInfo structure with error details
     */
    ErrorInfo getLastError() const { return lastError_; }
    
    /**
     * @brief Set default update confirmation callback
     * @param callback The callback to use when prompting for updates
     */
    void setUpdateConfirmCallback(UpdateConfirmCallback callback);
    
    /**
     * @brief Set default warning display callback
     * @param callback The callback to use when displaying warnings
     */
    void setWarningDisplayCallback(WarningDisplayCallback callback);
    
private:
    ErrorInfo lastError_;
    UpdateConfirmCallback defaultUpdateCallback_;
    WarningDisplayCallback defaultWarningCallback_;
    
    void setError(ErrorCode code, const std::string& message, const std::string& details = "");
};

/**
 * @brief Convert SyncStatus to string
 * @param status The sync status
 * @return String representation
 */
std::string syncStatusToString(SyncStatus status);

} // namespace creo_barcode

#endif // DATA_SYNC_CHECKER_H
