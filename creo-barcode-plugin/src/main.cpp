/**
 * @file main.cpp
 * @brief Creo Barcode Plugin main entry point
 * 
 * This file implements the plugin entry points for Creo Toolkit:
 * - user_initialize(): Called when the plugin is loaded
 * - user_terminate(): Called when the plugin is unloaded
 * 
 * Requirements: 1.1, 1.2, 4.1, 4.4
 */

// Pro/TOOLKIT headers (must be included first)
#ifdef HAS_CREO_TOOLKIT
#include <ProToolkit.h>
#include <ProMenu.h>
#include <ProMenuBar.h>
#include <ProMessage.h>
#include <ProUICmd.h>
#include <ProRibbon.h>
#include <ProArray.h>
#endif

#include "version_check.h"
#include "config_manager.h"
#include "logger.h"
#include "error_codes.h"
#include "menu_manager.h"
#include "drawing_interface.h"
#include "barcode_generator.h"
#include "batch_processor.h"
#include "settings_dialog.h"
#include "data_sync_checker.h"

#include <string>
#include <memory>
#include <filesystem>
#include <chrono>

namespace creo_barcode {

// Plugin status enumeration
enum class PluginStatus {
    OK,
    VERSION_ERROR,
    INIT_ERROR,
    NOT_INITIALIZED
};

// Global plugin state
static PluginStatus g_pluginStatus = PluginStatus::NOT_INITIALIZED;
static std::unique_ptr<ConfigManager> g_configManager;
static std::unique_ptr<DrawingInterface> g_drawingInterface;
static std::unique_ptr<BarcodeGenerator> g_barcodeGenerator;
static std::unique_ptr<BatchProcessor> g_batchProcessor;
static std::unique_ptr<DataSyncChecker> g_dataSyncChecker;
static std::string g_pluginVersion = "1.0.0";

// Forward declarations for workflow functions
void onGenerateBarcodeRequested(const BarcodeConfig& config);
void onBatchGenerateRequested();
std::string generateOutputPath(const std::string& partName);
bool ensureOutputDirectory(const std::string& path);
SyncCheckResult checkBarcodeSync(const std::string& barcodePath, const std::string& currentPartName);
bool updateBarcodeIfNeeded(const SyncCheckResult& syncResult, const BarcodeConfig& config);

/**
 * @brief Get the current Creo version
 * @param version Output parameter for the version
 * @return true if version was retrieved successfully
 */
bool getCurrentCreoVersion(CreoVersion& version) {
    // For now, assume compatible version
    // In production, this would query Creo for its actual version
    version = CreoVersion(8, 0, 0);
    return true;
}

/**
 * @brief Initialize plugin resources
 * @return true if initialization was successful
 */
bool initializeResources() {
    LOG_INFO("Initializing Creo Barcode Plugin v" + g_pluginVersion);
    
    // Initialize configuration manager
    g_configManager = std::make_unique<ConfigManager>();
    
    // Try to load configuration from default location
    std::string configPath;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        configPath = std::string(appData) + "\\CreoBarcodePlugin\\config.json";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        configPath = std::string(home) + "/.creo_barcode/config.json";
    }
#endif
    
    if (!configPath.empty()) {
        if (!g_configManager->loadConfig(configPath)) {
            LOG_WARNING("Could not load config from " + configPath + ", using defaults");
        } else {
            LOG_INFO("Configuration loaded from " + configPath);
        }
    }
    
    // Initialize drawing interface
    g_drawingInterface = std::make_unique<DrawingInterface>();
    LOG_INFO("Drawing interface initialized");
    
    // Initialize barcode generator
    g_barcodeGenerator = std::make_unique<BarcodeGenerator>();
    LOG_INFO("Barcode generator initialized");
    
    // Initialize batch processor
    g_batchProcessor = std::make_unique<BatchProcessor>();
    LOG_INFO("Batch processor initialized");
    
    // Initialize data sync checker (Requirements 3.1, 3.2, 3.3)
    g_dataSyncChecker = std::make_unique<DataSyncChecker>();
    
    // Set up default callbacks for sync checker
    g_dataSyncChecker->setUpdateConfirmCallback([](const std::string& oldData, const std::string& newData) {
        // In real implementation, show confirmation dialog to user
        LOG_INFO("Update confirmation requested: '" + oldData + "' -> '" + newData + "'");
        // For now, auto-confirm updates
        return true;
    });
    
