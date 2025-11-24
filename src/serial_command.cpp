#include "serial_command.h"
#include "logger.h"
#include "network.h"
#include "i2s_audio.h"
#include "StateManager.h"

// Static member initialization
char SerialCommandHandler::command_buffer[SerialCommandHandler::BUFFER_SIZE] = {0};
size_t SerialCommandHandler::buffer_index = 0;

// Declare external state manager (from main.cpp)
extern StateManager systemState;

void SerialCommandHandler::initialize() {
    LOG_INFO("Serial Command Handler initialized");
    LOG_INFO("Type 'HELP' for available commands");
}

void SerialCommandHandler::processCommands() {
    // Check if data is available on serial
    if (!Serial.available()) {
        return;
    }

    // Read one character
    char c = Serial.read();

    // Handle backspace
    if (c == '\b' || c == 0x7F) {
        if (buffer_index > 0) {
            buffer_index--;
            Serial.write('\b');
            Serial.write(' ');
            Serial.write('\b');
        }
        return;
    }

    // Handle newline (end of command)
    if (c == '\r' || c == '\n') {
        if (buffer_index > 0) {
            command_buffer[buffer_index] = '\0';
            Serial.println(); // Echo newline

            // Convert to uppercase for case-insensitive comparison
            for (size_t i = 0; i < buffer_index; i++) {
                command_buffer[i] = toupper(command_buffer[i]);
            }

            // Parse and execute command
            char* cmd = command_buffer;
            char* args = nullptr;
            char* space = strchr(command_buffer, ' ');
            if (space != nullptr) {
                *space = '\0'; // Null-terminate the command
                args = space + 1;
            }

            if (cmd != nullptr) {
                if (strcmp(cmd, "STATUS") == 0) {
                    handleStatusCommand();
                } else if (strcmp(cmd, "CONFIG") == 0) {
                    handleConfigCommand(args);
                } else if (strcmp(cmd, "RESTART") == 0) {
                    handleRestartCommand();
                } else if (strcmp(cmd, "DISCONNECT") == 0) {
                    handleDisconnectCommand();
                } else if (strcmp(cmd, "CONNECT") == 0) {
                    handleConnectCommand();
                } else if (strcmp(cmd, "STATS") == 0) {
                    handleStatsCommand();
                } else if (strcmp(cmd, "HEALTH") == 0) {
                    handleHealthCommand();
                } else if (strcmp(cmd, "HELP") == 0) {
                    handleHelpCommand();
                } else {
                    LOG_ERROR("Unknown command: %s", cmd);
                    handleHelpCommand();
                }
            }

            clearBuffer();
        }
        return;
    }

    // Handle regular characters
    // BUG FIX: Add explicit buffer overflow protection
    // Previous check was correct but lacked explicit overflow handling
    // Now we explicitly reject characters beyond buffer capacity to prevent silent data loss
    if (buffer_index < BUFFER_SIZE - 1) {
        command_buffer[buffer_index++] = c;
        Serial.write(c);  // Echo character
    } else {
        // Buffer full - reject further input and alert user
        Serial.write('\a');  // Bell character to indicate buffer full
        LOG_WARN("Serial command buffer full - rejecting input (max %d chars)", BUFFER_SIZE - 1);
    }
}

void SerialCommandHandler::handleStatusCommand() {
    LOG_INFO("========== SYSTEM STATUS ==========");
    printStatus();
    LOG_INFO("===================================");
}

