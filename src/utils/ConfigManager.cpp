#include "ConfigManager.h"
#include "../core/SystemManager.h"

ConfigManager::ConfigManager()
    : active_profile(nullptr), use_file_config(false), use_network_config(false),
      use_ble_config(false), initialized(false), config_loaded(false),
      last_config_update(0), config_updates(0), validation_errors(0), profile_switches(0) {
    
    // Set default paths
    config_file_path = "/config.json";
    network_config_url = "http://config.server/config.json";
}

ConfigManager::~ConfigManager() {
    shutdown();
}

bool ConfigManager::initialize() {
    if (initialized) {
        return true;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Initializing ConfigManager");
    }
    
    // Load default configuration
    loadDefaultConfiguration();
    
    // Create default profiles
    createDefaultProfiles();
    
    // Add validation rules
    addValidationRule(ConfigValidation("wifi.ssid", ConfigValueType::STRING)
                     .withValidator([](const ConfigValue& v) { return v.string_value.length() > 0; }, 
                                   "WiFi SSID cannot be empty"));
    
    addValidationRule(ConfigValidation("wifi.password", ConfigValueType::STRING));
    addValidationRule(ConfigValidation("server.host", ConfigValueType::STRING)
                     .withValidator([](const ConfigValue& v) { return v.string_value.length() > 0; },
                                   "Server host cannot be empty"));
    
    addValidationRule(ConfigValidation("server.port", ConfigValueType::INTEGER)
                     .withValidator([](const ConfigValue& v) { return v.int_value > 0 && v.int_value < 65536; },
                                   "Server port must be between 1 and 65535"));
    
    initialized = true;
    
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "ConfigManager initialized with %u configuration items",
                   current_config.size());
    }
    
    return true;
}

void ConfigManager::shutdown() {
    if (!initialized) {
        return;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Shutting down ConfigManager");
        printStatistics();
    }
    
    // Save current configuration if modified
    if (config_loaded && use_file_config) {
        saveConfigurationToFile();
    }
    
    // Clear all data
    current_config.clear();
    profiles.clear();
    validation_rules.clear();
    active_profile = nullptr;
    
    initialized = false;
}

void ConfigManager::loadDefaultConfiguration() {
    // Load configuration from compile-time config.h
    setString("wifi.ssid", WIFI_SSID);
    setString("wifi.password", WIFI_PASSWORD);
    setString("server.host", SERVER_HOST);
    setInt("server.port", SERVER_PORT);
    setInt("audio.sample_rate", I2S_SAMPLE_RATE);
    setInt("audio.bit_depth", 16);
    setInt("audio.channels", 1);
    setBool("audio.noise_reduction", true);
    setBool("audio.agc", true);
    setBool("audio.vad", true);
    setFloat("audio.noise_reduction_level", 0.7f);
    setFloat("audio.agc_target_level", 0.3f);
    setInt("network.reconnect_delay", 5000);
    setInt("system.debug_level", DEBUG_LEVEL);
    setBool("system.auto_recovery", true);
    setInt("system.watchdog_timeout", WATCHDOG_TIMEOUT_SEC);
    
    config_loaded = true;
    config_updates++;
}

void ConfigManager::createDefaultProfiles() {
    // High quality profile
    ConfigProfile high_quality("high_quality", "High quality audio streaming");
    high_quality.values["audio.sample_rate"] = ConfigValue(32000);
    high_quality.values["audio.bit_depth"] = ConfigValue(16);
    high_quality.values["audio.noise_reduction"] = ConfigValue(true);
    high_quality.values["audio.agc"] = ConfigValue(true);
    high_quality.values["audio.vad"] = ConfigValue(true);
    profiles.push_back(high_quality);
    
    // Medium quality profile
    ConfigProfile medium_quality("medium_quality", "Balanced quality and performance");
    medium_quality.values["audio.sample_rate"] = ConfigValue(16000);
    medium_quality.values["audio.bit_depth"] = ConfigValue(16);
    medium_quality.values["audio.noise_reduction"] = ConfigValue(true);
    medium_quality.values["audio.agc"] = ConfigValue(true);
    medium_quality.values["audio.vad"] = ConfigValue(false);
    profiles.push_back(medium_quality);
    
    // Low quality profile
    ConfigProfile low_quality("low_quality", "Low bandwidth, basic quality");
    low_quality.values["audio.sample_rate"] = ConfigValue(8000);
    low_quality.values["audio.bit_depth"] = ConfigValue(8);
    low_quality.values["audio.noise_reduction"] = ConfigValue(false);
    low_quality.values["audio.agc"] = ConfigValue(true);
    low_quality.values["audio.vad"] = ConfigValue(false);
    profiles.push_back(low_quality);
    
    // Power saving profile
    ConfigProfile power_saving("power_saving", "Optimized for low power consumption");
    power_saving.values["audio.sample_rate"] = ConfigValue(16000);
    power_saving.values["audio.bit_depth"] = ConfigValue(8);
    power_saving.values["audio.noise_reduction"] = ConfigValue(false);
    power_saving.values["audio.agc"] = ConfigValue(false);
    power_saving.values["audio.vad"] = ConfigValue(false);
    power_saving.values["system.debug_level"] = ConfigValue(1);  // Minimal logging
    profiles.push_back(power_saving);
}

