#include "OTAUpdater.h"
#include "../core/SystemManager.h"
#include "EnhancedLogger.h"
#include <vector>

OTAUpdater::OTAUpdater()
    : http_client(nullptr), https_client(nullptr), initialized(false),
      update_in_progress(false), last_check_time(0), last_progress_update(0),
      total_checks(0), updates_found(0), updates_downloaded(0),
      updates_applied(0), update_failures(0) {
    
    current_state = OTAState::IDLE;
    current_progress = OTAProgress();
}

OTAUpdater::~OTAUpdater() {
    shutdown();
}

bool OTAUpdater::initialize(const OTAConfig& cfg) {
    if (initialized) {
        return true;
    }
    
    config = cfg;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Initializing OTAUpdater");
    }
    
    // Initialize network clients
    http_client = new WiFiClient();
    https_client = new WiFiClientSecure();
    
    // Configure HTTPS client
    https_client->setInsecure();  // For testing - in production, use proper certificates
    
    // Set default callbacks
    setStatusCallback([this](const String& status) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->info( "OTAUpdater", "%s", status.c_str());
        }
    });
    
    setProgressCallback([this](const OTAProgress& progress) {
        if (progress.progress_percent % 10 == 0) {
            auto logger = SystemManager::getInstance().getLogger();
            if (logger) {
                logger->info( "OTAUpdater", "Update progress: %u%% - %s",
                           progress.progress_percent, progress.current_action.c_str());
            }
        }
    });
    
    initialized = true;
    
    if (logger) {
        logger->info( "OTAUpdater", "OTAUpdater initialized - version: %s",
                   config.current_version.c_str());
    }
    
    return true;
}

void OTAUpdater::shutdown() {
    if (!initialized) {
        return;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Shutting down OTAUpdater");
        printStatistics();
    }
    
    // Cancel any in-progress update
    if (update_in_progress) {
        cancelUpdate();
    }
    
    // Clean up network clients
    if (http_client) {
        delete http_client;
        http_client = nullptr;
    }
    
    if (https_client) {
        delete https_client;
        https_client = nullptr;
    }
    
    initialized = false;
}

bool OTAUpdater::checkForUpdate() {
    if (!initialized || update_in_progress) {
        return false;
    }
    
    total_checks++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Checking for updates");
    }
    
    updateProgress(OTAState::CHECKING_FOR_UPDATE, "Checking for available updates");
    
    // Check if update server uses HTTPS
    bool use_https = config.update_server_url.startsWith("https://");
    bool result = false;
    
    if (use_https) {
        result = checkForUpdateHTTPS();
    } else {
        result = checkForUpdateHTTP();
    }
    
    if (result) {
        updates_found++;
        
        if (logger) {
            logger->info( "OTAUpdater", "Update available: version %s, size: %u bytes",
                       available_update.version.c_str(), available_update.size);
        }
        
        updateProgress(OTAState::IDLE, "Update available");
    } else {
        if (logger) {
            logger->info( "OTAUpdater", "No updates available");
        }
        
        updateProgress(OTAState::IDLE, "No updates available");
    }
    
    last_check_time = millis();
    return result;
}

bool OTAUpdater::checkForUpdateHTTP() {
    // Simplified HTTP update check
    // In a real implementation, this would make an HTTP request to the update server
    
    // Simulate finding an update (for demonstration)
    if (random(100) < 10) {  // 10% chance of finding an update
        available_update.version = "3.1.0";
        available_update.description = "Bug fixes and performance improvements";
        available_update.download_url = config.update_server_url + "/firmware.bin";
        available_update.size = 500000;  // 500KB
        available_update.release_date = "2025-10-21";
        available_update.mandatory = false;
        
        return true;
    }
    
    return false;
}

bool OTAUpdater::checkForUpdateHTTPS() {
    // Simplified HTTPS update check
    // Similar to HTTP but using secure connection
    return checkForUpdateHTTP();  // For now, use same logic
}

bool OTAUpdater::downloadUpdate() {
    if (!initialized || update_in_progress || !isUpdateAvailable()) {
        return false;
    }
    
    update_in_progress = true;
    updates_downloaded++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Starting update download");
    }
    
    updateProgress(OTAState::DOWNLOADING_UPDATE, "Downloading update", 0, available_update.size);
    
    // Simulate download progress
    size_t chunk_size = 4096;
    size_t downloaded = 0;
    
    while (downloaded < available_update.size) {
        size_t current_chunk = std::min(chunk_size, available_update.size - downloaded);
        
        // Simulate downloading data
        // In a real implementation, this would download actual firmware data
        
        downloaded += current_chunk;
        
        // Update progress
        uint8_t progress = (downloaded * 100) / available_update.size;
        updateProgress(OTAState::DOWNLOADING_UPDATE, "Downloading update", downloaded, available_update.size);
        
        // Simulate network delay
        delay(100);
        
        // Check for cancellation
        if (!update_in_progress) {
            return false;
        }
    }
    
    if (logger) {
        logger->info( "OTAUpdater", "Update download completed");
    }
    
    updateProgress(OTAState::IDLE, "Download completed");
    update_in_progress = false;
    
    return true;
}

