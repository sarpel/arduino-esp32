#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include <Arduino.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include "../core/SystemManager.h"

// OTA update states
enum class OTAState {
    IDLE = 0,
    CHECKING_FOR_UPDATE = 1,
    DOWNLOADING_UPDATE = 2,
    VERIFYING_UPDATE = 3,
    APPLYING_UPDATE = 4,
    REBOOTING = 5,
    ERROR = 6,
    COMPLETED = 7
};

// OTA update configuration
struct OTAConfig {
    String update_server_url;
    String current_version;
    String device_id;
    bool enable_auto_check;
    bool enable_auto_download;
    bool enable_auto_install;
    uint32_t check_interval_ms;
    uint32_t download_timeout_ms;
    bool verify_signature;
    bool allow_downgrade;
    
    OTAConfig() : enable_auto_check(true), enable_auto_download(false),
                  enable_auto_install(false), check_interval_ms(3600000),  // 1 hour
                  download_timeout_ms(300000), verify_signature(true),    // 5 minutes
                  allow_downgrade(false) {}
};

// Update information
struct UpdateInfo {
    String version;
    String description;
    String download_url;
    size_t size;
    String checksum;
    String signature;
    String release_date;
    bool mandatory;
    
    UpdateInfo() : size(0), mandatory(false) {}
};

// OTA update progress
struct OTAProgress {
    OTAState state;
    size_t total_size;
    size_t downloaded_size;
    uint8_t progress_percent;
    String current_action;
    String error_message;
    unsigned long start_time;
    unsigned long estimated_time_remaining;
    
    OTAProgress() : state(OTAState::IDLE), total_size(0), downloaded_size(0),
                   progress_percent(0), start_time(0), estimated_time_remaining(0) {}
};

// Update validation result
struct ValidationResult {
    bool valid;
    String error_message;
    String warning_message;
    
    ValidationResult(bool v = true) : valid(v) {}
};

class OTAUpdater {
private:
    // Configuration
    OTAConfig config;
    
    // Current state
    OTAState current_state;
    OTAProgress current_progress;
    UpdateInfo available_update;
    
    // Network clients
    WiFiClient* http_client;
    WiFiClientSecure* https_client;
    
    // State tracking
    bool initialized;
    bool update_in_progress;
    unsigned long last_check_time;
    unsigned long last_progress_update;
    
    // Statistics
    uint32_t total_checks;
    uint32_t updates_found;
    uint32_t updates_downloaded;
    uint32_t updates_applied;
    uint32_t update_failures;
    
    // Callbacks
    std::function<void(const OTAProgress&)> progress_callback;
    std::function<void(const String&)> status_callback;
    std::function<void(const ValidationResult&)> validation_callback;
    
    // Internal methods
    bool checkForUpdateHTTP();
    bool checkForUpdateHTTPS();
    bool downloadUpdate();
    ValidationResult validateUpdate();
    bool applyUpdate();
    bool verifySignature();
    bool verifyChecksum();
    void updateProgress(OTAState state, const String& action, size_t current = 0, size_t total = 0);
    void reportError(const String& error);
    void reportStatus(const String& status);
    bool isUpdateCompatible(const UpdateInfo& info);
    bool canInstallUpdate(const UpdateInfo& info);
    String calculateChecksum(const uint8_t* data, size_t size);
    bool compareVersions(const String& v1, const String& v2);
    
public:
    OTAUpdater();
    ~OTAUpdater();
    
    // Lifecycle
    bool initialize(const OTAConfig& cfg);
    void shutdown();
    bool isInitialized() const { return initialized; }
    bool isUpdateInProgress() const { return update_in_progress; }
    
    // Update management
    bool checkForUpdate();
    bool downloadUpdate();
    bool installUpdate();
    bool performFullUpdate();
    
    // Update information
    bool isUpdateAvailable() const;
    const UpdateInfo& getAvailableUpdate() const { return available_update; }
    const OTAProgress& getProgress() const { return current_progress; }
    OTAState getCurrentState() const { return current_state; }
    String getStateString() const;
    
    // Auto-update
    void enableAutoCheck(bool enable) { config.enable_auto_check = enable; }
    void enableAutoDownload(bool enable) { config.enable_auto_download = enable; }
    void enableAutoInstall(bool enable) { config.enable_auto_install = enable; }
    void setCheckInterval(uint32_t interval_ms) { config.check_interval_ms = interval_ms; }
    void handleAutoUpdate();
    
    // Callbacks
    void setProgressCallback(std::function<void(const OTAProgress&)> callback) {
        progress_callback = callback;
    }
    void setStatusCallback(std::function<void(const String&)> callback) {
        status_callback = callback;
    }
    void setValidationCallback(std::function<void(const ValidationResult&)> callback) {
        validation_callback = callback;
    }
    
    // Validation
    ValidationResult validateUpdateFile(const String& file_path);
    ValidationResult validateUpdateData(const uint8_t* data, size_t size);
    void enableSignatureVerification(bool enable) { config.verify_signature = enable; }
    void enableDowngrade(bool enable) { config.allow_downgrade = enable; }
    
    // Statistics
    uint32_t getTotalChecks() const { return total_checks; }
    uint32_t getUpdatesFound() const { return updates_found; }
    uint32_t getUpdatesDownloaded() const { return updates_downloaded; }
    uint32_t getUpdatesApplied() const { return updates_applied; }
    uint32_t getUpdateFailures() const { return update_failures; }
    
    // Utility
    void printUpdateInfo() const;
    void printStatistics() const;
    void resetStatistics();
    bool cancelUpdate();
    bool isUpdateMandatory() const;
    String getCurrentVersion() const { return config.current_version; }
    void setCurrentVersion(const String& version) { config.current_version = version; }
    
    // Advanced features
    bool backupCurrentFirmware(const String& backup_name);
    bool restoreFirmware(const String& backup_name);
    bool downloadUpdateToFile(const String& file_path);
    bool installUpdateFromFile(const String& file_path);
    void listBackups(std::vector<String>& backups) const;
};

// Global OTA updater access
#define OTA_UPDATER() (SystemManager::getInstance().getOTAUpdater())

#endif // OTA_UPDATER_H