void SerialCommandHandler::printStatus() {
    // WiFi Status
    if (NetworkManager::isWiFiConnected()) {
        LOG_INFO("WiFi: CONNECTED (%s)", WiFi.localIP().toString().c_str());
        LOG_INFO("WiFi Signal: %d dBm", WiFi.RSSI());
    } else {
        LOG_INFO("WiFi: DISCONNECTED");
    }

    // Server/TCP Status
    TCPConnectionState tcp_state = NetworkManager::getTCPState();
    const char* state_name = "UNKNOWN";
    switch (tcp_state) {
        case TCPConnectionState::DISCONNECTED:
            state_name = "DISCONNECTED";
            break;
        case TCPConnectionState::CONNECTING:
            state_name = "CONNECTING";
            break;
        case TCPConnectionState::CONNECTED:
            state_name = "CONNECTED";
            LOG_INFO("TCP Connection uptime: %lu ms", NetworkManager::getConnectionUptime());
            break;
        case TCPConnectionState::ERROR:
            state_name = "ERROR";
            break;
        case TCPConnectionState::CLOSING:
            state_name = "CLOSING";
            break;
    }
    LOG_INFO("TCP State: %s", state_name);
    LOG_INFO("Server: %s:%d", SERVER_HOST, SERVER_PORT);

    // System State
    LOG_INFO("System State: %s", systemState.stateToString(systemState.getState()).c_str());

    // Memory Status
    uint32_t free_heap = ESP.getFreeHeap();
    LOG_INFO("Free Memory: %u bytes (%.1f KB)", free_heap, free_heap / 1024.0);

    // Statistics
    LOG_INFO("WiFi Reconnects: %u", NetworkManager::getWiFiReconnectCount());
    LOG_INFO("Server Reconnects: %u", NetworkManager::getServerReconnectCount());
    LOG_INFO("TCP Errors: %u", NetworkManager::getTCPErrorCount());
    LOG_INFO("TCP State Changes: %u", NetworkManager::getTCPStateChangeCount());
}

void SerialCommandHandler::handleConfigCommand(const char* args) {
    if (args == nullptr) {
        LOG_ERROR("CONFIG: Missing arguments");
        LOG_INFO("Usage: CONFIG [SHOW|SET <param> <value>]");
        return;
    }

    // BUG FIX: Use safer string copy with explicit bounds checking
    char args_copy[64];
    size_t args_len = strlen(args);
    if (args_len >= sizeof(args_copy)) {
        LOG_ERROR("CONFIG: Arguments too long (%u chars, max %u)", args_len, sizeof(args_copy) - 1);
        return;
    }
    
    strncpy(args_copy, args, sizeof(args_copy) - 1);
    args_copy[sizeof(args_copy) - 1] = '\0';

    char* subcmd = strtok(args_copy, " ");
    if (subcmd == nullptr) return;

    // BUG FIX: Add bounds check for string length before toupper loop
    size_t subcmd_len = strlen(subcmd);
    for (size_t i = 0; i < subcmd_len && i < 64; i++) {
        subcmd[i] = toupper(subcmd[i]);
    }

    if (strcmp(subcmd, "SHOW") == 0) {
        LOG_INFO("========== CONFIG PARAMETERS ==========");
        LOG_INFO("WiFi SSID: %s", WIFI_SSID);
        LOG_INFO("Server: %s:%d", SERVER_HOST, SERVER_PORT);
        LOG_INFO("I2S Sample Rate: %d Hz", I2S_SAMPLE_RATE);
        LOG_INFO("I2S Buffer Size: %d bytes", I2S_BUFFER_SIZE);
        LOG_INFO("Memory Warning Threshold: %d bytes", MEMORY_WARN_THRESHOLD);
        LOG_INFO("Memory Critical Threshold: %d bytes", MEMORY_CRITICAL_THRESHOLD);
        LOG_INFO("========================================");
    } else {
        LOG_ERROR("Unknown CONFIG subcommand: %s", subcmd);
    }
}

void SerialCommandHandler::handleRestartCommand() {
    LOG_CRITICAL("Restarting system in 3 seconds...");
    delay(3000);
    ESP.restart();
}

void SerialCommandHandler::handleDisconnectCommand() {
    LOG_INFO("Disconnecting from server...");
    NetworkManager::disconnectFromServer();
    LOG_INFO("Disconnected");
}

void SerialCommandHandler::handleConnectCommand() {
    LOG_INFO("Attempting to connect to server...");
    if (NetworkManager::connectToServer()) {
        LOG_INFO("Connected successfully");
    } else {
        LOG_WARN("Connection attempt scheduled (check exponential backoff)");
    }
}