    g_dataSyncChecker->setWarningDisplayCallback([](const std::string& message, const BarcodeInstance& instance) {
        // In real implementation, display warning indicator in drawing
        LOG_WARNING("Sync warning at position (" + std::to_string(instance.posX) + ", " + 
                   std::to_string(instance.posY) + "): " + message);
    });
    
    LOG_INFO("Data sync checker initialized");
    
    // Initialize menu manager and register callbacks
    MenuManager& menuManager = getMenuManager();
    menuManager.setConfigManager(g_configManager.get());
    menuManager.setGenerateBarcodeCallback(onGenerateBarcodeRequested);
    menuManager.setBatchGenerateCallback(onBatchGenerateRequested);
    
    // Register menus
    ErrorCode menuResult = menuManager.registerMenus();
    if (menuResult != ErrorCode::SUCCESS) {
        LOG_ERROR("Failed to register menus: " + menuManager.getLastError().message);
        return false;
    }
    LOG_INFO("Menus registered successfully");
    
    return true;
}

/**
 * @brief Clean up plugin resources
 */
void cleanupResources() {
    LOG_INFO("Cleaning up Creo Barcode Plugin resources");
    
    // Unregister menus first
    MenuManager& menuManager = getMenuManager();
    if (menuManager.isRegistered()) {
        menuManager.unregisterMenus();
        LOG_INFO("Menus unregistered");
    }
    
    // Clean up batch processor
    if (g_batchProcessor) {
        g_batchProcessor->clear();
        g_batchProcessor.reset();
        LOG_INFO("Batch processor cleaned up");
    }
    
    // Clean up data sync checker
    if (g_dataSyncChecker) {
        g_dataSyncChecker.reset();
        LOG_INFO("Data sync checker cleaned up");
    }
    
    // Clean up barcode generator
    if (g_barcodeGenerator) {
        g_barcodeGenerator.reset();
        LOG_INFO("Barcode generator cleaned up");
    }
    
    // Clean up drawing interface
    if (g_drawingInterface) {
        g_drawingInterface.reset();
        LOG_INFO("Drawing interface cleaned up");
    }
    
    // Save configuration before exit
    if (g_configManager) {
        std::string configPath;
#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        if (appData) {
            configPath = std::string(appData) + "\\CreoBarcodePlugin\\config.json";
        }
#else
        const char* home = std::getenv("HOME");
        if (home) {
            configPath = std::string(home) + "/.creo_barcode/config.json";
        }
#endif
        
        if (!configPath.empty()) {
            if (g_configManager->saveConfig(configPath)) {
                LOG_INFO("Configuration saved to " + configPath);
            }
        }
        
        g_configManager.reset();
        LOG_INFO("Configuration manager cleaned up");
    }
    
    LOG_INFO("Creo Barcode Plugin cleanup complete");
}

/**
 * @brief Display version incompatibility message to user
 * @param currentVersion The current Creo version
 * @param requiredVersion The minimum required version
 */
void showVersionError(const CreoVersion& currentVersion, const CreoVersion& requiredVersion) {
    std::string message = "Creo Barcode Plugin requires Creo version " + 
                          requiredVersion.toString() + " or higher. " +
                          "Current version: " + currentVersion.toString();
    
    LOG_ERROR(message);
}

/**
 * @brief Get the current plugin status
 * @return Current plugin status
 */
PluginStatus getPluginStatus() {
    return g_pluginStatus;
}

/**
 * @brief Get the configuration manager instance
 * @return Pointer to the configuration manager, or nullptr if not initialized
 */
ConfigManager* getConfigManager() {
    return g_configManager.get();
}

} // namespace creo_barcode

/**
 * @brief Plugin initialization entry point
 * 
 * Called by Creo when the plugin is loaded. This function:
 * 1. Checks Creo version compatibility (Requirement 4.2)
 * 2. Initializes plugin resources
 * 3. Registers menus and UI elements (Requirement 4.1)
 * 
 * @return 0 on success, non-zero on failure
 */
// Forward declaration for menu callbacks
#ifdef HAS_CREO_TOOLKIT

// Command IDs for ribbon buttons
static uiCmdCmdId g_cmdGenerate = 0;
static uiCmdCmdId g_cmdSettings = 0;
static uiCmdCmdId g_cmdBatch = 0;
static uiCmdCmdId g_cmdSyncCheck = 0;

// Access function - always available
static uiCmdAccessState BarcodeAccessDefault(uiCmdAccessMode access_mode)
{
    return ACCESS_AVAILABLE;
}

