#include "DegradationManager.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"

DegradationManager::DegradationManager()
    : current_mode(DegradationMode::NORMAL), previous_mode(DegradationMode::NORMAL),
      last_mode_change(millis()), mode_transition_delay(5000),
      health_degrade_threshold(80), health_safe_threshold(60),
      health_recovery_threshold(3), health_restore_threshold(85),
      consecutive_failures(0) {}

void DegradationManager::updateMode(int current_health_score) {
    unsigned long current_time = millis();

    // Only allow transitions every mode_transition_delay ms (5s)
    if (current_time - last_mode_change < mode_transition_delay) {
        return;
    }

    DegradationMode new_mode = current_mode;

    // Determine target mode based on health score
    if (current_health_score < health_safe_threshold) {
        new_mode = DegradationMode::SAFE_MODE;
    } else if (current_health_score < health_degrade_threshold) {
        if (current_mode == DegradationMode::NORMAL) {
            new_mode = DegradationMode::REDUCED_QUALITY;
        }
    } else if (current_health_score > health_restore_threshold) {
        // Allow gradual recovery
        if (current_mode == DegradationMode::REDUCED_QUALITY) {
            new_mode = DegradationMode::NORMAL;
        }
    }

    // Only transition if mode changed
    if (new_mode != current_mode && shouldTransition(current_health_score, current_mode, new_mode)) {
        setMode(new_mode);
    }
}

void DegradationManager::setMode(DegradationMode new_mode) {
    if (new_mode == current_mode) {
        return;
    }

    previous_mode = current_mode;
    current_mode = new_mode;
    last_mode_change = millis();

    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_WARN, "DegradationManager", __FILE__, __LINE__,
                    "Mode transition: %s → %s",
                    modeToString(previous_mode).c_str(), modeToString(current_mode).c_str());
    }

    // TODO: Publish mode change event to EventBus when available
    // (Deferred to avoid circular dependency)
}

void DegradationManager::recordFailure() {
    consecutive_failures++;
    if (consecutive_failures >= 3) {
        setMode(DegradationMode::RECOVERY);
    }
}

void DegradationManager::recordSuccess() {
    if (consecutive_failures > 0) {
        consecutive_failures--;
    }
}

bool DegradationManager::isFeatureEnabled(const String& feature_name) const {
    if (feature_name == "audio_streaming") {
        return current_mode != DegradationMode::RECOVERY;
    } else if (feature_name == "high_quality_audio") {
        return current_mode == DegradationMode::NORMAL;
    } else if (feature_name == "telemetry") {
        return current_mode != DegradationMode::RECOVERY;
    } else if (feature_name == "advanced_monitoring") {
        return current_mode == DegradationMode::NORMAL || current_mode == DegradationMode::REDUCED_QUALITY;
    }
    return true;  // Default: enabled
}

void DegradationManager::setHealthThresholds(int degrade, int safe_mode, int recovery, int restore) {
    health_degrade_threshold = degrade;
    health_safe_threshold = safe_mode;
    health_recovery_threshold = recovery;
    health_restore_threshold = restore;
}

void DegradationManager::printStatus() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "DegradationManager", __FILE__, __LINE__,
                "=== Degradation Status ===");
    logger->log(LogLevel::LOG_INFO, "DegradationManager", __FILE__, __LINE__,
                "Current Mode: %s", modeToString(current_mode).c_str());
    logger->log(LogLevel::LOG_INFO, "DegradationManager", __FILE__, __LINE__,
                "Consecutive Failures: %u", consecutive_failures);
    logger->log(LogLevel::LOG_INFO, "DegradationManager", __FILE__, __LINE__,
                "===========================");
}

bool DegradationManager::shouldTransition(int health_score, DegradationMode from, DegradationMode to) {
    // Hysteresis to prevent oscillation
    if (from == DegradationMode::NORMAL && to == DegradationMode::REDUCED_QUALITY) {
        return health_score < health_degrade_threshold - 5;  // Hysteresis: 5% lower
    }
    if (from == DegradationMode::REDUCED_QUALITY && to == DegradationMode::NORMAL) {
        return health_score > health_degrade_threshold + 5;  // Hysteresis: 5% higher
    }
    return true;  // Always allow other transitions
}

String DegradationManager::modeToString(DegradationMode mode) const {
    switch (mode) {
        case DegradationMode::NORMAL:
            return "NORMAL";
        case DegradationMode::REDUCED_QUALITY:
            return "REDUCED_QUALITY";
        case DegradationMode::SAFE_MODE:
            return "SAFE_MODE";
        case DegradationMode::RECOVERY:
            return "RECOVERY";
        default:
            return "UNKNOWN";
    }
}