bool ConfigManager::loadConfiguration() {
    if (!initialized) {
        return false;
    }
    
    bool loaded = false;
    
    // Try loading from different sources in priority order
    if (use_file_config) {
        loadConfigurationFromFile();
        loaded = true;
    }
    
    if (use_network_config) {
        loadConfigurationFromNetwork();
        loaded = true;
    }
    
    if (use_ble_config) {
        loadConfigurationFromBLE();
        loaded = true;
    }
    
    // Validate loaded configuration
    if (loaded) {
        if (!validateConfiguration()) {
            auto logger = SystemManager::getInstance().getLogger();
            if (logger) {
                logger->log(LOG_ERROR, "ConfigManager", "Configuration validation failed");
            }
            return false;
        }
        
        applyConfiguration();
        config_loaded = true;
        config_updates++;
        last_config_update = millis();
    }
    
    return loaded;
}

void ConfigManager::loadConfigurationFromFile() {
    // File-based configuration loading would be implemented here
    // For now, this is a placeholder
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Loading configuration from file: %s",
                   config_file_path.c_str());
    }
}

void ConfigManager::loadConfigurationFromNetwork() {
    // Network-based configuration loading would be implemented here
    // For now, this is a placeholder
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Loading configuration from network: %s",
                   network_config_url.c_str());
    }
}

void ConfigManager::loadConfigurationFromBLE() {
    // BLE-based configuration loading would be implemented here
    // For now, this is a placeholder
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Loading configuration from BLE");
    }
}

void ConfigManager::saveConfigurationToFile() {
    // File-based configuration saving would be implemented here
    // For now, this is a placeholder
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Saving configuration to file: %s",
                   config_file_path.c_str());
    }
}

bool ConfigManager::validateConfiguration() {
    std::vector<String> errors = validateConfig();
    
    if (!errors.empty()) {
        validation_errors += errors.size();
        
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LOG_ERROR, "ConfigManager", "Configuration validation failed with %u errors:",
                       errors.size());
            for (const auto& error : errors) {
                logger->log(LOG_ERROR, "ConfigManager", "  %s", error.c_str());
            }
        }
        
        return false;
    }
    
    return true;
}

bool ConfigManager::validateConfigValue(const ConfigValidation& rule, const ConfigValue& value) {
    // Check type
    if (value.type != rule.expected_type) {
        return false;
    }
    
    // Check custom validator
    if (rule.validator) {
        return rule.validator(value);
    }
    
    return true;
}

std::vector<String> ConfigManager::validateConfig() const {
    std::vector<String> errors;
    
    for (const auto& rule : validation_rules) {
        auto it = current_config.find(rule.key);
        
        if (it == current_config.end()) {
            if (rule.required) {
                errors.push_back("Missing required configuration: " + rule.key);
            }
            continue;
        }
        
        if (!validateConfigValue(rule, it->second)) {
            String error = "Invalid configuration for " + rule.key;
            if (!rule.error_message.isEmpty()) {
                error += ": " + rule.error_message;
            }
            errors.push_back(error);
        }
    }
    
    return errors;
}

bool ConfigManager::isConfigValid() const {
    return validateConfig().empty();
}