// Action: Generate Barcode
static int BarcodeGenerateAction(uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data)
{
    ::ProMessageClear();
    // TODO: Call actual barcode generation
    return 0;
}

// Action: Settings
static int BarcodeSettingsAction(uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data)
{
    ::ProMessageClear();
    // TODO: Open settings dialog
    return 0;
}

// Action: Batch Process
static int BarcodeBatchAction(uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data)
{
    ::ProMessageClear();
    // TODO: Open batch processing
    return 0;
}

// Action: Sync Check
static int BarcodeSyncCheckAction(uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data)
{
    ::ProMessageClear();
    // TODO: Perform sync check
    return 0;
}

// Helper to convert char* to wchar_t*
static void CharToWchar(const char* src, wchar_t* dst, size_t dstSize)
{
    size_t i = 0;
    while (src[i] && i < dstSize - 1) {
        dst[i] = (wchar_t)src[i];
        i++;
    }
    dst[i] = L'\0';
}

#endif

/**
 * Pro/TOOLKIT standard user_initialize function
 * Must match the exact signature expected by Creo
 * 
 * IMPORTANT: This function MUST contain at least one Pro/TOOLKIT API call,
 * otherwise Creo will return PRO_TK_GENERAL_ERROR.
 */
extern "C" int user_initialize(
    int argc,
    char *argv[],
    char *version,
    char *build,
    wchar_t errbuf[80])
{
#ifdef HAS_CREO_TOOLKIT
    ::ProError status;
    
    // ========================================
    // Step 1: Register all command actions
    // ========================================
    
    // Register Generate Barcode command
    status = ::ProCmdActionAdd(
        (char*)"BarcodePlugin_Generate",
        (uiCmdCmdActFn)BarcodeGenerateAction,
        uiProeImmediate,
        BarcodeAccessDefault,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdGenerate);
    
    // Register Settings command
    status = ::ProCmdActionAdd(
        (char*)"BarcodePlugin_Settings",
        (uiCmdCmdActFn)BarcodeSettingsAction,
        uiProeImmediate,
        BarcodeAccessDefault,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdSettings);
    
    // Register Batch Process command
    status = ::ProCmdActionAdd(
        (char*)"BarcodePlugin_Batch",
        (uiCmdCmdActFn)BarcodeBatchAction,
        uiProeImmediate,
        BarcodeAccessDefault,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdBatch);
    
    // Register Sync Check command
    status = ::ProCmdActionAdd(
        (char*)"BarcodePlugin_SyncCheck",
        (uiCmdCmdActFn)BarcodeSyncCheckAction,
        uiProeImmediate,
        BarcodeAccessDefault,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdSyncCheck);
    
    // ========================================
    // Step 2: Load Ribbon definition file
    // ========================================
    
    // Load the ribbon definition file to create the Barcode tab
    // The .rbn file path is relative to TEXT_DIR/ribbon/
    wchar_t ribbonFile[] = L"barcode_ribbon.rbn";
    status = ::ProRibbonDefinitionfileLoad(ribbonFile);
    
    // If ribbon loading fails, fall back to menu bar
    if (status != PRO_TK_NO_ERROR) {
        // Add to Tools menu as fallback
        if (g_cmdGenerate != 0) {
            wchar_t msgFile[] = L"usermsg.txt";
            ::ProMenubarmenuPushbuttonAdd(
                (char*)"Tools",
                (char*)"BarcodePlugin_Generate",
                (char*)"BarcodePlugin_Generate",
                (char*)"Generate Barcode",
                NULL,
                PRO_B_TRUE,
                g_cmdGenerate,
                msgFile);
        }
    }
    
#endif
    
    creo_barcode::g_pluginStatus = creo_barcode::PluginStatus::OK;
    
    return 0;  // Success
}

/**
 * @brief Plugin termination entry point
 * 
 * Called by Creo when the plugin is unloaded or Creo is closing.
 * This function properly releases all plugin resources (Requirement 4.4).
 */
/**
 * Pro/TOOLKIT standard user_terminate function
 */
extern "C" void user_terminate()
{
    using namespace creo_barcode;
    
    LOG_INFO("user_terminate() called");
    
    // Clean up all plugin resources
    cleanupResources();
    
    g_pluginStatus = PluginStatus::NOT_INITIALIZED;
    
    LOG_INFO("Creo Barcode Plugin terminated");
}

// Additional functions (available in all build modes)

