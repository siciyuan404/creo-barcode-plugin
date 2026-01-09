#include <gtest/gtest.h>
#include "config_manager.h"
#include <filesystem>
#include <fstream>

namespace creo_barcode {
namespace testing {

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "config_test";
        std::filesystem::create_directories(testDir_);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
    ConfigManager manager_;
};

TEST_F(ConfigManagerTest, SerializeProducesValidJson) {
    PluginConfig config;
    config.defaultType = BarcodeType::QR_CODE;
    config.defaultWidth = 300;
    config.defaultHeight = 300;
    
    manager_.setConfig(config);
    std::string json = manager_.serialize();
    
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("QR_CODE"), std::string::npos);
    EXPECT_NE(json.find("300"), std::string::npos);
}

TEST_F(ConfigManagerTest, DeserializeRestoresConfig) {
    std::string json = R"({
        "defaultBarcodeType": "CODE_39",
        "defaultWidth": 250,
        "defaultHeight": 100,
        "defaultShowText": false,
        "defaultDpi": 150,
        "outputDirectory": "/tmp/test",
        "recentFiles": ["file1.drw", "file2.drw"]
    })";
    
    EXPECT_TRUE(manager_.deserialize(json));
    
    auto config = manager_.getConfig();
    EXPECT_EQ(config.defaultType, BarcodeType::CODE_39);
    EXPECT_EQ(config.defaultWidth, 250);
    EXPECT_EQ(config.defaultHeight, 100);
    EXPECT_FALSE(config.defaultShowText);
    EXPECT_EQ(config.defaultDpi, 150);
    EXPECT_EQ(config.outputDirectory, "/tmp/test");
    EXPECT_EQ(config.recentFiles.size(), 2);
}

TEST_F(ConfigManagerTest, DeserializeHandlesInvalidJson) {
    EXPECT_FALSE(manager_.deserialize("not valid json"));
    EXPECT_FALSE(manager_.deserialize("{incomplete"));
}

TEST_F(ConfigManagerTest, SaveAndLoadConfig) {
    PluginConfig config;
    config.defaultType = BarcodeType::DATA_MATRIX;
    config.defaultWidth = 400;
    config.outputDirectory = "/custom/path";
    
    manager_.setConfig(config);
    
    std::string configPath = (testDir_ / "test_config.json").string();
    EXPECT_TRUE(manager_.saveConfig(configPath));
    
    ConfigManager loader;
    EXPECT_TRUE(loader.loadConfig(configPath));
    
    auto loaded = loader.getConfig();
    EXPECT_EQ(loaded.defaultType, BarcodeType::DATA_MATRIX);
    EXPECT_EQ(loaded.defaultWidth, 400);
    EXPECT_EQ(loaded.outputDirectory, "/custom/path");
}

TEST_F(ConfigManagerTest, LoadConfigFailsForMissingFile) {
    EXPECT_FALSE(manager_.loadConfig("/nonexistent/path/config.json"));
}

// Additional unit tests for ConfigManager

TEST_F(ConfigManagerTest, SaveConfigCreatesFile) {
    std::string configPath = (testDir_ / "new_config.json").string();
    
    // Ensure file doesn't exist
    EXPECT_FALSE(std::filesystem::exists(configPath));
    
    EXPECT_TRUE(manager_.saveConfig(configPath));
    
    // Verify file was created
    EXPECT_TRUE(std::filesystem::exists(configPath));
}

TEST_F(ConfigManagerTest, SaveConfigPreservesAllFields) {
    PluginConfig config;
    config.defaultType = BarcodeType::EAN_13;
    config.defaultWidth = 500;
    config.defaultHeight = 200;
    config.defaultShowText = false;
    config.defaultDpi = 600;
    config.outputDirectory = "/test/output";
    config.recentFiles = {"a.drw", "b.drw", "c.drw"};
    
    manager_.setConfig(config);
    
    std::string configPath = (testDir_ / "full_config.json").string();
    EXPECT_TRUE(manager_.saveConfig(configPath));
    
    ConfigManager loader;
    EXPECT_TRUE(loader.loadConfig(configPath));
    
    auto loaded = loader.getConfig();
    EXPECT_EQ(loaded.defaultType, BarcodeType::EAN_13);
    EXPECT_EQ(loaded.defaultWidth, 500);
    EXPECT_EQ(loaded.defaultHeight, 200);
    EXPECT_FALSE(loaded.defaultShowText);
    EXPECT_EQ(loaded.defaultDpi, 600);
    EXPECT_EQ(loaded.outputDirectory, "/test/output");
    EXPECT_EQ(loaded.recentFiles.size(), 3);
    EXPECT_EQ(loaded.recentFiles[0], "a.drw");
}

