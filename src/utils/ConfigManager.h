#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <map>
#include <vector>
#include <memory>
#include "../core/SystemManager.h"

// Configuration value types
enum class ConfigValueType {
    STRING = 0,
    INTEGER = 1,
    FLOAT = 2,
    BOOLEAN = 3,
    ARRAY = 4,
    OBJECT = 5
};

// Configuration value
struct ConfigValue {
    ConfigValueType type;
    String string_value;
    int int_value;
    float float_value;
    bool bool_value;
    std::vector<ConfigValue> array_value;
    std::map<String, ConfigValue> object_value;
    
    ConfigValue() : type(ConfigValueType::STRING) {}
    ConfigValue(const String& val) : type(ConfigValueType::STRING), string_value(val) {}
    ConfigValue(int val) : type(ConfigValueType::INTEGER), int_value(val) {}
    ConfigValue(float val) : type(ConfigValueType::FLOAT), float_value(val) {}
    ConfigValue(bool val) : type(ConfigValueType::BOOLEAN), bool_value(val) {}
};

// Configuration profile
struct ConfigProfile {
    String name;
    std::map<String, ConfigValue> values;
    String description;
    unsigned long created_at;
    
    ConfigProfile(const String& n, const String& desc = "")
        : name(n), description(desc), created_at(millis()) {}
};

// Configuration validation
struct ConfigValidation {
    String key;
    ConfigValueType expected_type;
    std::function<bool(const ConfigValue&)> validator;
    String error_message;
    bool required;
    
    ConfigValidation(const String& k, ConfigValueType type, bool req = true)
        : key(k), expected_type(type), required(req) {}
    
    ConfigValidation& withValidator(std::function<bool(const ConfigValue&)> val, const String& error) {
        validator = val;
        error_message = error;
        return *this;
    }
};

class ConfigManager {
private:
    // Current configuration
    std::map<String, ConfigValue> current_config;
    
    // Configuration profiles
    std::vector<ConfigProfile> profiles;
    ConfigProfile* active_profile;
    
    // Validation rules
    std::vector<ConfigValidation> validation_rules;
    
    // Configuration sources
    bool use_file_config;
    bool use_network_config;
    bool use_ble_config;
    String config_file_path;
    String network_config_url;
    
    // State
    bool initialized;
    bool config_loaded;
    unsigned long last_config_update;
    
    // Statistics
    uint32_t config_updates;
    uint32_t validation_errors;
    uint32_t profile_switches;
    
    // Internal methods
    void loadDefaultConfiguration();
    void loadConfigurationFromFile();
    void loadConfigurationFromNetwork();
    void loadConfigurationFromBLE();
    void saveConfigurationToFile();
    bool validateConfiguration();
    bool validateConfigValue(const ConfigValidation& rule, const ConfigValue& value) const;
    void applyConfiguration();
    ConfigProfile* findProfile(const String& name);
    void createDefaultProfiles();
    
public:
    ConfigManager();
    ~ConfigManager();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    bool isConfigLoaded() const { return config_loaded; }
    
    // Configuration loading
    bool loadConfiguration();
    bool reloadConfiguration();
    void setConfigFilePath(const String& path) { config_file_path = path; }
    void setNetworkConfigURL(const String& url) { network_config_url = url; }
    void enableFileConfig(bool enable) { use_file_config = enable; }
    void enableNetworkConfig(bool enable) { use_network_config = enable; }
    void enableBLEConfig(bool enable) { use_ble_config = enable; }
    
    // Configuration access
    ConfigValue getConfig(const String& key) const;
    String getString(const String& key) const;
    int getInt(const String& key) const;
    float getFloat(const String& key) const;
    bool getBool(const String& key) const;
    
    // Configuration modification
    bool setConfig(const String& key, const ConfigValue& value);
    bool setString(const String& key, const String& value);
    bool setInt(const String& key, int value);
    bool setFloat(const String& key, float value);
    bool setBool(const String& key, bool value);
    
    // Configuration validation
    void addValidationRule(const ConfigValidation& rule);
    void clearValidationRules();
    std::vector<String> validateConfig() const;
    bool isConfigValid() const;
    
    // Configuration profiles
    bool createProfile(const String& name, const String& description = "");
    bool saveProfile(const String& name);
    bool loadProfile(const String& name);
    bool deleteProfile(const String& name);
    std::vector<String> listProfiles() const;
    ConfigProfile* getActiveProfile() const { return active_profile; }
    
    // Configuration portal
    bool startConfigurationPortal();
    void stopConfigurationPortal();
    bool isConfigurationPortalActive() const;
    
    // Utility
    void printConfiguration() const;
    void printProfiles() const;
    void printStatistics() const;
    std::vector<String> getAllKeys() const;
    bool hasKey(const String& key) const;
    size_t getConfigCount() const { return current_config.size(); }
    
    // Statistics
    uint32_t getConfigUpdates() const { return config_updates; }
    uint32_t getValidationErrors() const { return validation_errors; }
    uint32_t getProfileSwitches() const { return profile_switches; }
    
    // Advanced features
    bool exportConfiguration(String& output) const;
    bool importConfiguration(const String& input);
    bool backupConfiguration(const String& backup_name);
    bool restoreConfiguration(const String& backup_name);
    void listBackups(std::vector<String>& backups) const;
};

// Global config manager access
#define CONFIG_MANAGER() (SystemManager::getInstance().getConfigManager()))

#endif // CONFIG_MANAGER_H