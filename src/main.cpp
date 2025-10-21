#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "i2s_audio.h"
#include "network.h"
#include "StateManager.h"
#include "NonBlockingTimer.h"
#include "config_validator.h"
#include "serial_command.h"
#include "esp_task_wdt.h"

// ===== Function Declarations =====
void gracefulShutdown();

// ===== Global State Management =====
StateManager systemState;
static uint8_t audio_buffer[I2S_BUFFER_SIZE];  // Static buffer to avoid heap fragmentation

// ===== Statistics =====
struct SystemStats {
    uint64_t total_bytes_sent;
    uint32_t i2s_errors;
    unsigned long uptime_start;

    // Memory tracking for leak detection
    uint32_t peak_heap;
    uint32_t min_heap;
    uint32_t last_heap;
    unsigned long last_memory_check;
    int32_t heap_trend;  // +1 = increasing, -1 = decreasing, 0 = stable

    void init() {
        total_bytes_sent = 0;
        i2s_errors = 0;
        uptime_start = millis();

        uint32_t current_heap = ESP.getFreeHeap();
        peak_heap = current_heap;
        min_heap = current_heap;
        last_heap = current_heap;
        last_memory_check = millis();
        heap_trend = 0;
    }

    void updateMemoryStats() {
        uint32_t current_heap = ESP.getFreeHeap();

        // Update peak and minimum
        if (current_heap > peak_heap) peak_heap = current_heap;
        if (current_heap < min_heap) min_heap = current_heap;

        // Detect heap trend (potential memory leak)
        if (current_heap < last_heap - 1000) {
            heap_trend = -1;  // Decreasing - potential leak
        } else if (current_heap > last_heap + 1000) {
            heap_trend = 1;   // Increasing - memory recovered
        } else {
            heap_trend = 0;   // Stable
        }

        last_heap = current_heap;
        last_memory_check = millis();
    }

    void printStats() {
        updateMemoryStats();  // Update memory trend before printing

        unsigned long uptime_sec = (millis() - uptime_start) / 1000;
        uint32_t current_heap = ESP.getFreeHeap();

        LOG_INFO("=== System Statistics ===");
        LOG_INFO("Uptime: %lu seconds (%.1f hours)", uptime_sec, uptime_sec / 3600.0);
        LOG_INFO("Data sent: %llu bytes (%.2f MB)", total_bytes_sent, total_bytes_sent / 1048576.0);
        LOG_INFO("WiFi reconnects: %u", NetworkManager::getWiFiReconnectCount());
        LOG_INFO("Server reconnects: %u", NetworkManager::getServerReconnectCount());
        LOG_INFO("I2S errors: %u (total: %u, transient: %u, permanent: %u)",
                 i2s_errors, I2SAudio::getErrorCount(),
                 I2SAudio::getTransientErrorCount(),
                 I2SAudio::getPermanentErrorCount());
        LOG_INFO("TCP errors: %u", NetworkManager::getTCPErrorCount());

        // Memory statistics
        LOG_INFO("--- Memory Statistics ---");
        LOG_INFO("Current heap: %u bytes", current_heap);
        LOG_INFO("Peak heap: %u bytes", peak_heap);
        LOG_INFO("Min heap: %u bytes", min_heap);
        LOG_INFO("Heap range: %u bytes", peak_heap - min_heap);

        // Detect potential memory leak
        if (heap_trend == -1) {
            LOG_WARN("Memory trend: DECREASING (potential leak)");
        } else if (heap_trend == 1) {
            LOG_INFO("Memory trend: INCREASING (recovered)");
        } else {
            LOG_INFO("Memory trend: STABLE");
        }

        LOG_INFO("========================");
    }
} stats;

// ===== Timers =====
NonBlockingTimer memoryCheckTimer(MEMORY_CHECK_INTERVAL, true);
NonBlockingTimer statsPrintTimer(STATS_PRINT_INTERVAL, true);

