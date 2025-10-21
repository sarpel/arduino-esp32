#ifndef CONFIG_VALIDATOR_H
#define CONFIG_VALIDATOR_H

#include "config.h"
#include "logger.h"
#include <string>

/**
 * Configuration Validator
 * Validates critical configuration values at runtime startup
 * Prevents system from starting with invalid or missing configurations
 */
class ConfigValidator {
public:
    /**
     * Validate all critical configuration parameters
     * @return true if all validations passed, false if critical validation failed
     */
    static bool validateAll() {
        bool all_valid = true;
        
        LOG_INFO("=== Starting Configuration Validation ===");
        
        // Validate WiFi configuration
        if (!validateWiFiConfig()) {
            all_valid = false;
        }
        
        // Validate server configuration
        if (!validateServerConfig()) {
            all_valid = false;
        }
        
        // Validate I2S configuration
        if (!validateI2SConfig()) {
            all_valid = false;
        }
        
        // Validate timing configuration
        if (!validateTimingConfig()) {
            all_valid = false;
        }
        
        // Validate memory thresholds
        if (!validateMemoryThresholds()) {
            all_valid = false;
        }

        // Validate watchdog configuration
        if (!validateWatchdogConfig()) {
            all_valid = false;
        }

        if (all_valid) {
            LOG_INFO("✓ All configuration validations passed");
        } else {
            LOG_CRITICAL("✗ Configuration validation FAILED - critical settings missing");
        }

        LOG_INFO("=== Configuration Validation Complete ===");

        return all_valid;
    }

private:
    /**
     * Validate WiFi configuration
     * Checks: SSID and password not empty
     */
    static bool validateWiFiConfig() {
        bool valid = true;
        
        LOG_INFO("Checking WiFi configuration...");
        
        // Check SSID
        if (strlen(WIFI_SSID) == 0) {
            LOG_ERROR("WiFi SSID is empty - must configure WIFI_SSID in config.h");
            valid = false;
        } else {
            LOG_INFO("  ✓ WiFi SSID configured");
        }
        
        // Check password
        if (strlen(WIFI_PASSWORD) == 0) {
            LOG_ERROR("WiFi password is empty - must configure WIFI_PASSWORD in config.h");
            valid = false;
        } else {
            LOG_INFO("  ✓ WiFi password configured");
        }
        
        // Validate retry parameters
        if (WIFI_RETRY_DELAY <= 0) {
            LOG_WARN("WIFI_RETRY_DELAY is 0 or negative - using default 500ms");
        } else {
            LOG_INFO("  ✓ WiFi retry delay: %u ms", WIFI_RETRY_DELAY);
        }
        
        if (WIFI_MAX_RETRIES <= 0) {
            LOG_WARN("WIFI_MAX_RETRIES is 0 or negative - using default 20");
        } else {
            LOG_INFO("  ✓ WiFi max retries: %u", WIFI_MAX_RETRIES);
        }
        
        if (WIFI_TIMEOUT <= 0) {
            LOG_WARN("WIFI_TIMEOUT is 0 or negative - using default 30000ms");
        } else {
            LOG_INFO("  ✓ WiFi timeout: %u ms", WIFI_TIMEOUT);
        }
        
        return valid;
    }

    /**
     * Validate server configuration
     * Checks: HOST and PORT not empty, valid port number
     */
    static bool validateServerConfig() {
        bool valid = true;
        
        LOG_INFO("Checking server configuration...");
        
        // Check HOST
        if (strlen(SERVER_HOST) == 0) {
            LOG_ERROR("Server HOST is empty - must configure SERVER_HOST in config.h");
            valid = false;
        } else {
            LOG_INFO("  ✓ Server HOST configured: %s", SERVER_HOST);
        }
        
        // Check PORT
        if (SERVER_PORT <= 0 || SERVER_PORT > 65535) {
            LOG_ERROR("Server PORT (%d) is invalid - must be 1-65535", SERVER_PORT);
            valid = false;
        } else {
            LOG_INFO("  ✓ Server PORT configured: %d", SERVER_PORT);
        }
        
        // Validate reconnection timeouts
        if (SERVER_RECONNECT_MIN <= 0) {
            LOG_WARN("SERVER_RECONNECT_MIN is %u ms - should be > 0", SERVER_RECONNECT_MIN);
        } else if (SERVER_RECONNECT_MIN < 1000) {
            LOG_WARN("SERVER_RECONNECT_MIN (%u ms) is very short - minimum recommended is 1000ms", SERVER_RECONNECT_MIN);
        } else {
            LOG_INFO("  ✓ Server reconnect min: %u ms", SERVER_RECONNECT_MIN);
        }
        
        if (SERVER_RECONNECT_MAX <= 0) {
            LOG_WARN("SERVER_RECONNECT_MAX is %u ms - should be > 0", SERVER_RECONNECT_MAX);
        } else if (SERVER_RECONNECT_MAX < SERVER_RECONNECT_MIN) {
            LOG_ERROR("SERVER_RECONNECT_MAX (%u ms) cannot be less than SERVER_RECONNECT_MIN (%u ms)",
                     SERVER_RECONNECT_MAX, SERVER_RECONNECT_MIN);
            valid = false;
        } else {
            LOG_INFO("  ✓ Server reconnect max: %u ms", SERVER_RECONNECT_MAX);
        }
        
        if (TCP_WRITE_TIMEOUT <= 0) {
            LOG_WARN("TCP_WRITE_TIMEOUT is %u ms - should be > 0", TCP_WRITE_TIMEOUT);
        } else if (TCP_WRITE_TIMEOUT < 1000) {
            LOG_WARN("TCP_WRITE_TIMEOUT (%u ms) is very short", TCP_WRITE_TIMEOUT);
        } else {
            LOG_INFO("  ✓ TCP write timeout: %u ms", TCP_WRITE_TIMEOUT);
        }
        
        return valid;
    }