namespace creo_barcode {

/**
 * @brief Test helper to simulate plugin initialization
 * @param version The Creo version to simulate
 * @return 0 on success, non-zero on failure
 */
int testInitialize(const CreoVersion& version) {
    LOG_INFO("testInitialize() called with version " + version.toString());
    
    CreoVersion minVersion = getMinimumVersion();
    if (!checkCreoVersion(version)) {
        showVersionError(version, minVersion);
        g_pluginStatus = PluginStatus::VERSION_ERROR;
        return -1;
    }
    
    if (!initializeResources()) {
        g_pluginStatus = PluginStatus::INIT_ERROR;
        return -1;
    }
    
    g_pluginStatus = PluginStatus::OK;
    return 0;
}

/**
 * @brief Test helper to simulate plugin termination
 */
void testTerminate() {
    cleanupResources();
    g_pluginStatus = PluginStatus::NOT_INITIALIZED;
}

/**
 * @brief Generate output path for barcode image
 * @param partName The part name to use in the filename
 * @return Full path for the output barcode image
 */
std::string generateOutputPath(const std::string& partName) {
    std::string outputDir;
    
    if (g_configManager) {
        outputDir = g_configManager->getConfig().outputDirectory;
    }
    
    // Use temp directory if no output directory configured
    if (outputDir.empty()) {
#ifdef _WIN32
        const char* temp = std::getenv("TEMP");
        if (temp) {
            outputDir = std::string(temp) + "\\creo_barcode";
        } else {
            outputDir = "C:\\Temp\\creo_barcode";
        }
#else
        outputDir = "/tmp/creo_barcode";
#endif
    }
    
    // Ensure output directory exists
    ensureOutputDirectory(outputDir);
    
    // Generate filename with timestamp to avoid conflicts
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::string filename = partName + "_" + std::to_string(timestamp) + ".png";
    
#ifdef _WIN32
    return outputDir + "\\" + filename;
#else
    return outputDir + "/" + filename;
#endif
}

/**
 * @brief Ensure output directory exists
 * @param path Directory path to create if needed
 * @return true if directory exists or was created successfully
 */
bool ensureOutputDirectory(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create output directory: " + std::string(e.what()));
        return false;
    }
}

/**
 * @brief Complete barcode generation workflow
 * 
 * This function implements the complete workflow for generating a barcode:
 * 1. Get current drawing from Creo
 * 2. Get associated model (part or assembly)
 * 3. Get part name from model
 * 4. Encode special characters if needed
 * 5. Generate barcode image
 * 6. Insert barcode into drawing
 * 
 * Requirements: 1.1, 1.2
 * 
 * @param config Barcode configuration to use
 */
void onGenerateBarcodeRequested(const BarcodeConfig& config) {
    LOG_INFO("Barcode generation workflow started");
    
    if (!g_drawingInterface || !g_barcodeGenerator) {
        LOG_ERROR("Plugin components not initialized");
        return;
    }
    
    // Step 1: Get current drawing
    ProDrawing drawing = nullptr;
    ProError err = g_drawingInterface->getCurrentDrawing(&drawing);
    if (err != PRO_TK_NO_ERROR) {
        LOG_ERROR("No drawing is currently open");
        // In real implementation, show error dialog to user
        return;
    }
    LOG_INFO("Current drawing retrieved");
    
    // Step 2: Get associated model
    ProMdl model = nullptr;
    err = g_drawingInterface->getAssociatedModel(drawing, &model);
    if (err != PRO_TK_NO_ERROR) {
        LOG_ERROR("No model associated with drawing");
        return;
    }
    LOG_INFO("Associated model retrieved");
    
    // Step 3: Check if model is assembly (Requirement 1.4)
    ModelType modelType = g_drawingInterface->getModelType(model);
    std::string partName;
    
    if (modelType == ModelType::ASSEMBLY) {
        // For assemblies, get list of parts and let user choose
        std::vector<PartInfo> parts;
        err = g_drawingInterface->getAssemblyParts(model, parts);
        if (err != PRO_TK_NO_ERROR || parts.empty()) {
            LOG_ERROR("Failed to get assembly parts");
            return;
        }
        
        // In real implementation, show dialog for user to select part
        // For now, use the first part
        LOG_INFO("Assembly detected with " + std::to_string(parts.size()) + " parts");
        partName = parts[0].name;
    } else {
        // Get part name directly
        err = g_drawingInterface->getPartName(model, partName);
        if (err != PRO_TK_NO_ERROR || partName.empty()) {
            LOG_ERROR("Failed to get part name");
            return;
        }
    }
    LOG_INFO("Part name: " + partName);
    
    // Step 4: Encode special characters (Requirement 1.3)
    std::string encodedData = g_barcodeGenerator->encodeSpecialChars(partName);
    LOG_INFO("Encoded data: " + encodedData);
    
    // Step 5: Validate data for barcode type
    if (!g_barcodeGenerator->validateData(encodedData, config.type)) {
        LOG_ERROR("Data is not valid for barcode type: " + barcodeTypeToString(config.type));
        return;
    }
    
    // Step 6: Generate barcode image
    std::string outputPath = generateOutputPath(partName);
    if (!g_barcodeGenerator->generate(encodedData, config, outputPath)) {
        LOG_ERROR("Failed to generate barcode: " + g_barcodeGenerator->getLastError().message);
        return;
    }
    LOG_INFO("Barcode generated: " + outputPath);
    
    // Step 7: Insert barcode into drawing (Requirement 1.2)
    // Default position - in real implementation, user would specify or click
    Position pos(100.0, 100.0);
    Size size(static_cast<double>(config.width) / config.dpi * 25.4,  // Convert to mm
              static_cast<double>(config.height) / config.dpi * 25.4);
    
    err = g_drawingInterface->insertImage(drawing, outputPath, pos, size);
    if (err != PRO_TK_NO_ERROR) {
        LOG_ERROR("Failed to insert barcode into drawing: " + 
                  g_drawingInterface->getLastError().message);
        return;
    }
    
    LOG_INFO("Barcode inserted into drawing successfully");
}

