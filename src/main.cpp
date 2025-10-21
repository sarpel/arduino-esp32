#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // GPIO2 is typically the built-in LED on ESP32
#endif

#include <Arduino.h>
#include "core/SystemManager.h"
#include "core/EventBus.h"
#include "core/StateMachine.h"
#include "audio/AudioProcessor.h"
#include "network/NetworkManager.h"
#include "monitoring/HealthMonitor.h"
#include "utils/MemoryManager.h"
#include "config.h"
#include "esp_task_wdt.h"

// Global system manager reference
SystemManager &systemManager = SystemManager::getInstance();

// System startup time
unsigned long systemStartupTime = 0;

// Function declarations
void handleSystemEvents();
void handleSerialCommands();
void printSystemBanner();
void printSystemInfo();
void emergencyHandler();

// Print function declarations
void printSystemStatus();
void printDetailedStatistics();
void printStateInfo();
void printMemoryInfo();
void printAudioInfo();
void printNetworkInfo();
void printHealthInfo();
void printEventInfo();

// Emergency flag
volatile bool emergencyStop = false;

void setup()
{
    // Initialize serial communication
    Serial.begin(115200);
    delay(SERIAL_INIT_DELAY);

    // Print system banner
    printSystemBanner();

    // Record startup time
    systemStartupTime = millis();

    // Install emergency handler
    // emergencyHandler(); // Commented out for now - can be implemented later

    // Initialize the system manager
    if (!systemManager.initialize())
    {
        Serial.println("[CRITICAL] System initialization failed!");
        Serial.println("[CRITICAL] System will halt. Please check configuration and restart.");

        // Enter infinite loop with error indication
        while (true)
        {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(200);
            digitalWrite(LED_BUILTIN, LOW);
            delay(200);
        }
    }

    // Print system information
    printSystemInfo();

    // Register event handlers
    handleSystemEvents();

    Serial.println("[INFO] System initialization completed successfully");
    Serial.println("[INFO] Type 'HELP' for available commands");
    Serial.println("========================================");
}