// ===== Memory Monitoring =====
void checkMemoryHealth() {
    if (!memoryCheckTimer.check()) return;

    // Update memory tracking statistics
    stats.updateMemoryStats();

    uint32_t free_heap = ESP.getFreeHeap();

    if (free_heap < MEMORY_CRITICAL_THRESHOLD) {
        LOG_CRITICAL("Critical low memory: %u bytes - system may crash", free_heap);
        // Consider restarting if critically low
        if (free_heap < MEMORY_CRITICAL_THRESHOLD / 2) {
            LOG_CRITICAL("Memory critically low - initiating graceful restart");
            gracefulShutdown();
            ESP.restart();
        }
    } else if (free_heap < MEMORY_WARN_THRESHOLD) {
        LOG_WARN("Memory low: %u bytes", free_heap);
    }

    // Warn about potential memory leak
    if (stats.heap_trend == -1) {
        LOG_WARN("Memory usage trending downward (potential leak detected)");
    }
}

// ===== State Change Callback =====
void onStateChange(SystemState from, SystemState to) {
    LOG_INFO("State transition: %s → %s",
             systemState.stateToString(from).c_str(),
             systemState.stateToString(to).c_str());
}

// ===== Graceful Shutdown =====
void gracefulShutdown() {
    LOG_INFO("========================================");
    LOG_INFO("Initiating graceful shutdown...");
    LOG_INFO("========================================");

    // Print final statistics
    stats.printStats();

    // Close TCP connection
    if (NetworkManager::isServerConnected()) {
        LOG_INFO("Closing server connection...");
        NetworkManager::disconnectFromServer();
        delay(GRACEFUL_SHUTDOWN_DELAY);
    }

    // Stop I2S audio
    LOG_INFO("Stopping I2S audio...");
    I2SAudio::cleanup();

    // Disconnect WiFi
    LOG_INFO("Disconnecting WiFi...");
    WiFi.disconnect(true);
    delay(GRACEFUL_SHUTDOWN_DELAY);

    LOG_INFO("Shutdown complete. Ready for restart.");
    delay(1000);
}

// ===== Setup =====
void setup() {
    // Initialize logger (align with compile-time DEBUG_LEVEL)
    LogLevel bootLogLevel = LOG_INFO;
    #if DEBUG_LEVEL >= 4
        bootLogLevel = LOG_DEBUG;
    #elif DEBUG_LEVEL == 3
        bootLogLevel = LOG_INFO;
    #elif DEBUG_LEVEL == 2
        bootLogLevel = LOG_WARN;
    #elif DEBUG_LEVEL == 1
        bootLogLevel = LOG_ERROR;
    #else
        bootLogLevel = LOG_CRITICAL;
    #endif
    Logger::init(bootLogLevel);
    LOG_INFO("========================================");
    LOG_INFO("ESP32 Audio Streamer Starting Up");
    LOG_INFO("Board: %s", BOARD_NAME);
    LOG_INFO("Version: 2.0 (Reliability-Enhanced)");
    LOG_INFO("========================================");

    // Initialize statistics
    stats.init();

    // Validate configuration before proceeding
    if (!ConfigValidator::validateAll()) {
        LOG_CRITICAL("Configuration validation failed - cannot start system");
        LOG_CRITICAL("Please check config.h and fix the issues listed above");
        systemState.setState(SystemState::ERROR);
        while (1) {
            delay(ERROR_RECOVERY_DELAY);
            LOG_CRITICAL("Waiting for configuration fix...");
        }
    }

    // Initialize state manager with callback
    systemState.onStateChange(onStateChange);
    systemState.setState(SystemState::INITIALIZING);

    // Initialize I2S
    if (!I2SAudio::initialize()) {
        LOG_CRITICAL("I2S initialization failed - cannot continue");
        systemState.setState(SystemState::ERROR);
        while (1) {
            delay(1000);
        }
    }

    // Initialize network
    NetworkManager::initialize();

    // Initialize serial command handler
    SerialCommandHandler::initialize();

    // Start memory and stats timers
    memoryCheckTimer.start();
    statsPrintTimer.start();

    // Move to WiFi connection state
    systemState.setState(SystemState::CONNECTING_WIFI);

    // Initialize and configure watchdog to a safe timeout
    // Ensure timeout comfortably exceeds WiFi timeouts and recovery delays
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);

    LOG_INFO("Setup complete - entering main loop");
}

