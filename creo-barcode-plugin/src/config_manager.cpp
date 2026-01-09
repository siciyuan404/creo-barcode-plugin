#include "config_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace creo_barcode {

using json = nlohmann::json;

std::string ConfigManager::serialize() const {
    json j;
    j["version"] = "1.0";
    j["defaultBarcodeType"] = barcodeTypeToString(config_.defaultType);
    j["defaultWidth"] = config_.defaultWidth;
    j["defaultHeight"] = config_.defaultHeight;
    j["defaultShowText"] = config_.defaultShowText;
    j["defaultDpi"] = config_.defaultDpi;
    j["outputDirectory"] = config_.outputDirectory;
    j["recentFiles"] = config_.recentFiles;
    
    return j.dump(4);
}

bool ConfigManager::deserialize(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        if (j.contains("defaultBarcodeType")) {
            auto typeOpt = stringToBarcodeType(j["defaultBarcodeType"].get<std::string>());
            if (typeOpt) config_.defaultType = *typeOpt;
        }
        
        if (j.contains("defaultWidth")) config_.defaultWidth = j["defaultWidth"].get<int>();
        if (j.contains("defaultHeight")) config_.defaultHeight = j["defaultHeight"].get<int>();
        if (j.contains("defaultShowText")) config_.defaultShowText = j["defaultShowText"].get<bool>();
        if (j.contains("defaultDpi")) config_.defaultDpi = j["defaultDpi"].get<int>();
        if (j.contains("outputDirectory")) config_.outputDirectory = j["outputDirectory"].get<std::string>();
        if (j.contains("recentFiles")) config_.recentFiles = j["recentFiles"].get<std::vector<std::string>>();
        
        return true;
    } catch (const json::exception& e) {
        lastError_ = ErrorInfo(ErrorCode::CONFIG_LOAD_FAILED, e.what());
        return false;
    }
}

bool ConfigManager::loadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        lastError_ = ErrorInfo(ErrorCode::FILE_NOT_FOUND, "Cannot open config file: " + configPath);
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return deserialize(content);
}

bool ConfigManager::saveConfig(const std::string& configPath) {
    std::ofstream file(configPath);
    if (!file.is_open()) {
        lastError_ = ErrorInfo(ErrorCode::CONFIG_SAVE_FAILED, "Cannot write config file: " + configPath);
        return false;
    }
    
    file << serialize();
    return file.good();
}

} // namespace creo_barcode
