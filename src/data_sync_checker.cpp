/**
 * @file data_sync_checker.cpp
 * @brief Implementation of data synchronization checker
 * 
 * Requirements: 3.1, 3.2, 3.3
 */

#include "data_sync_checker.h"
#include "logger.h"
#include <algorithm>

namespace creo_barcode {

DataSyncChecker::DataSyncChecker() 
    : defaultUpdateCallback_(nullptr)
    , defaultWarningCallback_(nullptr) {
}

void DataSyncChecker::setError(ErrorCode code, const std::string& message, const std::string& details) {
    lastError_ = ErrorInfo(code, message, details);
}

SyncCheckResult DataSyncChecker::checkSync(const std::string& currentPartName, 
                                           const BarcodeInstance& barcodeInstance) {
    SyncCheckResult result;
    result.currentPartName = currentPartName;
    result.barcodeData = barcodeInstance.decodedData;
    
    // Check if barcode data is empty
    if (barcodeInstance.decodedData.empty()) {
        result.status = SyncStatus::BARCODE_NOT_FOUND;
        result.message = getStatusMessage(SyncStatus::BARCODE_NOT_FOUND);
        LOG_WARNING("Barcode data is empty for sync check");
        return result;
    }
    
    // Check if part name is empty
    if (currentPartName.empty()) {
        result.status = SyncStatus::UNKNOWN;
        result.message = "Part name is empty";
        LOG_WARNING("Part name is empty for sync check");
        return result;
    }
    
    // Compare the data
    // Note: We compare decoded barcode data with the part name
    // The barcode may have been encoded with special character handling
    if (barcodeInstance.decodedData == currentPartName) {
        result.status = SyncStatus::IN_SYNC;
        result.message = getStatusMessage(SyncStatus::IN_SYNC);
        LOG_INFO("Barcode is in sync with part name: " + currentPartName);
    } else {
        result.status = SyncStatus::OUT_OF_SYNC;
        result.message = getStatusMessage(SyncStatus::OUT_OF_SYNC);
        result.warningDisplayed = true;
        LOG_WARNING("Barcode out of sync - Part: '" + currentPartName + 
                   "', Barcode: '" + barcodeInstance.decodedData + "'");
    }
    
    return result;
}

SyncCheckResult DataSyncChecker::checkSyncFromImage(const std::string& currentPartName,
                                                    const std::string& barcodePath,
                                                    BarcodeGenerator& generator) {
    SyncCheckResult result;
    result.currentPartName = currentPartName;
    
    // Try to decode the barcode from image
    auto decodedData = generator.decode(barcodePath);
    
    if (!decodedData.has_value()) {
        result.status = SyncStatus::DECODE_ERROR;
        result.message = "Failed to decode barcode from image";
        setError(ErrorCode::DECODE_FAILED, result.message, barcodePath);
        LOG_ERROR("Failed to decode barcode from: " + barcodePath);
        return result;
    }
    
    result.barcodeData = decodedData.value();
    
    // Decode special characters if needed
    std::string decodedPartName = generator.decodeSpecialChars(result.barcodeData);
    
    // Compare with current part name
    if (decodedPartName == currentPartName) {
        result.status = SyncStatus::IN_SYNC;
        result.message = getStatusMessage(SyncStatus::IN_SYNC);
        LOG_INFO("Barcode from image is in sync with part name: " + currentPartName);
    } else {
        result.status = SyncStatus::OUT_OF_SYNC;
        result.message = getStatusMessage(SyncStatus::OUT_OF_SYNC);
        result.warningDisplayed = true;
        LOG_WARNING("Barcode from image out of sync - Part: '" + currentPartName + 
                   "', Barcode: '" + decodedPartName + "'");
    }
    
    return result;
}

bool DataSyncChecker::promptUpdate(const SyncCheckResult& result, 
                                   UpdateConfirmCallback confirmCallback) {
    // Only prompt if out of sync
    if (result.status != SyncStatus::OUT_OF_SYNC) {
        LOG_INFO("No update needed - barcode is in sync");
        return false;
    }
    
    // Use provided callback or default
    UpdateConfirmCallback callback = confirmCallback ? confirmCallback : defaultUpdateCallback_;
    
    if (!callback) {
        LOG_WARNING("No update confirmation callback provided");
        return false;
    }
    
    // Requirement 3.1: Provide update option when part name changes
    LOG_INFO("Prompting user to update barcode from '" + result.barcodeData + 
             "' to '" + result.currentPartName + "'");
    
    bool userConfirmed = callback(result.barcodeData, result.currentPartName);
    
    if (userConfirmed) {
        LOG_INFO("User confirmed barcode update");
    } else {
        LOG_INFO("User declined barcode update");
    }
    
    return userConfirmed;
}

void DataSyncChecker::displayWarning(const BarcodeInstance& instance,
                                     WarningDisplayCallback displayCallback) {
    // Use provided callback or default
    WarningDisplayCallback callback = displayCallback ? displayCallback : defaultWarningCallback_;
    
    if (!callback) {
        LOG_WARNING("No warning display callback provided");
        return;
    }
    
    // Requirement 3.3: Display warning indicator when data is inconsistent
    std::string warningMessage = "Barcode data does not match current part name. "
                                 "Barcode contains: '" + instance.decodedData + "'";
    
    LOG_WARNING("Displaying sync warning: " + warningMessage);
    callback(warningMessage, instance);
}

bool DataSyncChecker::compareData(const std::string& partName,
                                  const std::string& barcodeData,
                                  BarcodeGenerator& generator) {
    if (partName.empty() || barcodeData.empty()) {
        return false;
    }
    
    // Direct comparison
    if (partName == barcodeData) {
        return true;
    }
    
    // Try comparing with encoded version
    std::string encodedPartName = generator.encodeSpecialChars(partName);
    if (encodedPartName == barcodeData) {
        return true;
    }
    
    // Try comparing with decoded version
    std::string decodedBarcodeData = generator.decodeSpecialChars(barcodeData);
    if (partName == decodedBarcodeData) {
        return true;
    }
    
    return false;
}

std::string DataSyncChecker::getStatusMessage(SyncStatus status) {
    switch (status) {
        case SyncStatus::IN_SYNC:
            return "Barcode data matches current part name";
        case SyncStatus::OUT_OF_SYNC:
            return "Barcode data does not match current part name - update recommended";
        case SyncStatus::BARCODE_NOT_FOUND:
            return "No barcode found in drawing";
        case SyncStatus::DECODE_ERROR:
            return "Could not decode barcode data";
        case SyncStatus::UNKNOWN:
        default:
            return "Synchronization status unknown";
    }
}

void DataSyncChecker::setUpdateConfirmCallback(UpdateConfirmCallback callback) {
    defaultUpdateCallback_ = callback;
}

void DataSyncChecker::setWarningDisplayCallback(WarningDisplayCallback callback) {
    defaultWarningCallback_ = callback;
}

std::string syncStatusToString(SyncStatus status) {
    switch (status) {
        case SyncStatus::IN_SYNC: return "IN_SYNC";
        case SyncStatus::OUT_OF_SYNC: return "OUT_OF_SYNC";
        case SyncStatus::BARCODE_NOT_FOUND: return "BARCODE_NOT_FOUND";
        case SyncStatus::DECODE_ERROR: return "DECODE_ERROR";
        case SyncStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

} // namespace creo_barcode