bool OTAUpdater::installUpdate() {
    if (!initialized || update_in_progress) {
        return false;
    }
    
    update_in_progress = true;
    updates_applied++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Starting update installation");
    }
    
    updateProgress(OTAState::VERIFYING_UPDATE, "Verifying update");
    
    // Validate the update
    ValidationResult validation = validateUpdate();
    if (!validation.valid) {
        update_failures++;
        reportError("Update validation failed: " + validation.error_message);
        updateProgress(OTAState::ERROR, "Update validation failed");
        update_in_progress = false;
        return false;
    }
    
    if (!validation.warning_message.isEmpty()) {
        reportStatus("Update validation warning: " + validation.warning_message);
    }
    
    updateProgress(OTAState::APPLYING_UPDATE, "Applying update");
    
    // Apply the update
    bool result = applyUpdate();
    
    if (result) {
        if (logger) {
            logger->info( "OTAUpdater", "Update applied successfully");
        }
        
        updateProgress(OTAState::COMPLETED, "Update completed successfully");
        
        // Update current version
        config.current_version = available_update.version;
        
        // Clear available update
        available_update = UpdateInfo();
    } else {
        update_failures++;
        
        if (logger) {
            logger->error( "OTAUpdater", "Update installation failed");
        }
        
        updateProgress(OTAState::ERROR, "Update installation failed");
    }
    
    update_in_progress = false;
    return result;
}

bool OTAUpdater::performFullUpdate() {
    if (!checkForUpdate()) {
        return false;
    }
    
    if (!downloadUpdate()) {
        return false;
    }
    
    return installUpdate();
}

bool OTAUpdater::isUpdateAvailable() const {
    return !available_update.version.isEmpty() && 
           available_update.version != config.current_version;
}

ValidationResult OTAUpdater::validateUpdate() {
    ValidationResult result;
    
    // Check version compatibility
    if (!isUpdateCompatible(available_update)) {
        result.valid = false;
        result.error_message = "Update version is not compatible";
        return result;
    }
    
    // Check if downgrade is allowed
    if (!config.allow_downgrade && 
        compareVersions(available_update.version, config.current_version) < 0) {
        result.valid = false;
        result.error_message = "Downgrade not allowed";
        return result;
    }
    
    // Verify signature if enabled
    if (config.verify_signature) {
        if (!verifySignature()) {
            result.valid = false;
            result.error_message = "Signature verification failed";
            return result;
        }
    }
    
    // Verify checksum
    if (!verifyChecksum()) {
        result.valid = false;
        result.error_message = "Checksum verification failed";
        return result;
    }
    
    // Check if update is mandatory
    if (available_update.mandatory) {
        result.warning_message = "This is a mandatory update";
    }
    
    // Call user validation callback
    if (validation_callback) {
        validation_callback(result);
    }
    
    return result;
}

bool OTAUpdater::applyUpdate() {
    // Simulate applying update
    // In a real implementation, this would use the Arduino Update library
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Applying firmware update");
    }
    
    // Simulate update process
    for (int i = 0; i <= 100; i += 10) {
        updateProgress(OTAState::APPLYING_UPDATE, "Applying update", i * available_update.size / 100, available_update.size);
        delay(200);
    }
    
    // Simulate reboot
    updateProgress(OTAState::REBOOTING, "System will reboot to complete update");
    
    return true;
}

bool OTAUpdater::verifySignature() {
    // Signature verification would be implemented here
    // For now, return true (simulated success)
    return true;
}

bool OTAUpdater::verifyChecksum() {
    // Checksum verification would be implemented here
    // For now, return true (simulated success)
    return true;
}

void OTAUpdater::updateProgress(OTAState state, const String& action, size_t current, size_t total) {
    current_state = state;
    current_progress.current_action = action;
    current_progress.state = state;
    
    if (total > 0) {
        current_progress.downloaded_size = current;
        current_progress.total_size = total;
        current_progress.progress_percent = (current * 100) / total;
    }

    // Calculate estimated time remaining (simplified)
    if (current > 0 && total > 0) {
        unsigned long elapsed = millis() - current_progress.start_time;
        if (elapsed > 0) {
            float rate = static_cast<float>(current) / elapsed;  // bytes per ms
            size_t remaining = total - current;
            current_progress.estimated_time_remaining = remaining / rate;
        }
    }
    
    // Call progress callback
    if (progress_callback) {
        progress_callback(current_progress);
    }
}

void OTAUpdater::reportError(const String& error) {
    current_progress.error_message = error;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->error( "OTAUpdater", "%s", error.c_str());
    }
    
    if (status_callback) {
        status_callback("Error: " + error);
    }
}

void OTAUpdater::reportStatus(const String& status) {
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "%s", status.c_str());
    }
    
    if (status_callback) {
        status_callback(status);
    }
}

