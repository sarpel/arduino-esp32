#include "AdaptiveReconnection.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"
#include <cmath>

AdaptiveReconnection::AdaptiveReconnection()
    : consecutive_failures(0), last_reset_time(millis()),
      current_backoff_ms(BASE_BACKOFF_MS), next_attempt_time(0) {}

void AdaptiveReconnection::recordConnectionAttempt(const String& ssid, bool success) {
    updateNetworkStats(ssid, success);

    if (success) {
        consecutive_failures = 0;
        current_backoff_ms = BASE_BACKOFF_MS;
        last_attempt_time[ssid] = millis();

        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_DEBUG, "AdaptiveReconnection", __FILE__, __LINE__,
                        "Connection successful to %s, backoff reset", ssid.c_str());
        }
    } else {
        consecutive_failures++;
        current_backoff_ms = calculateBackoffDelay(consecutive_failures);

        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_WARN, "AdaptiveReconnection", __FILE__, __LINE__,
                        "Connection failed to %s, next retry in %lums", ssid.c_str(), current_backoff_ms);
        }
    }

    next_attempt_time = millis() + current_backoff_ms;
}

unsigned long AdaptiveReconnection::getNextRetryDelay() {
    if (millis() < next_attempt_time) {
        return next_attempt_time - millis();
    }
    return 0;  // Ready to retry now
}

bool AdaptiveReconnection::shouldRetryNow() {
    return millis() >= next_attempt_time;
}

void AdaptiveReconnection::resetBackoffForNetwork(const String& ssid) {
    consecutive_failures = 0;
    current_backoff_ms = BASE_BACKOFF_MS;
    next_attempt_time = millis();

    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "AdaptiveReconnection", __FILE__, __LINE__,
                    "Backoff reset for network: %s", ssid.c_str());
    }
}

unsigned long AdaptiveReconnection::getNetworkSpecificDelay(const String& ssid) const {
    float success_rate = getNetworkSuccessRate(ssid);

    // Known-good networks get faster retry
    if (success_rate > 0.9f) {
        return BASE_BACKOFF_MS + addJitter(BASE_BACKOFF_MS);  // Fast retry
    }
    // Average networks
    else if (success_rate > 0.5f) {
        return calculateBackoffDelay(2) + addJitter(BASE_BACKOFF_MS);
    }
    // Unreliable networks
    else {
        return calculateBackoffDelay(5) + addJitter(BASE_BACKOFF_MS * 2);
    }
}

float AdaptiveReconnection::getNetworkSuccessRate(const String& ssid) const {
    for (const auto& record : network_history) {
        if (record.network_ssid == ssid) {
            return record.success_rate;
        }
    }
    return 0.5f;  // Default: assume 50% success for unknown networks
}

void AdaptiveReconnection::reset() {
    consecutive_failures = 0;
    current_backoff_ms = BASE_BACKOFF_MS;
    next_attempt_time = 0;
    last_reset_time = millis();
}

void AdaptiveReconnection::clearHistory() {
    network_history.clear();
    last_attempt_time.clear();
    reset();
}

void AdaptiveReconnection::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "AdaptiveReconnection", __FILE__, __LINE__,
                "=== Adaptive Reconnection Statistics ===");
    logger->log(LogLevel::LOG_INFO, "AdaptiveReconnection", __FILE__, __LINE__,
                "Consecutive Failures: %u", consecutive_failures);
    logger->log(LogLevel::LOG_INFO, "AdaptiveReconnection", __FILE__, __LINE__,
                "Current Backoff: %lu ms", current_backoff_ms);

    for (const auto& record : network_history) {
        float rate_pct = record.success_rate * 100.0f;
        logger->log(LogLevel::LOG_INFO, "AdaptiveReconnection", __FILE__, __LINE__,
                    "Network '%s': Success Rate=%.1f%%, Successes=%u, Failures=%u",
                    record.network_ssid.c_str(), rate_pct, record.success_count, record.failure_count);
    }
    logger->log(LogLevel::LOG_INFO, "AdaptiveReconnection", __FILE__, __LINE__,
                "========================================");
}

float AdaptiveReconnection::calculateSuccessRate(const String& ssid) const {
    for (const auto& record : network_history) {
        if (record.network_ssid == ssid) {
            uint32_t total = record.success_count + record.failure_count;
            if (total == 0) {
                return 1.0f;
            }
            return static_cast<float>(record.success_count) / total;
        }
    }
    return 0.5f;
}

unsigned long AdaptiveReconnection::calculateBackoffDelay(uint32_t failure_count) const {
    // Exponential backoff with cap: BASE * 2^failures, max MAX_BACKOFF
    unsigned long delay = BASE_BACKOFF_MS;
    for (uint32_t i = 0; i < failure_count && delay < MAX_BACKOFF_MS / 2; i++) {
        delay *= 2;
    }
    return std::min(delay, MAX_BACKOFF_MS);
}

void AdaptiveReconnection::updateNetworkStats(const String& ssid, bool success) {
    auto* record = findNetworkRecord(ssid);
    if (!record) {
        network_history.emplace_back(ssid);
        record = &network_history.back();
    }

    if (success) {
        record->success_count++;
        record->last_success_time = millis();
    } else {
        record->failure_count++;
    }

    // Update success rate
    uint32_t total = record->success_count + record->failure_count;
    if (total > 0) {
        record->success_rate = static_cast<float>(record->success_count) / total;
    }
}

NetworkSuccessRecord* AdaptiveReconnection::findNetworkRecord(const String& ssid) {
    for (auto& record : network_history) {
        if (record.network_ssid == ssid) {
            return &record;
        }
    }
    return nullptr;
}

unsigned long AdaptiveReconnection::addJitter(unsigned long base_delay) const {
    // Add random jitter of ±20% to prevent thundering herd
    int jitter_percent = (rand() % 41) - 20;  // -20 to +20
    long jitter_ms = (base_delay * jitter_percent) / 100;
    return base_delay + jitter_ms;
}