void loop()
{
    // Check for emergency stop
    if (emergencyStop)
    {
        systemManager.emergencyStop();
        Serial.println("[EMERGENCY] Emergency stop activated!");
        Serial.println("[EMERGENCY] System will shutdown...");

        // Graceful shutdown
        systemManager.shutdown();

        // Halt system
        while (true)
        {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    }

    // Run the main system loop
    systemManager.run();

    // Handle serial commands (non-blocking)
    handleSerialCommands();
    
    // Yield to WiFi stack and prevent CPU starvation
    vTaskDelay(pdMS_TO_TICKS(10));
}

void printSystemBanner()
{
    Serial.println("========================================");
    Serial.println("    ESP32 Audio Streamer v3.0");
    Serial.println("    Enhanced Modular Architecture");
    Serial.println("    Professional Audio Streaming System");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Features:");
    Serial.println("  ✓ Advanced Audio Processing (NR, AGC, VAD)");
    Serial.println("  ✓ Event-Driven Architecture");
    Serial.println("  ✓ Enhanced State Machine");
    Serial.println("  ✓ Modular Component Design");
    Serial.println("  ✓ Comprehensive Health Monitoring");
    Serial.println("  ✓ Memory Pool Management");
    Serial.println("  ✓ Power Optimization");
    Serial.println("  ✓ OTA Update Support");
    Serial.println("========================================");
}

void printSystemInfo()
{
    Serial.println("[INFO] System Information:");
    Serial.printf("[INFO] Board: %s\n", BOARD_NAME);
    Serial.printf("[INFO] CPU Frequency: %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("[INFO] Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("[INFO] Total Heap: %u bytes\n", ESP.getHeapSize());
    Serial.printf("[INFO] Flash Size: %u bytes\n", ESP.getFlashChipSize());
    Serial.printf("[INFO] SDK Version: %s\n", ESP.getSdkVersion());
    Serial.printf("[INFO] Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("[INFO] Chip Revision: %u\n", ESP.getChipRevision());
    Serial.printf("[INFO] Chip Cores: %u\n", ESP.getChipCores());
    Serial.println("========================================");
}

void handleSystemEvents()
{
    // Subscribe to critical system events
    EventBus *eventBus = systemManager.getEventBus();
    if (!eventBus)
    {
        Serial.println("[WARN] EventBus not available - event handling disabled");
        return;
    }

    // System error events
    eventBus->subscribe(SystemEvent::SYSTEM_ERROR, [](const void *data)
                        {
                            Serial.println("[ERROR] System error detected!");
                            // Additional error handling can be added here
                        },
                        EventPriority::CRITICAL_PRIORITY, "main");

    // Memory critical events
    eventBus->subscribe(SystemEvent::MEMORY_CRITICAL, [](const void *data)
                        {
        Serial.println("[CRITICAL] Memory critical situation!");
        // Emergency memory cleanup
        systemManager.getMemoryManager()->emergencyCleanup(); }, EventPriority::CRITICAL_PRIORITY, "main");

    // Network disconnection events
    eventBus->subscribe(SystemEvent::NETWORK_DISCONNECTED, [](const void *data)
                        { Serial.println("[WARN] Network connection lost!"); }, EventPriority::HIGH_PRIORITY, "main");

    // Server connection events
    eventBus->subscribe(SystemEvent::SERVER_CONNECTED, [](const void *data)
                        { Serial.println("[INFO] Server connection established!"); }, EventPriority::HIGH_PRIORITY, "main");

    eventBus->subscribe(SystemEvent::SERVER_DISCONNECTED, [](const void *data)
                        { Serial.println("[WARN] Server connection lost!"); }, EventPriority::HIGH_PRIORITY, "main");

    // Audio quality events
    eventBus->subscribe(SystemEvent::AUDIO_QUALITY_DEGRADED, [](const void *data)
                        { Serial.println("[WARN] Audio quality degraded!"); }, EventPriority::NORMAL_PRIORITY, "main");

    // CPU overload events
    eventBus->subscribe(SystemEvent::CPU_OVERLOAD, [](const void *data)
                        { Serial.println("[WARN] CPU overload detected!"); }, EventPriority::HIGH_PRIORITY, "main");

    Serial.println("[INFO] System event handlers registered");
}

void handleSerialCommands()
{
    static String commandBuffer = "";

    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\n' || c == '\r')
        {
            if (commandBuffer.length() > 0)
            {
                // Process complete command
                commandBuffer.toUpperCase();
                commandBuffer.trim();

                if (commandBuffer == "HELP")
                {
                    Serial.println("Available Commands:");
                    Serial.println("  HELP              - Show this help");
                    Serial.println("  STATUS            - Show system status");
                    Serial.println("  STATS             - Show detailed statistics");
                    Serial.println("  STATE             - Show current state");
                    Serial.println("  MEMORY            - Show memory information");
                    Serial.println("  AUDIO             - Show audio statistics");
                    Serial.println("  NETWORK           - Show network information");
                    Serial.println("  HEALTH            - Show health status");
                    Serial.println("  EVENTS            - Show event statistics");
                    Serial.println("  RECONNECT         - Force reconnection");
                    Serial.println("  REBOOT            - Restart the system");
                    Serial.println("  EMERGENCY         - Emergency stop");
                    Serial.println("  DEBUG <0-5>       - Set debug level");
                    Serial.println("  QUALITY <0-3>     - Set audio quality (0=LOW, 3=ULTRA)");
                    Serial.println("  FEATURE <name> <0/1> - Enable/disable audio feature");
                }
                else if (commandBuffer == "STATUS")
                {
                    printSystemStatus();
                }
                else if (commandBuffer == "STATS")
                {
                    printDetailedStatistics();
                }
                else if (commandBuffer == "STATE")
                {
                    printStateInfo();
                }
                else if (commandBuffer == "MEMORY")
                {
                    printMemoryInfo();
                }
                else if (commandBuffer == "AUDIO")
                {
                    printAudioInfo();
                }
                else if (commandBuffer == "NETWORK")
                {
                    printNetworkInfo();
                }
                else if (commandBuffer == "HEALTH")
                {
                    printHealthInfo();
                }
                else if (commandBuffer == "EVENTS")
                {
                    printEventInfo();
                }
                else if (commandBuffer == "RECONNECT")
                {
                    Serial.println("[INFO] Forcing reconnection...");
                    systemManager.getStateMachine()->setState(SystemState::CONNECTING_WIFI);
                }
                else if (commandBuffer == "REBOOT")
                {
                    Serial.println("[INFO] System reboot requested...");
                    delay(1000);
                    ESP.restart();
                }
                else if (commandBuffer == "EMERGENCY")
                {
                    Serial.println("[EMERGENCY] Emergency stop requested!");
                    emergencyStop = true;
                }
                else if (commandBuffer.startsWith("DEBUG "))
                {
                    int level = commandBuffer.substring(6).toInt();
                    if (level >= 0 && level <= 5)
                    {
                        Serial.printf("[INFO] Setting debug level to %d\n", level);
                        // Debug level setting would be implemented here
                    }
                    else
                    {
                        Serial.println("[ERROR] Debug level must be 0-5");
                    }
                }
                else if (commandBuffer.startsWith("QUALITY "))
                {
                    int quality = commandBuffer.substring(8).toInt();
                    if (quality >= 0 && quality <= 3)
                    {
                        AudioQuality audioQuality = static_cast<AudioQuality>(quality);
                        systemManager.getAudioProcessor()->setQuality(audioQuality);
                        Serial.printf("[INFO] Audio quality set to %d\n", quality);
                    }
                    else
                    {
                        Serial.println("[ERROR] Quality must be 0-3");
                    }
                }
                else if (commandBuffer.startsWith("FEATURE "))
                {
                    // Parse feature command: FEATURE <name> <0/1>
                    String featurePart = commandBuffer.substring(8);
                    int spaceIndex = featurePart.indexOf(' ');
                    if (spaceIndex > 0)
                    {
                        String featureName = featurePart.substring(0, spaceIndex);
                        int enable = featurePart.substring(spaceIndex + 1).toInt();

                        AudioFeature feature;
                        if (featureName == "NOISE_REDUCTION")
                        {
                            feature = AudioFeature::NOISE_REDUCTION;
                        }
                        else if (featureName == "AGC")
                        {
                            feature = AudioFeature::AUTOMATIC_GAIN_CONTROL;
                        }
                        else if (featureName == "VAD")
                        {
                            feature = AudioFeature::VOICE_ACTIVITY_DETECTION;
                        }
                        else
                        {
                            Serial.println("[ERROR] Unknown feature: " + featureName);
                            commandBuffer = "";
                            continue;
                        }

                        systemManager.getAudioProcessor()->enableFeature(feature, enable != 0);
                        Serial.printf("[INFO] Feature %s %s\n", featureName.c_str(), enable ? "enabled" : "disabled");
                    }
                    else
                    {
                        Serial.println("[ERROR] Invalid FEATURE command format");
                    }
                }
                else
                {
                    Serial.println("[ERROR] Unknown command: " + commandBuffer);
                    Serial.println("[INFO] Type 'HELP' for available commands");
                }

                commandBuffer = "";
            }
        }
        else if (c >= 32 && c <= 126)
        { // Printable characters only
            commandBuffer += c;
            if (commandBuffer.length() > 100)
            { // Prevent buffer overflow
                commandBuffer = "";
            }
        }
    }
}

// ESP32 API compatibility helper
static uint8_t getHeapFragmentation()
{
#if defined(ESP32)
    // ESP32 doesn't have getHeapFragmentation() - calculate it
    size_t free_heap = ESP.getFreeHeap();
    size_t largest_block = ESP.getMaxAllocHeap();
    if (free_heap > 0)
    {
        return 100 - ((largest_block * 100) / free_heap);
    }
    return 0;
#else
    return ESP.getHeapFragmentation();
#endif
}

void printMemoryInfo()
{
    Serial.println("=== Memory Information ===");
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Total Heap: %u bytes\n", ESP.getHeapSize());
    Serial.printf("Used Heap: %u bytes\n", ESP.getHeapSize() - ESP.getFreeHeap());
    Serial.printf("Heap Fragmentation: %u%%\n", getHeapFragmentation());
    Serial.printf("Largest Free Block: %u bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("Minimum Free Heap: %u bytes\n", ESP.getMinFreeHeap());

    if (systemManager.getMemoryManager())
    {
        systemManager.getMemoryManager()->printStatistics();
    }

    Serial.println("=========================");
}

void printAudioInfo()
{
    Serial.println("=== Audio Information ===");
    if (systemManager.getAudioProcessor())
    {
        systemManager.getAudioProcessor()->printStatistics();
    }
    else
    {
        Serial.println("Audio processor not available");
    }
    Serial.println("========================");
}

void printNetworkInfo()
{
    Serial.println("=== Network Information ===");
    if (systemManager.getNetworkManager())
    {
        Serial.printf("WiFi Connected: %s\n", systemManager.getNetworkManager()->isWiFiConnected() ? "yes" : "no");
        if (systemManager.getNetworkManager()->isWiFiConnected())
        {
            Serial.printf("WiFi RSSI: %d dBm\n", systemManager.getNetworkManager()->getWiFiRSSI());
            Serial.printf("Network Stability: %.2f\n", systemManager.getNetworkManager()->getNetworkStability());
        }
        Serial.printf("Server Connected: %s\n", systemManager.getNetworkManager()->isServerConnected() ? "yes" : "no");
        Serial.printf("Connection Drops: %u\n", systemManager.getContext().connection_drops);
    }
    else
    {
        Serial.println("Network manager not available");
    }
    Serial.println("==========================");
}

void printHealthInfo()
{
    Serial.println("=== Health Information ===");
    if (systemManager.getHealthMonitor())
    {
        auto health = systemManager.getHealthMonitor()->checkSystemHealth();
        Serial.printf("Overall Health Score: %.2f\n", health.overall_score);
        Serial.printf("CPU Load: %.1f%%\n", health.cpu_load_percent);
        Serial.printf("Memory Pressure: %.2f\n", health.memory_pressure);
        Serial.printf("Network Stability: %.2f\n", health.network_stability);
        Serial.printf("Audio Quality Score: %.2f\n", health.audio_quality_score);
        Serial.printf("Temperature: %.1f°C\n", health.temperature);
        Serial.printf("Predicted Failures: %u\n", health.predicted_failures);
    }
    else
    {
        Serial.println("Health monitor not available");
    }
    Serial.println("=========================");
}

void printEventInfo()
{
    Serial.println("=== Event Information ===");
    if (systemManager.getEventBus())
    {
        systemManager.getEventBus()->printStatistics();
    }
    else
    {
        Serial.println("Event bus not available");
    }
    Serial.println("========================");
}

void emergencyHandler()
{
    // This function can be called in case of critical errors
    // It will set the emergency stop flag
    emergencyStop = true;
}

void printSystemStatus()
{
    Serial.println("=== System Status ===");
    Serial.printf("Uptime: %lu ms\n", millis() - systemStartupTime);
    if (systemManager.getEventBus())
    {
        systemManager.getEventBus()->printStatistics();
    }
    Serial.println("====================");
}

void printDetailedStatistics()
{
    Serial.println("=== Detailed Statistics ===");
    if (systemManager.getEventBus())
    {
        systemManager.getEventBus()->printStatistics();
    }
    if (systemManager.getAudioProcessor())
    {
        systemManager.getAudioProcessor()->printStatistics();
    }
    Serial.println("===========================");
}

void printStateInfo()
{
    Serial.println("=== State Information ===");
    if (systemManager.getStateMachine())
    {
        Serial.println("State Machine available");
    }
    Serial.println("========================");
}