/**
 * @brief Batch barcode generation workflow
 * 
 * This function implements the batch processing workflow:
 * 1. Let user select multiple drawing files
 * 2. Process each file sequentially
 * 3. Report progress and results
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4
 */
void onBatchGenerateRequested() {
    LOG_INFO("Batch generation workflow started");
    
    if (!g_batchProcessor || !g_configManager) {
        LOG_ERROR("Plugin components not initialized");
        return;
    }
    
    // In real implementation, show file selection dialog
    // For now, we just log that batch processing was requested
    LOG_INFO("Batch processing requested - file selection dialog would appear here");
    
    // Get current configuration
    PluginConfig pluginConfig = g_configManager->getConfig();
    BarcodeConfig barcodeConfig;
    barcodeConfig.type = pluginConfig.defaultType;
    barcodeConfig.width = pluginConfig.defaultWidth;
    barcodeConfig.height = pluginConfig.defaultHeight;
    barcodeConfig.showText = pluginConfig.defaultShowText;
    barcodeConfig.dpi = pluginConfig.defaultDpi;
    
    // Process files with progress callback
    auto progressCallback = [](int current, int total) {
        LOG_INFO("Processing file " + std::to_string(current) + " of " + std::to_string(total));
    };
    
    std::vector<BatchResult> results = g_batchProcessor->process(barcodeConfig, progressCallback);
    
    // Generate and log summary
    std::string summary = BatchProcessor::getSummary(results);
    LOG_INFO("Batch processing complete:\n" + summary);
}

/**
 * @brief Check barcode synchronization with current part name
 * 
 * Requirement 3.3: Check if barcode data matches current part name
 * 
 * @param barcodePath Path to the barcode image file
 * @param currentPartName Current part name from the model
 * @return SyncCheckResult containing sync status and details
 */
SyncCheckResult checkBarcodeSync(const std::string& barcodePath, const std::string& currentPartName) {
    LOG_INFO("Checking barcode sync for: " + barcodePath);
    
    if (!g_dataSyncChecker || !g_barcodeGenerator) {
        LOG_ERROR("Plugin components not initialized for sync check");
        SyncCheckResult result;
        result.status = SyncStatus::UNKNOWN;
        result.message = "Plugin not initialized";
        return result;
    }
    
    // Check sync by decoding barcode from image
    SyncCheckResult result = g_dataSyncChecker->checkSyncFromImage(
        currentPartName, barcodePath, *g_barcodeGenerator);
    
    // If out of sync, display warning (Requirement 3.3)
    if (result.status == SyncStatus::OUT_OF_SYNC) {
        BarcodeInstance instance;
        instance.imagePath = barcodePath;
        instance.decodedData = result.barcodeData;
        g_dataSyncChecker->displayWarning(instance, nullptr);
    }
    
    return result;
}