void ConfigManager::applyConfiguration() {
    // Apply configuration to system components
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Applying configuration");
    }
    
    // Apply audio configuration
    auto audio_processor = SystemManager::getInstance().getAudioProcessor();
    if (audio_processor) {
        if (hasKey("audio.noise_reduction")) {
            audio_processor->enableFeature(AudioFeature::NOISE_REDUCTION, getBool("audio.noise_reduction"));
        }
        if (hasKey("audio.agc")) {
            audio_processor->enableFeature(AudioFeature::AUTOMATIC_GAIN_CONTROL, getBool("audio.agc"));
        }
        if (hasKey("audio.vad")) {
            audio_processor->enableFeature(AudioFeature::VOICE_ACTIVITY_DETECTION, getBool("audio.vad"));
        }
    }
    
    // Apply network configuration
    auto network_manager = SystemManager::getInstance().getNetworkManager();
    if (network_manager) {
        if (hasKey("wifi.ssid") && hasKey("wifi.password")) {
            network_manager->addWiFiNetwork(getString("wifi.ssid"), getString("wifi.password"));
        }
    }
    
    // Apply system configuration
    if (hasKey("system.debug_level")) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->setGlobalMinLevel(static_cast<LogLevel>(getInt("system.debug_level")));
        }
    }
}

ConfigValue ConfigManager::getConfig(const String& key) const {
    auto it = current_config.find(key);
    if (it != current_config.end()) {
        return it->second;
    }
    
    // Return empty value
    return ConfigValue();
}

String ConfigManager::getString(const String& key) const {
    auto value = getConfig(key);
    if (value.type == ConfigValueType::STRING) {
        return value.string_value;
    }
    return "";
}

int ConfigManager::getInt(const String& key) const {
    auto value = getConfig(key);
    if (value.type == ConfigValueType::INTEGER) {
        return value.int_value;
    }
    return 0;
}

float ConfigManager::getFloat(const String& key) const {
    auto value = getConfig(key);
    if (value.type == ConfigValueType::FLOAT) {
        return value.float_value;
    }
    return 0.0f;
}

bool ConfigManager::getBool(const String& key) const {
    auto value = getConfig(key);
    if (value.type == ConfigValueType::BOOLEAN) {
        return value.bool_value;
    }
    return false;
}

bool ConfigManager::setConfig(const String& key, const ConfigValue& value) {
    if (!initialized) {
        return false;
    }
    
    current_config[key] = value;
    config_updates++;
    last_config_update = millis();
    
    return true;
}

bool ConfigManager::setString(const String& key, const String& value) {
    return setConfig(key, ConfigValue(value));
}

bool ConfigManager::setInt(const String& key, int value) {
    return setConfig(key, ConfigValue(value));
}

bool ConfigManager::setFloat(const String& key, float value) {
    return setConfig(key, ConfigValue(value));
}

bool ConfigManager::setBool(const String& key, bool value) {
    return setConfig(key, ConfigValue(value));
}

void ConfigManager::addValidationRule(const ConfigValidation& rule) {
    validation_rules.push_back(rule);
}

void ConfigManager::clearValidationRules() {
    validation_rules.clear();
}

bool ConfigManager::createProfile(const String& name, const String& description) {
    if (findProfile(name)) {
        return false;  // Profile already exists
    }
    
    profiles.emplace_back(name, description);
    return true;
}

bool ConfigManager::saveProfile(const String& name) {
    ConfigProfile* profile = findProfile(name);
    if (!profile) {
        return false;
    }
    
    // Save current configuration to profile
    profile->values = current_config;
    profile->created_at = millis();
    
    return true;
}

bool ConfigManager::loadProfile(const String& name) {
    ConfigProfile* profile = findProfile(name);
    if (!profile) {
        return false;
    }
    
    // Load profile configuration
    current_config = profile->values;
    active_profile = profile;
    profile_switches++;
    config_updates++;
    last_config_update = millis();
    
    // Apply the new configuration
    applyConfiguration();
    
    return true;
}

bool ConfigManager::deleteProfile(const String& name) {
    auto it = std::find_if(profiles.begin(), profiles.end(),
        [&name](const ConfigProfile& profile) { return profile.name == name; });
    
    if (it == profiles.end()) {
        return false;
    }
    
    if (active_profile == &(*it)) {
        active_profile = nullptr;
    }
    
    profiles.erase(it);
    return true;
}

std::vector<String> ConfigManager::listProfiles() const {
    std::vector<String> profile_names;
    for (const auto& profile : profiles) {
        profile_names.push_back(profile.name);
    }
    return profile_names;
}

ConfigProfile* ConfigManager::findProfile(const String& name) {
    for (auto& profile : profiles) {
        if (profile.name == name) {
            return &profile;
        }
    }
    return nullptr;
}

bool ConfigManager::startConfigurationPortal() {
    // Configuration portal implementation would go here
    // For now, this is a placeholder
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Starting configuration portal");
    }
    return true;
}

