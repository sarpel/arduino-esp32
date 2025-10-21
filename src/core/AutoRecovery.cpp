#include "AutoRecovery.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"
#include "../network/NetworkManager.h"

AutoRecovery::AutoRecovery() : recovery_attempts(0), successful_recoveries(0), last_recovery_time(0) {}

RecoveryStrategy AutoRecovery::getRecoveryStrategy(const String& component, const String& failure_reason) {
    if (component == "WiFi" || component == "Network") {
        return RecoveryStrategy::RECONNECT_WIFI;
    } else if (component == "TCP" || component == "Server") {
        return RecoveryStrategy::RECONNECT_TCP;
    } else if (component == "I2S" || component == "Audio") {
        return RecoveryStrategy::REINITIALIZE_I2S;
    } else if (failure_reason.indexOf("memory") >= 0) {
        return RecoveryStrategy::DEGRADE_MODE;
    }
    return RecoveryStrategy::DEGRADE_MODE;
}

bool AutoRecovery::executeRecovery(RecoveryStrategy strategy) {
    recovery_attempts++;
    last_recovery_time = millis();

    auto logger = SystemManager::getInstance().getLogger();
    bool success = false;

    switch (strategy) {
        case RecoveryStrategy::RECONNECT_WIFI:
            if (logger) logger->log(LogLevel::LOG_WARN, "AutoRecovery", __FILE__, __LINE__, "Attempting WiFi reconnection");
            success = recoverWiFi();
            break;

        case RecoveryStrategy::RECONNECT_TCP:
            if (logger) logger->log(LogLevel::LOG_WARN, "AutoRecovery", __FILE__, __LINE__, "Attempting TCP reconnection");
            success = recoverTCP();
            break;

        case RecoveryStrategy::REINITIALIZE_I2S:
            if (logger) logger->log(LogLevel::LOG_WARN, "AutoRecovery", __FILE__, __LINE__, "Attempting I2S reinitialization");
            success = recoverI2S();
            break;

        case RecoveryStrategy::DEGRADE_MODE:
            if (logger) logger->log(LogLevel::LOG_WARN, "AutoRecovery", __FILE__, __LINE__, "Triggering degradation mode");
            success = degrade();
            break;

        case RecoveryStrategy::RESET_DEVICE:
            if (logger) logger->log(LogLevel::LOG_ERROR, "AutoRecovery", __FILE__, __LINE__, "Device reset required");
            success = false;  // Don't actually reset for safety
            break;
    }

    if (success) {
        successful_recoveries++;
    }

    return success;
}

void AutoRecovery::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "AutoRecovery", __FILE__, __LINE__,
                "=== Recovery Statistics ===");
    logger->log(LogLevel::LOG_INFO, "AutoRecovery", __FILE__, __LINE__,
                "Total Attempts: %u", recovery_attempts);
    logger->log(LogLevel::LOG_INFO, "AutoRecovery", __FILE__, __LINE__,
                "Successful: %u", successful_recoveries);
    if (recovery_attempts > 0) {
        float success_rate = (successful_recoveries * 100.0f) / recovery_attempts;
        logger->log(LogLevel::LOG_INFO, "AutoRecovery", __FILE__, __LINE__,
                    "Success Rate: %.1f%%", success_rate);
    }
    logger->log(LogLevel::LOG_INFO, "AutoRecovery", __FILE__, __LINE__,
                "==========================");
}

bool AutoRecovery::recoverWiFi() {
    auto netmgr = SystemManager::getInstance().getNetworkManager();
    if (netmgr) {
        // Attempt to reconnect to WiFi
        netmgr->handleWiFiConnection();
        return netmgr->isWiFiConnected();
    }
    return false;
}

bool AutoRecovery::recoverTCP() {
    auto netmgr = SystemManager::getInstance().getNetworkManager();
    if (netmgr && netmgr->isWiFiConnected()) {
        return netmgr->connectToServer();
    }
    return false;
}

bool AutoRecovery::recoverI2S() {
    // I2S recovery would require audio subsystem interaction
    // For now, just log the attempt
    return true;
}

bool AutoRecovery::degrade() {
    // Triggering degradation is handled by DegradationManager
    // AutoRecovery just signals the need
    return true;
}