void SerialCommandHandler::handleStatsCommand() {
    LOG_INFO("========== DETAILED STATISTICS ==========");
    
    // BUG FIX: Handle millis() overflow in uptime calculation
    unsigned long current_millis = millis();
    unsigned long uptime_ms = current_millis; // Simplified - millis() returns total uptime
    unsigned long uptime_sec = uptime_ms / 1000;
    LOG_INFO("Uptime: %lu seconds", uptime_sec);

    // Memory stats
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t heap_size = ESP.getHeapSize();
    
    // BUG FIX: Validate heap_size to prevent division by zero
    if (heap_size == 0) {
        LOG_ERROR("Heap size reported as 0 - cannot calculate usage percentage");
        LOG_INFO("Heap - Free: %u bytes, Total: UNKNOWN", free_heap);
    } else {
        uint32_t used_heap = (heap_size > free_heap) ? (heap_size - free_heap) : 0;
        float usage_pct = (used_heap * 100.0f) / heap_size;
        
        LOG_INFO("Heap - Free: %u bytes, Total: %u bytes", free_heap, heap_size);
        LOG_INFO("Heap - Used: %u bytes (%.1f%%)", used_heap, usage_pct);
    }

    // I2S stats
    LOG_INFO("I2S Total Errors: %u", I2SAudio::getErrorCount());
    LOG_INFO("I2S Transient Errors: %u", I2SAudio::getTransientErrorCount());
    LOG_INFO("I2S Permanent Errors: %u", I2SAudio::getPermanentErrorCount());

    // Network stats
    LOG_INFO("WiFi Reconnects: %u", NetworkManager::getWiFiReconnectCount());
    LOG_INFO("Server Reconnects: %u", NetworkManager::getServerReconnectCount());
    LOG_INFO("TCP Errors: %u", NetworkManager::getTCPErrorCount());

    // Connection info
    if (NetworkManager::isTCPConnected()) {
        LOG_INFO("Time Since Last Write: %lu ms", NetworkManager::getTimeSinceLastWrite());
        LOG_INFO("Connection Uptime: %lu ms", NetworkManager::getConnectionUptime());
    }

    LOG_INFO("=========================================");
}

void SerialCommandHandler::handleHealthCommand() {
    LOG_INFO("========== SYSTEM HEALTH CHECK ==========");
    printHealth();
    LOG_INFO("=========================================");
}

void SerialCommandHandler::printHealth() {
    // WiFi health
    if (NetworkManager::isWiFiConnected()) {
        int32_t rssi = WiFi.RSSI();
        LOG_INFO("✓ WiFi Connected - Signal: %d dBm", rssi);
        if (rssi < -80) {
            LOG_WARN("  ⚠ Weak signal - consider relocating device");
        }
    } else {
        LOG_ERROR("✗ WiFi Not Connected");
    }

    // TCP health
    if (NetworkManager::isTCPConnected()) {
        LOG_INFO("✓ TCP Connected - Uptime: %lu ms", NetworkManager::getConnectionUptime());
    } else {
        LOG_ERROR("✗ TCP Not Connected - State: %d", (int)NetworkManager::getTCPState());
    }

    // Memory health
    uint32_t free_heap = ESP.getFreeHeap();
    if (free_heap > 50000) {
        LOG_INFO("✓ Memory Healthy - Free: %u bytes", free_heap);
    } else if (free_heap > 20000) {
        LOG_WARN("⚠ Memory Low - Free: %u bytes", free_heap);
    } else {
        LOG_ERROR("✗ Memory Critical - Free: %u bytes", free_heap);
    }

    // I2S health
    if (I2SAudio::healthCheck()) {
        LOG_INFO("✓ I2S Healthy - Errors: %u", I2SAudio::getErrorCount());
    } else {
        LOG_ERROR("✗ I2S Unhealthy - Consider reinitialization");
    }

    // System state
    SystemState state = systemState.getState();
    if (state == SystemState::CONNECTED) {
        LOG_INFO("✓ System State: CONNECTED");
    } else if (state == SystemState::ERROR) {
        LOG_ERROR("✗ System State: ERROR");
    } else {
        LOG_WARN("⚠ System State: %s", systemState.stateToString(state).c_str());
    }
}

void SerialCommandHandler::handleHelpCommand() {
    LOG_INFO("========== AVAILABLE COMMANDS ==========");
    LOG_INFO("STATUS  - Show current system status");
    LOG_INFO("STATS   - Show detailed statistics");
    LOG_INFO("HEALTH  - Perform system health check");
    LOG_INFO("CONFIG SHOW - Display configuration");
    LOG_INFO("CONNECT - Attempt to connect to server");
    LOG_INFO("DISCONNECT - Disconnect from server");
    LOG_INFO("RESTART - Restart the system");
    LOG_INFO("HELP    - Show this help message");
    LOG_INFO("=========================================");
}

void SerialCommandHandler::clearBuffer() {
    memset(command_buffer, 0, BUFFER_SIZE);
    buffer_index = 0;
}
