#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>
#include "barcode_generator.h"
#include "error_codes.h"

namespace creo_barcode {

struct PluginConfig {
    BarcodeType defaultType = BarcodeType::CODE_128;
    int defaultWidth = 200;
    int defaultHeight = 80;
    bool defaultShowText = true;
    std::string outputDirectory;
    int defaultDpi = 300;
    std::vector<std::string> recentFiles;
    
    bool operator==(const PluginConfig& other) const {
        return defaultType == other.defaultType &&
               defaultWidth == other.defaultWidth &&
               defaultHeight == other.defaultHeight &&
               defaultShowText == other.defaultShowText &&
               outputDirectory == other.outputDirectory &&
               defaultDpi == other.defaultDpi &&
               recentFiles == other.recentFiles;
    }
};

class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    // Load configuration from file
    bool loadConfig(const std::string& configPath);
    
    // Save configuration to file
    bool saveConfig(const std::string& configPath);
    
    // Get/set configuration
    PluginConfig getConfig() const { return config_; }
    void setConfig(const PluginConfig& config) { config_ = config; }
    
    // Serialize to JSON string
    std::string serialize() const;
    
    // Deserialize from JSON string
    bool deserialize(const std::string& jsonStr);
    
    // Get last error
    ErrorInfo getLastError() const { return lastError_; }
    
private:
    PluginConfig config_;
    ErrorInfo lastError_;
};

} // namespace creo_barcode

#endif // CONFIG_MANAGER_H