// ===== Main Loop with State Machine =====
void loop() {
    // Feed watchdog timer
    esp_task_wdt_reset();

    // Process serial commands (non-blocking)
    SerialCommandHandler::processCommands();

    // Handle WiFi connection (non-blocking)
    NetworkManager::handleWiFiConnection();

    // Monitor WiFi quality
    NetworkManager::monitorWiFiQuality();

    // Check memory health
    checkMemoryHealth();

    // Print statistics periodically
    if (statsPrintTimer.check()) {
        stats.printStats();
    }

    // State machine
    switch (systemState.getState()) {
        case SystemState::INITIALIZING:
            // Should not reach here after setup
            systemState.setState(SystemState::CONNECTING_WIFI);
            break;

        case SystemState::CONNECTING_WIFI:
            if (NetworkManager::isWiFiConnected()) {
                LOG_INFO("WiFi connected - IP: %s", WiFi.localIP().toString().c_str());
                systemState.setState(SystemState::CONNECTING_SERVER);
            } else if (systemState.hasStateTimedOut(WIFI_TIMEOUT)) {
                LOG_ERROR("WiFi connection timeout");
                systemState.setState(SystemState::ERROR);
            }
            break;

        case SystemState::CONNECTING_SERVER:
            if (!NetworkManager::isWiFiConnected()) {
                LOG_WARN("WiFi lost while connecting to server");
                systemState.setState(SystemState::CONNECTING_WIFI);
                break;
            }

            if (NetworkManager::connectToServer()) {
                systemState.setState(SystemState::CONNECTED);
            }
            // Timeout handled by exponential backoff in NetworkManager
            break;

        case SystemState::CONNECTED:
            {
                // Verify WiFi is still connected
                if (!NetworkManager::isWiFiConnected()) {
                    LOG_WARN("WiFi lost during streaming");
                    NetworkManager::disconnectFromServer();
                    systemState.setState(SystemState::CONNECTING_WIFI);
                    break;
                }

                // Verify server connection
                if (!NetworkManager::isServerConnected()) {
                    LOG_WARN("Server connection lost");
                    systemState.setState(SystemState::CONNECTING_SERVER);
                    break;
                }

                // Read audio data with retry
                size_t bytes_read = 0;
                if (I2SAudio::readDataWithRetry(audio_buffer, I2S_BUFFER_SIZE, &bytes_read)) {
                    // Send data to server
                    if (NetworkManager::writeData(audio_buffer, bytes_read)) {
                        stats.total_bytes_sent += bytes_read;
                    } else {
                        // Write failed - let NetworkManager handle reconnection
                        LOG_WARN("Data transmission failed");
                        systemState.setState(SystemState::CONNECTING_SERVER);
                    }
                } else {
                    // I2S read failed even after retries
                    stats.i2s_errors++;
                    LOG_ERROR("I2S read failed after retries");

                    // If too many consecutive errors, may need to reinitialize
                    // (handled internally by I2SAudio)
                }

                // Small delay to allow background tasks
                delay(TASK_YIELD_DELAY);
            }
            break;

        case SystemState::DISCONNECTED:
            // Attempt to reconnect
            systemState.setState(SystemState::CONNECTING_SERVER);
            break;

        case SystemState::ERROR:
            LOG_ERROR("System in error state - attempting recovery...");
            delay(ERROR_RECOVERY_DELAY);

            // Try to recover
            NetworkManager::disconnectFromServer();
            systemState.setState(SystemState::CONNECTING_WIFI);
            break;

        case SystemState::MAINTENANCE:
            // Reserved for future use (e.g., firmware updates)
            LOG_INFO("System in maintenance mode");
            delay(ERROR_RECOVERY_DELAY);
            break;
    }
}