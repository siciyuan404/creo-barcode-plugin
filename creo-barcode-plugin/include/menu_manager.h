/**
 * @file menu_manager.h
 * @brief Menu management for Creo Barcode Plugin
 * 
 * This file defines the MenuManager class which handles:
 * - Registration of menu items in Creo's menu bar
 * - Menu callback functions for user interactions
 * - Settings dialog management
 * 
 * Requirements: 4.1
 */

#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <string>
#include <functional>
#include "barcode_generator.h"
#include "config_manager.h"
#include "error_codes.h"

namespace creo_barcode {

/**
 * @brief Settings dialog result structure
 */
struct SettingsDialogResult {
    bool accepted = false;
    BarcodeConfig config;
};

/**
 * @brief Callback type for barcode generation
 */
using GenerateBarcodeCallback = std::function<void(const BarcodeConfig&)>;

/**
 * @brief Callback type for batch generation
 */
using BatchGenerateCallback = std::function<void()>;

/**
 * @brief MenuManager class handles Creo menu integration
 * 
 * This class is responsible for:
 * - Registering plugin menus in Creo's interface
 * - Handling menu item callbacks
 * - Managing the settings dialog
 */
class MenuManager {
public:
    MenuManager();
    ~MenuManager();
    
    /**
     * @brief Register all plugin menus in Creo
     * @return ErrorCode indicating success or failure
     */
    ErrorCode registerMenus();
    
    /**
     * @brief Unregister all plugin menus
     * @return ErrorCode indicating success or failure
     */
    ErrorCode unregisterMenus();
    
    /**
     * @brief Check if menus are registered
     * @return true if menus are registered
     */
    bool isRegistered() const { return menusRegistered_; }
    
    /**
     * @brief Set callback for barcode generation
     * @param callback Function to call when user requests barcode generation
     */
    void setGenerateBarcodeCallback(GenerateBarcodeCallback callback);
    
    /**
     * @brief Set callback for batch generation
     * @param callback Function to call when user requests batch generation
     */
    void setBatchGenerateCallback(BatchGenerateCallback callback);
    
    /**
     * @brief Set the configuration manager for settings dialog
     * @param configManager Pointer to the configuration manager
     */
    void setConfigManager(ConfigManager* configManager);
    
    /**
     * @brief Show the settings dialog
     * @param currentConfig Current configuration to display
     * @return SettingsDialogResult with user's choices
     */
    SettingsDialogResult showSettingsDialog(const BarcodeConfig& currentConfig);
    
    /**
     * @brief Get the last error
     * @return ErrorInfo with error details
     */
    ErrorInfo getLastError() const { return lastError_; }
    
    // Callback handlers
    void handleGenerateBarcode();
    void handleBatchGenerate();
    void handleSettings();
    
private:
    bool menusRegistered_ = false;
    ErrorInfo lastError_;
    
    GenerateBarcodeCallback generateCallback_;
    BatchGenerateCallback batchCallback_;
    ConfigManager* configManager_ = nullptr;
    
    // Helper methods
    ErrorCode registerMainMenu();
    ErrorCode registerToolbarButtons();
    void setError(ErrorCode code, const std::string& message, const std::string& details = "");
};

/**
 * @brief Get the global MenuManager instance
 * @return Reference to the singleton MenuManager
 */
MenuManager& getMenuManager();

} // namespace creo_barcode

#endif // MENU_MANAGER_H
