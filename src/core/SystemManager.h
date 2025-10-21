#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <memory>
#include "SystemTypes.h"
#include "../config.h"

// Forward declarations
class EventBus;
class StateMachine;
class AudioProcessor;
class NetworkManager;
class HealthMonitor;
class EnhancedLogger;
class ConfigManager;
class MemoryManager;

// Forward declarations
class AudioProcessor;
class NetworkManager;
class HealthMonitor;
class EnhancedLogger;
class ConfigManager;
class MemoryManager;

enum class SystemEvent {
    // System events
    SYSTEM_STARTUP,
    SYSTEM_SHUTDOWN,
    SYSTEM_ERROR,
    SYSTEM_RECOVERY,
    
    // Audio events
    AUDIO_DATA_AVAILABLE,
    AUDIO_PROCESSING_ERROR,
    AUDIO_QUALITY_DEGRADED,
    
    // Network events
    NETWORK_CONNECTED,
    NETWORK_DISCONNECTED,
    NETWORK_QUALITY_CHANGED,
    SERVER_CONNECTED,
    SERVER_DISCONNECTED,
    
    // Health events
    MEMORY_LOW,
    MEMORY_CRITICAL,
    CPU_OVERLOAD,
    TEMPERATURE_HIGH,
    
    // Configuration events
    CONFIG_CHANGED,
    CONFIG_INVALID,
    PROFILE_LOADED,
    
    // Security events
    SECURITY_BREACH,
    AUTHENTICATION_FAILED,
    ENCRYPTION_ERROR
};

struct SystemContext {
    // System state
    SystemState current_state;
    SystemState previous_state;
    unsigned long uptime_ms;
    uint32_t cycle_count;
    
    // Performance metrics
    float cpu_load_percent;
    uint32_t free_memory;
    uint32_t peak_memory;
    float temperature;
    
    // Audio metrics
    uint32_t audio_samples_processed;
    uint32_t audio_errors;
    float audio_quality_score;
    
    // Network metrics
    int wifi_rssi;
    uint32_t bytes_sent;
    uint32_t connection_drops;
    float network_stability;
    
    // Error tracking
    uint32_t total_errors;
    uint32_t recovered_errors;
    uint32_t fatal_errors;
    
    SystemContext() : current_state(SystemState::INITIALIZING),
                     previous_state(SystemState::INITIALIZING),
                     uptime_ms(0), cycle_count(0),
                     cpu_load_percent(0.0f), free_memory(0),
                     peak_memory(0), temperature(0.0f),
                     audio_samples_processed(0), audio_errors(0),
                     audio_quality_score(1.0f), wifi_rssi(0),
                     bytes_sent(0), connection_drops(0),
                     network_stability(1.0f), total_errors(0),
                     recovered_errors(0), fatal_errors(0) {}
};

class SystemManager {
private:
    static SystemManager* instance;
    
    // Core components
    std::unique_ptr<EventBus> event_bus;
    std::unique_ptr<StateMachine> state_machine;
    std::unique_ptr<AudioProcessor> audio_processor;
    std::unique_ptr<NetworkManager> network_manager;
    std::unique_ptr<HealthMonitor> health_monitor;
    std::unique_ptr<EnhancedLogger> logger;
    std::unique_ptr<ConfigManager> config_manager;
    std::unique_ptr<MemoryManager> memory_manager;
    
    // System context
    SystemContext context;
    
    // Timing and scheduling
    unsigned long last_cycle_time;
    unsigned long cycle_start_time;
    static constexpr uint32_t MAIN_LOOP_FREQUENCY_HZ = 100;  // 100Hz main loop
    static constexpr uint32_t CYCLE_TIME_MS = 1000 / MAIN_LOOP_FREQUENCY_HZ;
    
    // System control
    bool system_initialized;
    bool system_running;
    bool emergency_stop;
    uint32_t consecutive_errors;
    static constexpr uint32_t MAX_CONSECUTIVE_ERRORS = 10;
    
    // Private constructor for singleton
    SystemManager();
    
    // Initialization methods
    bool initializeEventBus();
    bool initializeStateMachine();
    bool initializeAudioProcessor();
    bool initializeNetworkManager();
    bool initializeHealthMonitor();
    bool initializeLogger();
    bool initializeConfigManager();
    bool initializeMemoryManager();
    
    // Event handlers
    void handleSystemEvent(SystemEvent event, const void* data = nullptr);
    void handleAudioEvent(SystemEvent event, const void* data = nullptr);
    void handleNetworkEvent(SystemEvent event, const void* data = nullptr);
    void handleHealthEvent(SystemEvent event, const void* data = nullptr);
    
    // System maintenance
    void updateContext();
    void performHealthChecks();
    void handleErrors();
    void enterSafeMode();
    void emergencyShutdown();
    
    // Performance monitoring
    void measureCPULoad();
    void updateMemoryStats();
    void updateTemperature();
    
public:
    // Singleton access
    static SystemManager& getInstance();
    static void destroyInstance();
    
    // Lifecycle management
    bool initialize();
    void run();  // Main system loop
    void shutdown();
    
    // Component access
    EventBus* getEventBus() { return event_bus.get(); }
    StateMachine* getStateMachine() { return state_machine.get(); }
    AudioProcessor* getAudioProcessor() { return audio_processor.get(); }
    NetworkManager* getNetworkManager() { return network_manager.get(); }
    HealthMonitor* getHealthMonitor() { return health_monitor.get(); }
    EnhancedLogger* getLogger() { return logger.get(); }
    ConfigManager* getConfigManager() { return config_manager.get(); }
    MemoryManager* getMemoryManager() { return memory_manager.get(); }
    
    // System information
    const SystemContext& getContext() const { return context; }
    SystemState getCurrentState() const;
    bool isRunning() const { return system_running; }
    bool isInitialized() const { return system_initialized; }
    
    // Emergency control
    void emergencyStop() { emergency_stop = true; }
    void clearEmergencyStop() { emergency_stop = false; }
    bool isEmergencyStop() const { return emergency_stop; }
    
    // Statistics
    uint32_t getCycleCount() const { return context.cycle_count; }
    unsigned long getUptime() const { return context.uptime_ms; }
    float getCPULoad() const { return context.cpu_load_percent; }
    uint32_t getFreeMemory() const { return context.free_memory; }
    
    // Error handling
    void reportError(const char* component, const char* error_msg, bool fatal = false);
    void recoverFromError();
    uint32_t getErrorCount() const { return context.total_errors; }
    uint32_t getFatalErrorCount() const { return context.fatal_errors; }
};

// Global system manager access
#define SYSTEM() SystemManager::getInstance()

#endif // SYSTEM_MANAGER_H