void ConfigManager::stopConfigurationPortal() {
    // Configuration portal implementation would go here
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Stopping configuration portal");
    }
}

bool ConfigManager::isConfigurationPortalActive() const {
    // Configuration portal implementation would go here
    return false;
}

void ConfigManager::printConfiguration() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "ConfigManager", "=== Current Configuration ===");
    logger->log(LOG_INFO, "ConfigManager", "Total items: %u", current_config.size());
    
    for (const auto& pair : current_config) {
        const String& key = pair.first;
        const ConfigValue& value = pair.second;
        
        String value_str;
        switch (value.type) {
            case ConfigValueType::STRING:
                value_str = value.string_value;
                break;
            case ConfigValueType::INTEGER:
                value_str = String(value.int_value);
                break;
            case ConfigValueType::FLOAT:
                value_str = String(value.float_value);
                break;
            case ConfigValueType::BOOLEAN:
                value_str = value.bool_value ? "true" : "false";
                break;
            default:
                value_str = "unknown";
        }
        
        logger->log(LOG_INFO, "ConfigManager", "%s: %s", key.c_str(), value_str.c_str());
    }
    
    logger->log(LOG_INFO, "ConfigManager", "============================");
}

void ConfigManager::printProfiles() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "ConfigManager", "=== Configuration Profiles ===");
    
    for (const auto& profile : profiles) {
        logger->log(LOG_INFO, "ConfigManager", "%s: %s (%u items)",
                   profile.name.c_str(), profile.description.c_str(), profile.values.size());
        
        if (active_profile == &profile) {
            logger->log(LOG_INFO, "ConfigManager", "  [ACTIVE]");
        }
    }
    
    logger->log(LOG_INFO, "ConfigManager", "=============================");
}

void ConfigManager::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "ConfigManager", "=== Configuration Statistics ===");
    logger->log(LOG_INFO, "ConfigManager", "Configuration updates: %u", config_updates);
    logger->log(LOG_INFO, "ConfigManager", "Validation errors: %u", validation_errors);
    logger->log(LOG_INFO, "ConfigManager", "Profile switches: %u", profile_switches);
    logger->log(LOG_INFO, "ConfigManager", "Current items: %u", current_config.size());
    logger->log(LOG_INFO, "ConfigManager", "Profiles: %u", profiles.size());
    logger->log(LOG_INFO, "ConfigManager", "Validation rules: %u", validation_rules.size());
    logger->log(LOG_INFO, "ConfigManager", "Last update: %lu ms ago", millis() - last_config_update);
    logger->log(LOG_INFO, "ConfigManager", "================================");
}

std::vector<String> ConfigManager::getAllKeys() const {
    std::vector<String> keys;
    for (const auto& pair : current_config) {
        keys.push_back(pair.first);
    }
    return keys;
}

bool ConfigManager::hasKey(const String& key) const {
    return current_config.find(key) != current_config.end();
}

bool ConfigManager::exportConfiguration(String& output) const {
    // Export configuration as JSON string
    output = "{";
    bool first = true;
    
    for (const auto& pair : current_config) {
        if (!first) output += ",";
        first = false;
        
        output += "\"" + pair.first + "\":";
        
        const ConfigValue& value = pair.second;
        switch (value.type) {
            case ConfigValueType::STRING:
                output += "\"" + value.string_value + "\"";
                break;
            case ConfigValueType::INTEGER:
                output += String(value.int_value);
                break;
            case ConfigValueType::FLOAT:
                output += String(value.float_value);
                break;
            case ConfigValueType::BOOLEAN:
                output += value.bool_value ? "true" : "false";
                break;
            default:
                output += "null";
        }
    }
    
    output += "}";
    return true;
}

bool ConfigManager::importConfiguration(const String& input) {
    // Import configuration from JSON string
    // This is a simplified implementation
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Importing configuration");
    }
    
    // For now, just mark as updated
    config_updates++;
    last_config_update = millis();
    
    return true;
}

bool ConfigManager::backupConfiguration(const String& backup_name) {
    // Configuration backup implementation would go here
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Creating configuration backup: %s",
                   backup_name.c_str());
    }
    return true;
}

bool ConfigManager::restoreConfiguration(const String& backup_name) {
    // Configuration restore implementation would go here
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "ConfigManager", "Restoring configuration from backup: %s",
                   backup_name.c_str());
    }
    return true;
}

void ConfigManager::listBackups(std::vector<String>& backups) const {
    // Configuration backup listing would be implemented here
    backups.clear();
}