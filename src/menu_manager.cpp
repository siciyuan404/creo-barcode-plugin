/**
 * @file menu_manager.cpp
 * @brief Implementation of MenuManager class
 * 
 * This file implements the menu management functionality for the
 * Creo Barcode Plugin, including menu registration and callbacks.
 * 
 * Requirements: 4.1
 */

#include "menu_manager.h"
#include "settings_dialog.h"
#include "logger.h"

#include <memory>

namespace creo_barcode {

// Singleton instance
static std::unique_ptr<MenuManager> g_menuManager;

MenuManager& getMenuManager() {
    if (!g_menuManager) {
        g_menuManager = std::make_unique<MenuManager>();
    }
    return *g_menuManager;
}

MenuManager::MenuManager() {
    LOG_INFO("MenuManager created");
}

MenuManager::~MenuManager() {
    if (menusRegistered_) {
        unregisterMenus();
    }
    LOG_INFO("MenuManager destroyed");
}

void MenuManager::setError(ErrorCode code, const std::string& message, const std::string& details) {
    lastError_ = ErrorInfo(code, message, details);
    if (code != ErrorCode::SUCCESS) {
        LOG_ERROR(message + (details.empty() ? "" : ": " + details));
    }
}

void MenuManager::setGenerateBarcodeCallback(GenerateBarcodeCallback callback) {
    generateCallback_ = std::move(callback);
}

void MenuManager::setBatchGenerateCallback(BatchGenerateCallback callback) {
    batchCallback_ = std::move(callback);
}

void MenuManager::setConfigManager(ConfigManager* configManager) {
    configManager_ = configManager;
}

// Non-Creo implementation (always used for barcode_core library)

// Non-Creo implementation (for testing)

ErrorCode MenuManager::registerMenus() {
    if (menusRegistered_) {
        LOG_WARNING("Menus already registered");
        return ErrorCode::SUCCESS;
    }
    
    LOG_INFO("Registering plugin menus (non-Creo mode)");
    
    // In non-Creo mode, we just mark menus as registered
    // The actual menu functionality is simulated through direct method calls
    
    menusRegistered_ = true;
    setError(ErrorCode::SUCCESS, "Menus registered successfully (non-Creo mode)");
    
    return ErrorCode::SUCCESS;
}

ErrorCode MenuManager::registerMainMenu() {
    // No-op in non-Creo mode
    return ErrorCode::SUCCESS;
}

ErrorCode MenuManager::registerToolbarButtons() {
    // No-op in non-Creo mode
    return ErrorCode::SUCCESS;
}

ErrorCode MenuManager::unregisterMenus() {
    if (!menusRegistered_) {
        return ErrorCode::SUCCESS;
    }
    
    LOG_INFO("Unregistering plugin menus (non-Creo mode)");
    menusRegistered_ = false;
    
    return ErrorCode::SUCCESS;
}

SettingsDialogResult MenuManager::showSettingsDialog(const BarcodeConfig& currentConfig) {
    SettingsDialogResult result;
    result.config = currentConfig;
    result.accepted = false;
    
    // Use SettingsDialog class for the actual dialog
    SettingsDialog dialog;
    DialogResult dialogResult = dialog.show(currentConfig);
    
    result.accepted = dialogResult.accepted;
    result.config = dialogResult.config;
    
    return result;
}

// Common implementation

void MenuManager::handleGenerateBarcode() {
    LOG_INFO("Generate Barcode menu item activated");
    
    if (generateCallback_) {
        BarcodeConfig config;
        
        // Get current config from ConfigManager if available
        if (configManager_) {
            PluginConfig pluginConfig = configManager_->getConfig();
            config.type = pluginConfig.defaultType;
            config.width = pluginConfig.defaultWidth;
            config.height = pluginConfig.defaultHeight;
            config.showText = pluginConfig.defaultShowText;
            config.dpi = pluginConfig.defaultDpi;
        }
        
        generateCallback_(config);
    } else {
        LOG_WARNING("No generate barcode callback registered");
    }
}

void MenuManager::handleBatchGenerate() {
    LOG_INFO("Batch Generate menu item activated");
    
    if (batchCallback_) {
        batchCallback_();
    } else {
        LOG_WARNING("No batch generate callback registered");
    }
}

void MenuManager::handleSettings() {
    LOG_INFO("Settings menu item activated");
    
    if (!configManager_) {
        LOG_ERROR("No configuration manager available");
        return;
    }
    
    // Get current config
    PluginConfig pluginConfig = configManager_->getConfig();
    BarcodeConfig barcodeConfig;
    barcodeConfig.type = pluginConfig.defaultType;
    barcodeConfig.width = pluginConfig.defaultWidth;
    barcodeConfig.height = pluginConfig.defaultHeight;
    barcodeConfig.showText = pluginConfig.defaultShowText;
    barcodeConfig.dpi = pluginConfig.defaultDpi;
    
    // Show settings dialog
    SettingsDialogResult result = showSettingsDialog(barcodeConfig);
    
    if (result.accepted) {
        // Update configuration
        pluginConfig.defaultType = result.config.type;
        pluginConfig.defaultWidth = result.config.width;
        pluginConfig.defaultHeight = result.config.height;
        pluginConfig.defaultShowText = result.config.showText;
        pluginConfig.defaultDpi = result.config.dpi;
        
        configManager_->setConfig(pluginConfig);
        LOG_INFO("Settings updated");
    }
}

} // namespace creo_barcode