bool OTAUpdater::isUpdateCompatible(const UpdateInfo& info) {
    // Check if update is compatible with current hardware/software
    // For now, assume all updates are compatible
    return true;
}

bool OTAUpdater::canInstallUpdate(const UpdateInfo& info) {
    // Check if we can install this update
    // Consider battery level, network stability, etc.
    return true;
}

String OTAUpdater::calculateChecksum(const uint8_t* data, size_t size) {
    // Simple checksum calculation
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum += data[i];
        checksum = (checksum << 1) | (checksum >> 31);  // Rotate
    }
    return String(checksum, HEX);
}

bool OTAUpdater::compareVersions(const String& v1, const String& v2) {
    // Simple version comparison
    // Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
    
    if (v1 == v2) return 0;
    
    // For simplicity, just compare as strings
    // In a real implementation, this would parse semantic versions
    return v1 < v2 ? -1 : 1;
}

void OTAUpdater::handleAutoUpdate() {
    if (!initialized || !config.enable_auto_check) {
        return;
    }
    
    unsigned long current_time = millis();
    if (current_time - last_check_time < config.check_interval_ms) {
        return;
    }
    
    if (checkForUpdate() && config.enable_auto_download) {
        if (downloadUpdate() && config.enable_auto_install) {
            installUpdate();
        }
    }
}

bool OTAUpdater::cancelUpdate() {
    if (!update_in_progress) {
        return false;
    }
    
    update_in_progress = false;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Update cancelled");
    }
    
    updateProgress(OTAState::IDLE, "Update cancelled");
    return true;
}

bool OTAUpdater::isUpdateMandatory() const {
    return available_update.mandatory;
}

void OTAUpdater::printUpdateInfo() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->info( "OTAUpdater", "=== Update Information ===");
    logger->info( "OTAUpdater", "Current version: %s", config.current_version.c_str());
    
    if (isUpdateAvailable()) {
        logger->info( "OTAUpdater", "Available version: %s", available_update.version.c_str());
        logger->info( "OTAUpdater", "Description: %s", available_update.description.c_str());
        logger->info( "OTAUpdater", "Size: %u bytes", available_update.size);
        logger->info( "OTAUpdater", "Release date: %s", available_update.release_date.c_str());
        logger->info( "OTAUpdater", "Mandatory: %s", available_update.mandatory ? "yes" : "no");
    } else {
        logger->info( "OTAUpdater", "No updates available");
    }
    
    logger->info( "OTAUpdater", "==========================");
}

void OTAUpdater::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->info( "OTAUpdater", "=== OTA Update Statistics ===");
    logger->info( "OTAUpdater", "Total checks: %u", total_checks);
    logger->info( "OTAUpdater", "Updates found: %u", updates_found);
    logger->info( "OTAUpdater", "Updates downloaded: %u", updates_downloaded);
    logger->info( "OTAUpdater", "Updates applied: %u", updates_applied);
    logger->info( "OTAUpdater", "Update failures: %u", update_failures);
    logger->info( "OTAUpdater", "Success rate: %.1f%%",
               total_checks > 0 ? (static_cast<float>(updates_applied) / total_checks) * 100.0f : 0.0f);
    logger->info( "OTAUpdater", "============================");
}

void OTAUpdater::resetStatistics() {
    total_checks = 0;
    updates_found = 0;
    updates_downloaded = 0;
    updates_applied = 0;
    update_failures = 0;
}

String OTAUpdater::getStateString() const {
    switch (current_state) {
        case OTAState::IDLE: return "IDLE";
        case OTAState::CHECKING_FOR_UPDATE: return "CHECKING_FOR_UPDATE";
        case OTAState::DOWNLOADING_UPDATE: return "DOWNLOADING_UPDATE";
        case OTAState::VERIFYING_UPDATE: return "VERIFYING_UPDATE";
        case OTAState::APPLYING_UPDATE: return "APPLYING_UPDATE";
        case OTAState::REBOOTING: return "REBOOTING";
        case OTAState::ERROR: return "ERROR";
        case OTAState::COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}

bool OTAUpdater::backupCurrentFirmware(const String& backup_name) {
    // Firmware backup implementation would go here
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Creating firmware backup: %s", backup_name.c_str());
    }
    return true;
}

bool OTAUpdater::restoreFirmware(const String& backup_name) {
    // Firmware restore implementation would go here
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Restoring firmware from backup: %s", backup_name.c_str());
    }
    return true;
}

bool OTAUpdater::downloadUpdateToFile(const String& file_path) {
    // Download update to file implementation would go here
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Downloading update to file: %s", file_path.c_str());
    }
    return true;
}

bool OTAUpdater::installUpdateFromFile(const String& file_path) {
    // Install update from file implementation would go here
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->info( "OTAUpdater", "Installing update from file: %s", file_path.c_str());
    }
    return true;
}

void OTAUpdater::listBackups(std::vector<String>& backups) const {
    // List firmware backups implementation would go here
    backups.clear();
}