#ifndef METRICS_TRACKER_H
#define METRICS_TRACKER_H

#include <Arduino.h>
#include "../config.h"

// Performance metrics tracking
struct PerformanceMetrics {
    unsigned long uptime_ms;
    unsigned long total_uptime_ms;
    uint32_t error_count;
    uint32_t recovered_errors;
    uint32_t fatal_errors;
    float availability_percent;
    unsigned long min_latency_ms;
    unsigned long max_latency_ms;
    unsigned long avg_latency_ms;
    uint32_t total_bytes_sent;
    uint32_t total_bytes_received;
    uint32_t errors_per_component[4];  // Network, Memory, Audio, System

    PerformanceMetrics()
        : uptime_ms(0), total_uptime_ms(0), error_count(0), recovered_errors(0),
          fatal_errors(0), availability_percent(100.0f), min_latency_ms(0),
          max_latency_ms(0), avg_latency_ms(0), total_bytes_sent(0),
          total_bytes_received(0) {
        memset(errors_per_component, 0, sizeof(errors_per_component));
    }
};

// KPI tracking and metrics
class MetricsTracker {
private:
    PerformanceMetrics metrics;
    unsigned long startup_time;
    unsigned long last_update_time;
    uint32_t sample_count;

public:
    MetricsTracker();

    // Update tracking
    void updateUptime();
    void recordError(const String& component);
    void recordRecoveredError();
    void recordFatalError();
    void recordLatency(unsigned long latency_ms);
    void recordDataTransfer(uint32_t sent, uint32_t received);

    // Metrics queries
    const PerformanceMetrics& getMetrics() const { return metrics; }
    float getAvailability() const { return metrics.availability_percent; }
    unsigned long getUptime() const { return metrics.uptime_ms; }
    uint32_t getErrorCount() const { return metrics.error_count; }
    float getErrorRate() const;

    // Utility
    void printMetrics() const;
    void reset();
};

#endif // METRICS_TRACKER_H
