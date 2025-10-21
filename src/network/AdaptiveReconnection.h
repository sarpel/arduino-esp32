#ifndef ADAPTIVE_RECONNECTION_H
#define ADAPTIVE_RECONNECTION_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "../config.h"

// Network success history entry
struct NetworkSuccessRecord {
    String network_ssid;
    unsigned long last_success_time;
    uint32_t success_count;
    uint32_t failure_count;
    float success_rate;  // 0-1.0

    NetworkSuccessRecord(const String& ssid)
        : network_ssid(ssid), last_success_time(0), success_count(0),
          failure_count(0), success_rate(1.0f) {}
};

// Adaptive reconnection strategy with learning
class AdaptiveReconnection {
private:
    static constexpr unsigned long BASE_BACKOFF_MS = 1000;
    static constexpr unsigned long MAX_BACKOFF_MS = 60000;
    static constexpr unsigned long HISTORY_WINDOW = 86400000;  // 24 hours in ms

    std::vector<NetworkSuccessRecord> network_history;
    std::map<String, unsigned long> last_attempt_time;
    uint32_t consecutive_failures;
    unsigned long last_reset_time;

    // Exponential backoff state
    unsigned long current_backoff_ms;
    unsigned long next_attempt_time;

    float calculateSuccessRate(const String& ssid) const;
    unsigned long calculateBackoffDelay(uint32_t failure_count) const;
    void updateNetworkStats(const String& ssid, bool success);
    unsigned long addJitter(unsigned long base_delay) const;

public:
    AdaptiveReconnection();

    // Record connection attempts
    void recordConnectionAttempt(const String& ssid, bool success);

    // Get next retry delay (with exponential backoff and jitter)
    unsigned long getNextRetryDelay();

    // Check if should attempt reconnection
    bool shouldRetryNow();

    // Reset backoff for known-good networks
    void resetBackoffForNetwork(const String& ssid);

    // Get network-specific connection strategy
    unsigned long getNetworkSpecificDelay(const String& ssid) const;

    // Statistics and analysis
    float getNetworkSuccessRate(const String& ssid) const;
    const std::vector<NetworkSuccessRecord>& getNetworkHistory() const { return network_history; }

    // Management
    void reset();
    void clearHistory();

    // Utility
    void printStatistics() const;

private:
    NetworkSuccessRecord* findNetworkRecord(const String& ssid);
};

#endif // ADAPTIVE_RECONNECTION_H
