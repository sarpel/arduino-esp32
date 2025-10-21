#include "MetricsTracker.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"

MetricsTracker::MetricsTracker() : startup_time(millis()), last_update_time(millis()), sample_count(0) {}

void MetricsTracker::updateUptime() {
    metrics.uptime_ms = millis() - startup_time;
    metrics.total_uptime_ms += metrics.uptime_ms;

    // Update availability (assuming 99.5% target with downtime tracking)
    if (metrics.error_count > 0) {
        float downtime_estimate = (metrics.error_count * 100.0f) / (metrics.uptime_ms / 1000.0f);
        metrics.availability_percent = std::max(0.0f, 100.0f - downtime_estimate);
    }
}

void MetricsTracker::recordError(const String& component) {
    metrics.error_count++;

    if (component == "Network") {
        metrics.errors_per_component[0]++;
    } else if (component == "Memory") {
        metrics.errors_per_component[1]++;
    } else if (component == "Audio") {
        metrics.errors_per_component[2]++;
    } else if (component == "System") {
        metrics.errors_per_component[3]++;
    }
}

void MetricsTracker::recordRecoveredError() {
    if (metrics.error_count > 0) {
        metrics.recovered_errors++;
    }
}

void MetricsTracker::recordFatalError() {
    metrics.fatal_errors++;
}

void MetricsTracker::recordLatency(unsigned long latency_ms) {
    sample_count++;

    if (metrics.min_latency_ms == 0 || latency_ms < metrics.min_latency_ms) {
        metrics.min_latency_ms = latency_ms;
    }

    if (latency_ms > metrics.max_latency_ms) {
        metrics.max_latency_ms = latency_ms;
    }

    // Update running average
    if (metrics.avg_latency_ms == 0) {
        metrics.avg_latency_ms = latency_ms;
    } else {
        metrics.avg_latency_ms = (metrics.avg_latency_ms * 0.9f) + (latency_ms * 0.1f);
    }
}

void MetricsTracker::recordDataTransfer(uint32_t sent, uint32_t received) {
    metrics.total_bytes_sent += sent;
    metrics.total_bytes_received += received;
}

float MetricsTracker::getErrorRate() const {
    if (metrics.uptime_ms == 0) {
        return 0.0f;
    }

    float hours = metrics.uptime_ms / (3600000.0f);
    if (hours == 0) {
        return 0.0f;
    }

    return metrics.error_count / hours;  // Errors per hour
}

void MetricsTracker::printMetrics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "=== Performance Metrics ===");
    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "Uptime: %lu ms", metrics.uptime_ms);
    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "Availability: %.2f%%", metrics.availability_percent);
    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "Errors: %u (recovered: %u, fatal: %u)", metrics.error_count,
                metrics.recovered_errors, metrics.fatal_errors);
    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "Latency: min=%lu, avg=%lu, max=%lu ms",
                metrics.min_latency_ms, metrics.avg_latency_ms, metrics.max_latency_ms);
    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "Data: sent=%u, received=%u bytes",
                metrics.total_bytes_sent, metrics.total_bytes_received);
    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "Error Distribution: Net=%u, Mem=%u, Audio=%u, Sys=%u",
                metrics.errors_per_component[0], metrics.errors_per_component[1],
                metrics.errors_per_component[2], metrics.errors_per_component[3]);
    logger->log(LogLevel::LOG_INFO, "MetricsTracker", __FILE__, __LINE__,
                "==========================");
}

void MetricsTracker::reset() {
    metrics = PerformanceMetrics();
    startup_time = millis();
    sample_count = 0;
}