TEST_F(ConfigManagerTest, DeserializeHandlesEmptyJson) {
    EXPECT_FALSE(manager_.deserialize(""));
}

TEST_F(ConfigManagerTest, DeserializeHandlesPartialConfig) {
    // Only some fields present - should use defaults for missing
    std::string json = R"({
        "defaultWidth": 350
    })";
    
    EXPECT_TRUE(manager_.deserialize(json));
    
    auto config = manager_.getConfig();
    EXPECT_EQ(config.defaultWidth, 350);
    // Other fields should retain defaults
    EXPECT_EQ(config.defaultType, BarcodeType::CODE_128);
    EXPECT_TRUE(config.defaultShowText);
}

TEST_F(ConfigManagerTest, DeserializeHandlesInvalidBarcodeType) {
    std::string json = R"({
        "defaultBarcodeType": "INVALID_TYPE",
        "defaultWidth": 200
    })";
    
    EXPECT_TRUE(manager_.deserialize(json));
    
    // Invalid type should be ignored, keeping default
    auto config = manager_.getConfig();
    EXPECT_EQ(config.defaultType, BarcodeType::CODE_128);
    EXPECT_EQ(config.defaultWidth, 200);
}

TEST_F(ConfigManagerTest, DeserializeHandlesWrongFieldTypes) {
    // Width as string instead of int - should fail
    std::string json = R"({
        "defaultWidth": "not_a_number"
    })";
    
    EXPECT_FALSE(manager_.deserialize(json));
}

TEST_F(ConfigManagerTest, LoadConfigHandlesCorruptedFile) {
    std::string configPath = (testDir_ / "corrupted.json").string();
    
    // Write corrupted content
    std::ofstream file(configPath);
    file << "{ corrupted json content";
    file.close();
    
    EXPECT_FALSE(manager_.loadConfig(configPath));
}

TEST_F(ConfigManagerTest, GetSetConfigWorks) {
    PluginConfig config;
    config.defaultType = BarcodeType::QR_CODE;
    config.defaultWidth = 256;
    
    manager_.setConfig(config);
    
    auto retrieved = manager_.getConfig();
    EXPECT_EQ(retrieved.defaultType, BarcodeType::QR_CODE);
    EXPECT_EQ(retrieved.defaultWidth, 256);
}

TEST_F(ConfigManagerTest, DefaultConfigValues) {
    auto config = manager_.getConfig();
    
    // Verify default values
    EXPECT_EQ(config.defaultType, BarcodeType::CODE_128);
    EXPECT_EQ(config.defaultWidth, 200);
    EXPECT_EQ(config.defaultHeight, 80);
    EXPECT_TRUE(config.defaultShowText);
    EXPECT_EQ(config.defaultDpi, 300);
    EXPECT_TRUE(config.recentFiles.empty());
}

TEST_F(ConfigManagerTest, SaveConfigToInvalidPath) {
    // Try to save to a path that doesn't exist and can't be created
    EXPECT_FALSE(manager_.saveConfig("/nonexistent/deeply/nested/path/config.json"));
}

TEST_F(ConfigManagerTest, SerializeAllBarcodeTypes) {
    // Test serialization of each barcode type
    std::vector<BarcodeType> types = {
        BarcodeType::CODE_128,
        BarcodeType::CODE_39,
        BarcodeType::QR_CODE,
        BarcodeType::DATA_MATRIX,
        BarcodeType::EAN_13
    };
    
    for (auto type : types) {
        PluginConfig config;
        config.defaultType = type;
        manager_.setConfig(config);
        
        std::string json = manager_.serialize();
        EXPECT_FALSE(json.empty());
        
        ConfigManager loader;
        EXPECT_TRUE(loader.deserialize(json));
        EXPECT_EQ(loader.getConfig().defaultType, type);
    }
}

} // namespace testing
} // namespace creo_barcode