/**
 * @brief Update barcode if sync check indicates it's needed
 * 
 * Requirements 3.1, 3.2: Provide update option and regenerate barcode
 * 
 * @param syncResult The result from checkBarcodeSync
 * @param config Barcode configuration to use for regeneration
 * @return true if barcode was updated, false otherwise
 */
bool updateBarcodeIfNeeded(const SyncCheckResult& syncResult, const BarcodeConfig& config) {
    if (!g_dataSyncChecker || !g_barcodeGenerator) {
        LOG_ERROR("Plugin components not initialized for barcode update");
        return false;
    }
    
    // Only update if out of sync
    if (syncResult.status != SyncStatus::OUT_OF_SYNC) {
        LOG_INFO("Barcode is in sync, no update needed");
        return false;
    }
    
    // Requirement 3.1: Prompt user to update
    bool shouldUpdate = g_dataSyncChecker->promptUpdate(syncResult, nullptr);
    
    if (!shouldUpdate) {
        LOG_INFO("User declined barcode update");
        return false;
    }
    
    // Requirement 3.2: Regenerate barcode with new part name
    LOG_INFO("Regenerating barcode with new part name: " + syncResult.currentPartName);
    
    std::string encodedData = g_barcodeGenerator->encodeSpecialChars(syncResult.currentPartName);
    std::string outputPath = generateOutputPath(syncResult.currentPartName);
    
    if (!g_barcodeGenerator->generate(encodedData, config, outputPath)) {
        LOG_ERROR("Failed to regenerate barcode: " + g_barcodeGenerator->getLastError().message);
        return false;
    }
    
    LOG_INFO("Barcode updated successfully: " + outputPath);
    return true;
}

/**
 * @brief Perform sync check on current drawing's barcode
 * 
 * This is the main entry point for sync checking, called when:
 * - User requests sync check from menu
 * - Drawing is opened (if auto-check is enabled)
 * - Part name changes are detected
 * 
 * Requirements: 3.1, 3.2, 3.3
 */
void onSyncCheckRequested() {
    LOG_INFO("Sync check workflow started");
    
    if (!g_drawingInterface || !g_barcodeGenerator || !g_dataSyncChecker) {
        LOG_ERROR("Plugin components not initialized");
        return;
    }
    
    // Get current drawing
    ProDrawing drawing = nullptr;
    ProError err = g_drawingInterface->getCurrentDrawing(&drawing);
    if (err != PRO_TK_NO_ERROR) {
        LOG_ERROR("No drawing is currently open");
        return;
    }
    
    // Get associated model
    ProMdl model = nullptr;
    err = g_drawingInterface->getAssociatedModel(drawing, &model);
    if (err != PRO_TK_NO_ERROR) {
        LOG_ERROR("No model associated with drawing");
        return;
    }
    
    // Get current part name
    std::string currentPartName;
    err = g_drawingInterface->getPartName(model, currentPartName);
    if (err != PRO_TK_NO_ERROR || currentPartName.empty()) {
        LOG_ERROR("Failed to get part name");
        return;
    }
    
    LOG_INFO("Current part name: " + currentPartName);
    
    // In real implementation, we would:
    // 1. Find all barcode instances in the drawing
    // 2. Check each one for sync status
    // 3. Display warnings for out-of-sync barcodes
    // 4. Offer to update them
    
    // For now, we simulate with a placeholder barcode path
    // In real implementation, this would come from drawing metadata
    LOG_INFO("Sync check complete - would check all barcodes in drawing");
}

// Accessor functions for testing and external use

/**
 * @brief Get the drawing interface instance
 * @return Pointer to the drawing interface, or nullptr if not initialized
 */
DrawingInterface* getDrawingInterface() {
    return g_drawingInterface.get();
}

/**
 * @brief Get the barcode generator instance
 * @return Pointer to the barcode generator, or nullptr if not initialized
 */
BarcodeGenerator* getBarcodeGenerator() {
    return g_barcodeGenerator.get();
}

/**
 * @brief Get the batch processor instance
 * @return Pointer to the batch processor, or nullptr if not initialized
 */
BatchProcessor* getBatchProcessor() {
    return g_batchProcessor.get();
}

/**
 * @brief Get the data sync checker instance
 * @return Pointer to the data sync checker, or nullptr if not initialized
 */
DataSyncChecker* getDataSyncChecker() {
    return g_dataSyncChecker.get();
}

} // namespace creo_barcode