    /**
     * Validate I2S configuration
     * Checks: Valid sample rate, buffer sizes, DMA parameters
     */
    static bool validateI2SConfig() {
        bool valid = true;
        
        LOG_INFO("Checking I2S configuration...");
        
        if (I2S_SAMPLE_RATE <= 0) {
            LOG_ERROR("I2S_SAMPLE_RATE must be > 0, got %u", I2S_SAMPLE_RATE);
            valid = false;
        } else if (I2S_SAMPLE_RATE < 8000 || I2S_SAMPLE_RATE > 48000) {
            LOG_WARN("I2S_SAMPLE_RATE (%u Hz) outside typical range (8000-48000)", I2S_SAMPLE_RATE);
        } else {
            LOG_INFO("  ✓ I2S sample rate: %u Hz", I2S_SAMPLE_RATE);
        }
        
        if (I2S_BUFFER_SIZE <= 0) {
            LOG_ERROR("I2S_BUFFER_SIZE must be > 0, got %u", I2S_BUFFER_SIZE);
            valid = false;
        } else if ((I2S_BUFFER_SIZE & (I2S_BUFFER_SIZE - 1)) != 0) {
            LOG_WARN("I2S_BUFFER_SIZE (%u) is not a power of 2", I2S_BUFFER_SIZE);
        } else {
            LOG_INFO("  ✓ I2S buffer size: %u bytes", I2S_BUFFER_SIZE);
        }
        
        if (I2S_DMA_BUF_COUNT <= 0) {
            LOG_ERROR("I2S_DMA_BUF_COUNT must be > 0, got %u", I2S_DMA_BUF_COUNT);
            valid = false;
        } else if (I2S_DMA_BUF_COUNT < 2) {
            LOG_WARN("I2S_DMA_BUF_COUNT (%u) should be >= 2", I2S_DMA_BUF_COUNT);
        } else {
            LOG_INFO("  ✓ I2S DMA buffer count: %u", I2S_DMA_BUF_COUNT);
        }
        
        if (I2S_DMA_BUF_LEN <= 0) {
            LOG_ERROR("I2S_DMA_BUF_LEN must be > 0, got %u", I2S_DMA_BUF_LEN);
            valid = false;
        } else {
            LOG_INFO("  ✓ I2S DMA buffer length: %u", I2S_DMA_BUF_LEN);
        }
        
        if (I2S_MAX_READ_RETRIES <= 0) {
            LOG_WARN("I2S_MAX_READ_RETRIES is %u - should be > 0", I2S_MAX_READ_RETRIES);
        } else {
            LOG_INFO("  ✓ I2S max read retries: %u", I2S_MAX_READ_RETRIES);
        }
        
        return valid;
    }

    /**
     * Validate timing configuration
     * Checks: Check intervals are reasonable
     */
    static bool validateTimingConfig() {
        bool valid = true;
        
        LOG_INFO("Checking timing configuration...");
        
        if (MEMORY_CHECK_INTERVAL <= 0) {
            LOG_WARN("MEMORY_CHECK_INTERVAL is %u ms - should be > 0", MEMORY_CHECK_INTERVAL);
        } else if (MEMORY_CHECK_INTERVAL < 5000) {
            LOG_WARN("MEMORY_CHECK_INTERVAL (%u ms) is very frequent", MEMORY_CHECK_INTERVAL);
        } else {
            LOG_INFO("  ✓ Memory check interval: %u ms", MEMORY_CHECK_INTERVAL);
        }
        
        if (RSSI_CHECK_INTERVAL <= 0) {
            LOG_WARN("RSSI_CHECK_INTERVAL is %u ms - should be > 0", RSSI_CHECK_INTERVAL);
        } else {
            LOG_INFO("  ✓ RSSI check interval: %u ms", RSSI_CHECK_INTERVAL);
        }
        
        if (STATS_PRINT_INTERVAL <= 0) {
            LOG_WARN("STATS_PRINT_INTERVAL is %u ms - should be > 0", STATS_PRINT_INTERVAL);
        } else {
            LOG_INFO("  ✓ Stats print interval: %u ms", STATS_PRINT_INTERVAL);
        }
        
        return valid;
    }

