/**
 * @file barcode_c_api.cpp
 * @brief C API wrapper implementation for barcode functionality
 */

#include "barcode_c_api.h"
#include "barcode_generator.h"
#include "config_manager.h"
#include "data_sync_checker.h"
#include "creo_com_bridge.h"
#include "logger.h"
#include <string>
#include <memory>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace creo_barcode;

// Global instances
static std::unique_ptr<BarcodeGenerator> g_generator;
static std::unique_ptr<ConfigManager> g_configManager;
static std::unique_ptr<DataSyncChecker> g_syncChecker;
static std::string g_lastError;

// Convert C enum to C++ enum
static BarcodeType toCppType(BarcodeTypeC type) {
    switch (type) {
        case BARCODE_CODE_128: return BarcodeType::CODE_128;
        case BARCODE_CODE_39: return BarcodeType::CODE_39;
        case BARCODE_QR_CODE: return BarcodeType::QR_CODE;
        case BARCODE_DATA_MATRIX: return BarcodeType::DATA_MATRIX;
        case BARCODE_EAN_13: return BarcodeType::EAN_13;
        default: return BarcodeType::CODE_128;
    }
}

extern "C" {

int barcode_init(void) {
    try {
        // Use explicit new instead of make_unique to avoid potential
        // static initialization issues in DLL context
        if (!g_generator) {
            g_generator.reset(new BarcodeGenerator());
        }
        if (!g_configManager) {
            g_configManager.reset(new ConfigManager());
        }
        if (!g_syncChecker) {
            g_syncChecker.reset(new DataSyncChecker());
        }
        g_lastError.clear();
        return 0;
    } catch (const std::exception& e) {
        g_lastError = std::string("barcode_init failed: ") + e.what();
        return -1;
    } catch (...) {
        g_lastError = "barcode_init failed: unknown exception";
        return -1;
    }
}

void barcode_cleanup(void) {
    g_generator.reset();
    g_configManager.reset();
    g_syncChecker.reset();
}

int barcode_generate(const char* data, const BarcodeConfigC* config, const char* outputPath) {
    // Validate input parameters first (before any C++ operations)
    if (!data || !config || !outputPath) {
        g_lastError = "Invalid parameters: null pointer";
        return -1;
    }
    
    if (data[0] == '\0') {
        g_lastError = "Invalid parameters: empty data string";
        return -1;
    }
    
    if (outputPath[0] == '\0') {
        g_lastError = "Invalid parameters: empty output path";
        return -1;
    }
    
    // Check if module is initialized
    if (!g_generator) {
        // Try to initialize on demand
        try {
            g_generator.reset(new BarcodeGenerator());
        } catch (const std::exception& e) {
            g_lastError = std::string("Failed to initialize generator: ") + e.what();
            return -1;
        } catch (...) {
            g_lastError = "Failed to initialize generator: unknown error";
            return -1;
        }
    }
    
    try {
        BarcodeConfig cppConfig;
        cppConfig.type = toCppType(config->type);
        cppConfig.width = config->width;
        cppConfig.height = config->height;
        cppConfig.margin = config->margin;
        cppConfig.showText = config->showText != 0;
        cppConfig.dpi = config->dpi;
        
        // Use std::string with explicit construction to avoid issues
        std::string dataStr(data);
        std::string pathStr(outputPath);
        
        if (g_generator->generate(dataStr, cppConfig, pathStr)) {
            return 0;
        } else {
            g_lastError = g_generator->getLastError().message;
            return -1;
        }
    } catch (const std::exception& e) {
        g_lastError = std::string("Exception in barcode_generate: ") + e.what();
        return -1;
    } catch (...) {
        g_lastError = "Unknown exception in barcode_generate";
        return -1;
    }
}

int barcode_decode(const char* imagePath, char* outputBuffer, int bufferSize) {
    if (!g_generator || !imagePath || !outputBuffer || bufferSize <= 0) {
        g_lastError = "Invalid parameters or module not initialized";
        return -1;
    }
    
    try {
        auto result = g_generator->decode(imagePath);
        if (result.has_value()) {
            strncpy(outputBuffer, result->c_str(), bufferSize - 1);
            outputBuffer[bufferSize - 1] = '\0';
            return 0;
        } else {
            g_lastError = g_generator->getLastError().message;
            return -1;
        }
    } catch (const std::exception& e) {
        g_lastError = e.what();
        return -1;
    }
}

int config_load(const char* configPath) {
    if (!g_configManager || !configPath) {
        g_lastError = "Invalid parameters or module not initialized";
        return -1;
    }
    
    try {
        if (g_configManager->loadConfig(configPath)) {
            return 0;
        } else {
            g_lastError = g_configManager->getLastError().message;
            return -1;
        }
    } catch (const std::exception& e) {
        g_lastError = e.what();
        return -1;
    }
}

int config_save(const char* configPath) {
    if (!g_configManager || !configPath) {
        g_lastError = "Invalid parameters or module not initialized";
        return -1;
    }
    
    try {
        if (g_configManager->saveConfig(configPath)) {
            return 0;
        } else {
            g_lastError = g_configManager->getLastError().message;
            return -1;
        }
    } catch (const std::exception& e) {
        g_lastError = e.what();
        return -1;
    }
}

void config_get_defaults(BarcodeConfigC* config) {
    if (!config) return;
    
    config->type = BARCODE_CODE_128;
    config->width = 200;
    config->height = 80;
    config->margin = 10;
    config->showText = 1;
    config->dpi = 300;
}

int sync_check(const char* partName, const char* barcodeData) {
    if (!g_syncChecker || !g_generator || !partName || !barcodeData) {
        g_lastError = "Invalid parameters or module not initialized";
        return -1;
    }
    
    try {
        if (g_syncChecker->compareData(partName, barcodeData, *g_generator)) {
            return 1;  // In sync
        } else {
            return 0;  // Out of sync
        }
    } catch (const std::exception& e) {
        g_lastError = e.what();
        return -1;
    }
}

const char* barcode_get_last_error(void) {
    return g_lastError.c_str();
}

// Global current config
static BarcodeConfigC g_currentConfig = {
    BARCODE_CODE_128,  // type
    200,               // width
    80,                // height
    10,                // margin
    1,                 // showText
    300                // dpi
};

void config_get_current(BarcodeConfigC* config) {
    if (config) {
        *config = g_currentConfig;
    }
}

void config_set_current(const BarcodeConfigC* config) {
    if (config) {
        g_currentConfig = *config;
    }
}

const char* barcode_type_name(BarcodeTypeC type) {
    switch (type) {
        case BARCODE_CODE_128: return "Code 128";
        case BARCODE_CODE_39: return "Code 39";
        case BARCODE_QR_CODE: return "QR Code";
        case BARCODE_DATA_MATRIX: return "Data Matrix";
        case BARCODE_EAN_13: return "EAN-13";
        default: return "Unknown";
    }
}

int barcode_get_image_size(const char* imagePath, int* width, int* height) {
    if (!g_generator || !imagePath || !width || !height) {
        g_lastError = "Invalid parameters or module not initialized";
        return -1;
    }
    
    try {
        if (g_generator->getImageSize(imagePath, *width, *height)) {
            return 0;
        } else {
            g_lastError = "Failed to get image size";
            return -1;
        }
    } catch (const std::exception& e) {
        g_lastError = e.what();
        return -1;
    }
}

int barcode_generate_for_part(const char* partName, 
                              const BarcodeConfigC* config,
                              const char* outputDir,
                              char* outputPath,
                              int pathSize) {
    if (!g_generator || !partName || !config || !outputDir || !outputPath || pathSize <= 0) {
        g_lastError = "Invalid parameters or module not initialized";
        return -1;
    }
    
    try {
        // Build output path
        std::string outPath = std::string(outputDir) + "\\barcode_" + partName + ".png";
        
        BarcodeConfig cppConfig;
        cppConfig.type = toCppType(config->type);
        cppConfig.width = config->width;
        cppConfig.height = config->height;
        cppConfig.margin = config->margin;
        cppConfig.showText = config->showText != 0;
        cppConfig.dpi = config->dpi;
        
        if (g_generator->generate(partName, cppConfig, outPath)) {
            strncpy(outputPath, outPath.c_str(), pathSize - 1);
            outputPath[pathSize - 1] = '\0';
            return 0;
        } else {
            g_lastError = g_generator->getLastError().message;
            return -1;
        }
    } catch (const std::exception& e) {
        g_lastError = e.what();
        return -1;
    }
}

// ============================================================================
// COM Bridge C API Wrapper Functions
// ============================================================================

// Static storage for COM bridge error message
static std::string g_comBridgeLastError;

int com_bridge_init(void) {
    try {
        // Delay COM initialization - don't initialize COM bridge during plugin load
        // This avoids potential conflicts with Creo's own COM initialization
        // The bridge will be initialized on first use
        g_comBridgeLastError.clear();
        return 0;  // Return success but don't actually initialize yet
    } catch (const std::exception& e) {
        g_comBridgeLastError = std::string("com_bridge_init failed: ") + e.what();
        return -1;
    } catch (...) {
        g_comBridgeLastError = "com_bridge_init failed: unknown exception";
        return -1;
    }
}

void com_bridge_cleanup(void) {
    try {
        // Only cleanup if COM bridge was actually used
        // Check if the singleton was ever accessed
        // Note: We don't call getInstance() here to avoid creating it just to destroy it
    } catch (...) {
        // Ignore exceptions during cleanup
    }
}

int com_bridge_is_initialized(void) {
    // COM bridge is disabled for now - always return 0 to use fallback
    // This avoids COM-related crashes during plugin operation
    return 0;
}

int barcode_insert_image(const char* imagePath,
                         double x, double y,
                         double width, double height) {
    if (!imagePath) {
        g_comBridgeLastError = "Image path is null";
        return -1;
    }
    
    try {
        if (CreoComBridge::getInstance().insertImage(imagePath, x, y, width, height)) {
            g_comBridgeLastError.clear();
            return 0;
        } else {
            g_comBridgeLastError = CreoComBridge::getInstance().getLastError();
            return -1;
        }
    } catch (const std::exception& e) {
        g_comBridgeLastError = e.what();
        return -1;
    }
}

int barcode_batch_insert_images(const char** imagePaths,
                                const double* positions,
                                int count,
                                double width, double height,
                                BatchInsertResultC* result) {
    // Initialize result if provided
    if (result) {
        result->totalCount = count;
        result->successCount = 0;
        result->failCount = 0;
    }
    
    if (!imagePaths || !positions || count <= 0) {
        g_comBridgeLastError = "Invalid parameters for batch insert";
        return 0;
    }
    
    try {
        // Build vector of BatchImageInfo
        std::vector<BatchImageInfo> images;
        images.reserve(count);
        
        for (int i = 0; i < count; i++) {
            if (!imagePaths[i]) {
                continue;  // Skip null paths
            }
            
            BatchImageInfo info;
            info.imagePath = imagePaths[i];
            info.x = positions[i * 2];      // x is at even indices
            info.y = positions[i * 2 + 1];  // y is at odd indices
            info.width = width;
            info.height = height;
            images.push_back(info);
        }
        
        // Call COM bridge batch insert
        BatchInsertResult batchResult = CreoComBridge::getInstance().batchInsertImages(images);
        
        // Copy results
        if (result) {
            result->totalCount = batchResult.totalCount;
            result->successCount = batchResult.successCount;
            result->failCount = batchResult.failCount;
        }
        
        // Store last error if any failures
        if (batchResult.failCount > 0 && !batchResult.errorMessages.empty()) {
            g_comBridgeLastError = batchResult.errorMessages.back();
        } else {
            g_comBridgeLastError.clear();
        }
        
        return batchResult.successCount;
        
    } catch (const std::exception& e) {
        g_comBridgeLastError = e.what();
        return 0;
    }
}

int barcode_batch_insert_images_grid(const char** imagePaths,
                                     int count,
                                     double startX, double startY,
                                     double width, double height,
                                     int columns, double spacing,
                                     BatchInsertResultC* result) {
    // Initialize result if provided
    if (result) {
        result->totalCount = count;
        result->successCount = 0;
        result->failCount = 0;
    }
    
    if (!imagePaths || count <= 0) {
        g_comBridgeLastError = "Invalid parameters for grid batch insert";
        return 0;
    }
    
    try {
        // Build vector of image paths
        std::vector<std::string> paths;
        paths.reserve(count);
        
        for (int i = 0; i < count; i++) {
            if (imagePaths[i]) {
                paths.push_back(imagePaths[i]);
            }
        }
        
        // Set up grid layout parameters
        GridLayoutParams params;
        params.startX = startX;
        params.startY = startY;
        params.width = width;
        params.height = height;
        params.columns = (columns > 0) ? columns : 1;
        params.spacing = spacing;
        
        // Call COM bridge grid batch insert
        BatchInsertResult batchResult = CreoComBridge::getInstance().batchInsertImagesGrid(paths, params);
        
        // Copy results
        if (result) {
            result->totalCount = batchResult.totalCount;
            result->successCount = batchResult.successCount;
            result->failCount = batchResult.failCount;
        }
        
        // Store last error if any failures
        if (batchResult.failCount > 0 && !batchResult.errorMessages.empty()) {
            g_comBridgeLastError = batchResult.errorMessages.back();
        } else {
            g_comBridgeLastError.clear();
        }
        
        return batchResult.successCount;
        
    } catch (const std::exception& e) {
        g_comBridgeLastError = e.what();
        return 0;
    }
}

const char* com_bridge_get_last_error(void) {
    return g_comBridgeLastError.c_str();
}

int barcode_insert_image_with_fallback(const char* imagePath,
                                       double x, double y,
                                       double width, double height,
                                       const char* partName,
                                       int* usedFallback) {
    // Initialize output parameter
    if (usedFallback) {
        *usedFallback = 0;
    }
    
    if (!imagePath) {
        g_comBridgeLastError = "Image path is null";
        return -1;  // FALLBACK_FAILED
    }
    
    // Try COM bridge first if initialized
    if (CreoComBridge::getInstance().isInitialized()) {
        LOG_INFO("Attempting COM image insertion for: " + std::string(imagePath));
        
        if (CreoComBridge::getInstance().insertImage(imagePath, x, y, width, height)) {
            LOG_INFO("COM image insertion succeeded");
            g_comBridgeLastError.clear();
            return 0;  // FALLBACK_SUCCESS_COM
        }
        
        // COM insertion failed, log the error
        std::string comError = CreoComBridge::getInstance().getLastError();
        LOG_WARNING("COM image insertion failed: " + comError + " - falling back to note-based approach");
        g_comBridgeLastError = comError;
    } else {
        LOG_WARNING("COM bridge not initialized - using note-based fallback");
        g_comBridgeLastError = "COM bridge not initialized";
    }
    
    // Set fallback flag
    if (usedFallback) {
        *usedFallback = 1;
    }
    
    // Fallback: Return success code indicating fallback should be used
    // The actual note insertion is done by the caller (main_pure_c.c)
    // because it requires Pro/TOOLKIT API calls
    LOG_INFO("Fallback to note-based approach for: " + std::string(imagePath));
    return 1;  // FALLBACK_SUCCESS_NOTE (caller should insert note)
}

int com_bridge_format_hresult(long hr, char* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0) {
        return -1;
    }
    
#ifdef _WIN32
    // Format HRESULT as hex
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(8) 
        << static_cast<unsigned long>(hr);
    
    // Try to get system error message
    LPWSTR msgBuffer = nullptr;
    DWORD msgLen = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<HRESULT>(hr),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msgBuffer),
        0,
        nullptr
    );
    
    if (msgLen > 0 && msgBuffer != nullptr) {
        std::wstring wMsg(msgBuffer, msgLen);
        // Remove trailing newlines
        while (!wMsg.empty() && (wMsg.back() == L'\n' || wMsg.back() == L'\r')) {
            wMsg.pop_back();
        }
        std::string utf8Msg = StringUtils::wstringToUtf8(wMsg);
        oss << " (" << utf8Msg << ")";
        LocalFree(msgBuffer);
    }
    
    std::string result = oss.str();
    strncpy(buffer, result.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return 0;
#else
    snprintf(buffer, bufferSize, "0x%08lx (HRESULT not supported on this platform)", hr);
    return 0;
#endif
}

} // extern "C"