    /**
     * Validate memory thresholds
     * Checks: Thresholds are logical (critical < warning) and non-zero
     */
    static bool validateMemoryThresholds() {
        bool valid = true;
        
        LOG_INFO("Checking memory thresholds...");
        
        if (MEMORY_CRITICAL_THRESHOLD <= 0) {
            LOG_ERROR("MEMORY_CRITICAL_THRESHOLD must be > 0, got %u bytes", MEMORY_CRITICAL_THRESHOLD);
            valid = false;
        } else {
            LOG_INFO("  ✓ Memory critical threshold: %u bytes", MEMORY_CRITICAL_THRESHOLD);
        }
        
        if (MEMORY_WARN_THRESHOLD <= 0) {
            LOG_ERROR("MEMORY_WARN_THRESHOLD must be > 0, got %u bytes", MEMORY_WARN_THRESHOLD);
            valid = false;
        } else {
            LOG_INFO("  ✓ Memory warn threshold: %u bytes", MEMORY_WARN_THRESHOLD);
        }
        
        if (MEMORY_CRITICAL_THRESHOLD >= MEMORY_WARN_THRESHOLD) {
            LOG_ERROR("MEMORY_CRITICAL_THRESHOLD (%u) must be < MEMORY_WARN_THRESHOLD (%u)",
                     MEMORY_CRITICAL_THRESHOLD, MEMORY_WARN_THRESHOLD);
            valid = false;
        } else {
            LOG_INFO("  ✓ Memory threshold hierarchy correct");
        }
        
        if (RSSI_WEAK_THRESHOLD > 0) {
            LOG_WARN("RSSI_WEAK_THRESHOLD (%d) is positive - should be negative dBm value", RSSI_WEAK_THRESHOLD);
        } else if (RSSI_WEAK_THRESHOLD > -20) {
            LOG_WARN("RSSI_WEAK_THRESHOLD (%d dBm) is very strong - typical range is -80 to -50", RSSI_WEAK_THRESHOLD);
        } else {
            LOG_INFO("  ✓ RSSI weak threshold: %d dBm", RSSI_WEAK_THRESHOLD);
        }
        
        if (MAX_CONSECUTIVE_FAILURES <= 0) {
            LOG_WARN("MAX_CONSECUTIVE_FAILURES is %u - should be > 0", MAX_CONSECUTIVE_FAILURES);
        } else {
            LOG_INFO("  ✓ Max consecutive failures: %u", MAX_CONSECUTIVE_FAILURES);
        }
        
        return valid;
    }

    /**
     * Validate watchdog configuration
     * Ensures watchdog timeout is compatible with operation timeouts
     */
    static bool validateWatchdogConfig() {
        bool valid = true;

        LOG_INFO("Checking watchdog configuration...");

        if (WATCHDOG_TIMEOUT_SEC <= 0) {
            LOG_ERROR("WATCHDOG_TIMEOUT_SEC must be > 0, got %u seconds", WATCHDOG_TIMEOUT_SEC);
            valid = false;
        } else if (WATCHDOG_TIMEOUT_SEC < 30) {
            LOG_WARN("WATCHDOG_TIMEOUT_SEC (%u sec) is short - recommend >= 30 seconds", WATCHDOG_TIMEOUT_SEC);
        } else {
            LOG_INFO("  \u2713 Watchdog timeout: %u seconds", WATCHDOG_TIMEOUT_SEC);
        }

        // Verify watchdog timeout doesn't conflict with WiFi timeout
        uint32_t wifi_timeout_sec = WIFI_TIMEOUT / 1000;
        if (WATCHDOG_TIMEOUT_SEC <= wifi_timeout_sec) {
            LOG_WARN("WATCHDOG_TIMEOUT_SEC (%u) <= WIFI_TIMEOUT (%u sec) - watchdog may reset during WiFi connection",
                     WATCHDOG_TIMEOUT_SEC, wifi_timeout_sec);
        } else {
            LOG_INFO("  \u2713 Watchdog timeout compatible with WiFi timeout");
        }

        // Verify watchdog timeout doesn't conflict with error recovery delay
        uint32_t error_delay_sec = ERROR_RECOVERY_DELAY / 1000;
        if (WATCHDOG_TIMEOUT_SEC <= error_delay_sec) {
            LOG_ERROR("WATCHDOG_TIMEOUT_SEC (%u) <= ERROR_RECOVERY_DELAY (%u sec) - watchdog will reset during error recovery",
                     WATCHDOG_TIMEOUT_SEC, error_delay_sec);
            valid = false;
        } else {
            LOG_INFO("  \u2713 Watchdog timeout compatible with error recovery delay");
        }

        // Verify watchdog is long enough for state operations
        // Typical operations: WiFi ~25s, I2S read ~1ms, TCP write ~100ms
        if (WATCHDOG_TIMEOUT_SEC < (wifi_timeout_sec + 5)) {
            LOG_WARN("WATCHDOG_TIMEOUT_SEC (%u) is close to WIFI_TIMEOUT (%u sec) - margin may be tight",
                     WATCHDOG_TIMEOUT_SEC, wifi_timeout_sec);
        }

        return valid;
    }
};

#endif // CONFIG_VALIDATOR_H
