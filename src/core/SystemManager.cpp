#include "SystemManager.h"
#include "EventBus.h"
#include "StateMachine.h"
#include "../audio/AudioProcessor.h"
#include "../network/NetworkManager.h"
#include "../monitoring/HealthMonitor.h"
#include "../utils/EnhancedLogger.h"
#include "../utils/ConfigManager.h"
#include "../utils/MemoryManager.h"
#include "../i2s_audio.h"
#include "esp_task_wdt.h"

// Static member initialization
SystemManager* SystemManager::instance = nullptr;

SystemManager::SystemManager() 
    : system_initialized(false),
      system_running(false),
      emergency_stop(false),
      consecutive_errors(0),
      last_cycle_time(0),
      cycle_start_time(0) {
    
    // Initialize context
    context.uptime_start = millis();
    context.current_state = SystemState::INITIALIZING;
    context.previous_state = SystemState::INITIALIZING;
}

SystemManager& SystemManager::getInstance() {
    if (instance == nullptr) {
        instance = new SystemManager();
    }
    return *instance;
}

void SystemManager::destroyInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

bool SystemManager::initialize() {
    // Initialize watchdog with extended timeout for startup
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC * 2, true);
    esp_task_wdt_add(NULL);
    
    // Initialize components in dependency order
    if (!initializeLogger()) {
        return false;
    }
    
    logger->log(LOG_INFO, "SystemManager", "========================================");
    logger->log(LOG_INFO, "SystemManager", "ESP32 Audio Streamer v3.0 - System Startup");
    logger->log(LOG_INFO, "SystemManager", "Enhanced Architecture with Modular Design");
    logger->log(LOG_INFO, "SystemManager", "========================================");
    
    if (!initializeMemoryManager()) {
        logger->log(LOG_CRITICAL, "SystemManager", "MemoryManager initialization failed");
        return false;
    }
    
    if (!initializeConfigManager()) {
        logger->log(LOG_CRITICAL, "SystemManager", "ConfigManager initialization failed");
        return false;
    }
    
    if (!initializeEventBus()) {
        logger->log(LOG_CRITICAL, "SystemManager", "EventBus initialization failed");
        return false;
    }
    
    if (!initializeStateMachine()) {
        logger->log(LOG_CRITICAL, "SystemManager", "StateMachine initialization failed");
        return false;
    }
    
    if (!initializeAudioProcessor()) {
        logger->log(LOG_CRITICAL, "SystemManager", "AudioProcessor initialization failed");
        return false;
    }
    
    if (!initializeNetworkManager()) {
        logger->log(LOG_CRITICAL, "SystemManager", "NetworkManager initialization failed");
        return false;
    }
    
    if (!initializeHealthMonitor()) {
        logger->log(LOG_CRITICAL, "SystemManager", "HealthMonitor initialization failed");
        return false;
    }
    
    // Register event handlers
    event_bus->subscribe(SystemEvent::SYSTEM_ERROR, 
        [this](const void* data) { handleSystemEvent(SystemEvent::SYSTEM_ERROR, data); });
    event_bus->subscribe(SystemEvent::MEMORY_CRITICAL, 
        [this](const void* data) { handleHealthEvent(SystemEvent::MEMORY_CRITICAL, data); });
    event_bus->subscribe(SystemEvent::NETWORK_DISCONNECTED, 
        [this](const void* data) { handleNetworkEvent(SystemEvent::NETWORK_DISCONNECTED, data); });
    
    system_initialized = true;
    system_running = true;
    
    logger->log(LOG_INFO, "SystemManager", "System initialization completed successfully");
    logger->log(LOG_INFO, "SystemManager", "Free memory: %u bytes", context.free_memory);
    logger->log(LOG_INFO, "SystemManager", "Main loop frequency: %u Hz", MAIN_LOOP_FREQUENCY_HZ);
    
    return true;
}

bool SystemManager::initializeEventBus() {
    event_bus = std::make_unique<EventBus>();
    if (!event_bus->initialize()) {
        return false;
    }
    
    logger->log(LOG_INFO, "SystemManager", "EventBus initialized");
    return true;
}

bool SystemManager::initializeStateMachine() {
    state_machine = std::make_unique<StateMachine>();
    if (!state_machine->initialize()) {
        return false;
    }
    
    // Set up state change callback
    state_machine->onStateChange([this](SystemState from, SystemState to) {
        context.previous_state = from;
        context.current_state = to;
        logger->log(LOG_INFO, "SystemManager", "State transition: %s → %s",
                   state_machine->stateToString(from).c_str(),
                   state_machine->stateToString(to).c_str());
    });
    
    logger->log(LOG_INFO, "SystemManager", "StateMachine initialized");
    return true;
}

bool SystemManager::initializeAudioProcessor() {
    audio_processor = std::make_unique<AudioProcessor>();
    if (!audio_processor->initialize()) {
        return false;
    }
    
    logger->log(LOG_INFO, "SystemManager", "AudioProcessor initialized");
    return true;
}

bool SystemManager::initializeNetworkManager() {
    network_manager = std::make_unique<NetworkManager>();
    if (!network_manager->initialize()) {
        return false;
    }
    
    logger->log(LOG_INFO, "SystemManager", "NetworkManager initialized");
    return true;
}

bool SystemManager::initializeHealthMonitor() {
    health_monitor = std::make_unique<HealthMonitor>();
    if (!health_monitor->initialize()) {
        return false;
    }
    
    logger->log(LOG_INFO, "SystemManager", "HealthMonitor initialized");
    return true;
}

bool SystemManager::initializeLogger() {
    logger = std::make_unique<EnhancedLogger>();
    if (!logger->initialize()) {
        return false;
    }
    
    return true;
}

bool SystemManager::initializeConfigManager() {
    config_manager = std::make_unique<ConfigManager>();
    if (!config_manager->initialize()) {
        return false;
    }
    
    logger->log(LOG_INFO, "SystemManager", "ConfigManager initialized");
    return true;
}

bool SystemManager::initializeMemoryManager() {
    memory_manager = std::make_unique<MemoryManager>();
    if (!memory_manager->initialize()) {
        return false;
    }
    
    // Update initial memory stats
    updateMemoryStats();
    
    logger->log(LOG_INFO, "SystemManager", "MemoryManager initialized");
    return true;
}

void SystemManager::run() {
    if (!system_initialized) {
        logger->log(LOG_CRITICAL, "SystemManager", "System not initialized - cannot run");
        return;
    }
    
    if (!system_running) {
        logger->log(LOG_WARN, "SystemManager", "System not running - starting now");
        system_running = true;
    }
    
    cycle_start_time = millis();
    
    // Main system loop
    while (system_running) {
        // Feed watchdog
        esp_task_wdt_reset();
        
        // Check for emergency stop
        if (emergency_stop) {
            logger->log(LOG_CRITICAL, "SystemManager", "Emergency stop activated");
            emergencyShutdown();
            break;
        }
        
        // Update system context
        updateContext();
        
        // Perform health checks
        performHealthChecks();
        
        // Process events
        event_bus->processEvents();
        
        // Update components based on current state
        switch (state_machine->getCurrentState()) {
            case SystemState::INITIALIZING:
                // Should not reach here after initialization
                state_machine->setState(SystemState::CONNECTING_WIFI);
                break;
                
            case SystemState::CONNECTING_WIFI:
                network_manager->handleWiFiConnection();
                if (network_manager->isWiFiConnected()) {
                    state_machine->setState(SystemState::CONNECTING_SERVER);
                }
                break;
                
            case SystemState::CONNECTING_SERVER:
                if (!network_manager->isWiFiConnected()) {
                    state_machine->setState(SystemState::CONNECTING_WIFI);
                    break;
                }
                
                if (network_manager->connectToServer()) {
                    state_machine->setState(SystemState::CONNECTED);
                }
                break;
                
            case SystemState::CONNECTED:
                if (!network_manager->isWiFiConnected()) {
                    state_machine->setState(SystemState::CONNECTING_WIFI);
                    break;
                }
                
                if (!network_manager->isServerConnected()) {
                    state_machine->setState(SystemState::CONNECTING_SERVER);
                    break;
                }
                
                // Process audio streaming
                {
                    static uint8_t audio_buffer[I2S_BUFFER_SIZE];
                    size_t bytes_read = 0;
                    
                    if (audio_processor->readData(audio_buffer, I2S_BUFFER_SIZE, &bytes_read)) {
                        context.audio_samples_processed += bytes_read / 2;  // 16-bit samples
                        
                        if (network_manager->writeData(audio_buffer, bytes_read)) {
                            context.bytes_sent += bytes_read;
                        } else {
                            // Network write failed
                            state_machine->setState(SystemState::CONNECTING_SERVER);
                        }
                    } else {
                        // Audio read failed
                        context.audio_errors++;
                        if (context.audio_errors > MAX_CONSECUTIVE_FAILURES) {
                            logger->log(LOG_ERROR, "SystemManager", "Too many audio errors - reinitializing");
                            audio_processor->reinitialize();
                            context.audio_errors = 0;
                        }
                    }
                }
                break;
                
            case SystemState::ERROR:
                handleErrors();
                break;
                
            case SystemState::MAINTENANCE:
                // Reserved for future use
                delay(ERROR_RECOVERY_DELAY);
                break;
                
            case SystemState::DISCONNECTED:
                state_machine->setState(SystemState::CONNECTING_SERVER);
                break;
        }
        
        // Maintain timing - ensure consistent loop frequency
        unsigned long cycle_time = millis() - cycle_start_time;
        if (cycle_time < CYCLE_TIME_MS) {
            delay(CYCLE_TIME_MS - cycle_time);
        }
        
        cycle_start_time = millis();
        context.cycle_count++;
    }
    
    logger->log(LOG_INFO, "SystemManager", "Main loop terminated");
}

void SystemManager::updateContext() {
    // Update timing
    context.uptime_ms = millis() - context.uptime_start;
    
    // Update performance metrics
    measureCPULoad();
    updateMemoryStats();
    updateTemperature();
    
    // Update network metrics
    if (network_manager) {
        context.wifi_rssi = network_manager->getWiFiRSSI();
        context.network_stability = network_manager->getNetworkStability();
    }
}

void SystemManager::measureCPULoad() {
    static unsigned long last_measurement = 0;
    static uint32_t last_cycle_count = 0;
    
    unsigned long current_time = millis();
    if (current_time - last_measurement >= 1000) {  // Measure every second
        uint32_t cycles_per_second = context.cycle_count - last_cycle_count;
        context.cpu_load_percent = (cycles_per_second * 100.0f) / MAIN_LOOP_FREQUENCY_HZ;
        
        last_measurement = current_time;
        last_cycle_count = context.cycle_count;
    }
}

void SystemManager::updateMemoryStats() {
    context.free_memory = ESP.getFreeHeap();
    if (context.free_memory > context.peak_memory) {
        context.peak_memory = context.free_memory;
    }
}

void SystemManager::updateTemperature() {
    // ESP32 internal temperature sensor (if available)
    #ifdef CONFIG_IDF_TARGET_ESP32
    context.temperature = temperatureRead();
    #else
    context.temperature = 0.0f;  // Not available on all variants
    #endif
}

void SystemManager::performHealthChecks() {
    if (!health_monitor) return;
    
    auto health_status = health_monitor->checkSystemHealth();
    
    if (health_status.memory_pressure > 0.8f) {
        event_bus->publish(SystemEvent::MEMORY_LOW, &health_status);
    }
    
    if (health_status.memory_pressure > 0.9f) {
        event_bus->publish(SystemEvent::MEMORY_CRITICAL, &health_status);
    }
    
    if (health_status.cpu_load > 0.9f) {
        event_bus->publish(SystemEvent::CPU_OVERLOAD, &health_status);
    }
}

void SystemManager::handleSystemEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::SYSTEM_ERROR:
            consecutive_errors++;
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                logger->log(LOG_CRITICAL, "SystemManager", "Too many consecutive errors - entering safe mode");
                enterSafeMode();
            }
            break;
            
        case SystemEvent::SYSTEM_RECOVERY:
            consecutive_errors = 0;
            logger->log(LOG_INFO, "SystemManager", "System recovered from error state");
            break;
            
        default:
            break;
    }
}

void SystemManager::handleAudioEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::AUDIO_PROCESSING_ERROR:
            context.audio_errors++;
            logger->log(LOG_ERROR, "SystemManager", "Audio processing error detected");
            break;
            
        case SystemEvent::AUDIO_QUALITY_DEGRADED:
            logger->log(LOG_WARN, "SystemManager", "Audio quality degraded");
            break;
            
        default:
            break;
    }
}

void SystemManager::handleNetworkEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::NETWORK_DISCONNECTED:
            context.connection_drops++;
            logger->log(LOG_WARN, "SystemManager", "Network connection lost");
            break;
            
        default:
            break;
    }
}

void SystemManager::handleHealthEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::MEMORY_CRITICAL:
            logger->log(LOG_CRITICAL, "SystemManager", "Critical memory situation detected");
            memory_manager->emergencyCleanup();
            break;
            
        default:
            break;
    }
}

void SystemManager::handleErrors() {
    logger->log(LOG_ERROR, "SystemManager", "System in error state - attempting recovery");
    
    // Try to recover from error state
    if (health_monitor && health_monitor->canAutoRecover()) {
        health_monitor->attemptRecovery();
        state_machine->setState(SystemState::CONNECTING_WIFI);
        event_bus->publish(SystemEvent::SYSTEM_RECOVERY);
    } else {
        // Cannot auto-recover, enter safe mode
        enterSafeMode();
    }
}

void SystemManager::enterSafeMode() {
    logger->log(LOG_CRITICAL, "SystemManager", "Entering safe mode - minimal functionality");
    
    // Disable non-critical components
    if (audio_processor) audio_processor->setSafeMode(true);
    if (network_manager) network_manager->setSafeMode(true);
    
    // Set minimal operational state
    state_machine->setState(SystemState::MAINTENANCE);
}

void SystemManager::emergencyShutdown() {
    logger->log(LOG_CRITICAL, "SystemManager", "Emergency shutdown initiated");
    
    system_running = false;
    
    // Graceful component shutdown
    if (network_manager) network_manager->shutdown();
    if (audio_processor) audio_processor->shutdown();
    if (health_monitor) health_monitor->shutdown();
    if (logger) logger->shutdown();
    
    logger->log(LOG_CRITICAL, "SystemManager", "Emergency shutdown completed");
}

void SystemManager::shutdown() {
    logger->log(LOG_INFO, "SystemManager", "System shutdown initiated");
    
    system_running = false;
    
    // Print final statistics
    logger->log(LOG_INFO, "SystemManager", "========================================");
    logger->log(LOG_INFO, "SystemManager", "Final System Statistics:");
    logger->log(LOG_INFO, "SystemManager", "Uptime: %lu seconds", context.uptime_ms / 1000);
    logger->log(LOG_INFO, "SystemManager", "Cycles completed: %u", context.cycle_count);
    logger->log(LOG_INFO, "SystemManager", "Audio samples processed: %u", context.audio_samples_processed);
    logger->log(LOG_INFO, "SystemManager", "Bytes sent: %u", context.bytes_sent);
    logger->log(LOG_INFO, "SystemManager", "Total errors: %u", context.total_errors);
    logger->log(LOG_INFO, "SystemManager", "Fatal errors: %u", context.fatal_errors);
    logger->log(LOG_INFO, "SystemManager", "========================================");
    
    // Graceful component shutdown
    if (network_manager) network_manager->shutdown();
    if (audio_processor) audio_processor->shutdown();
    if (health_monitor) health_monitor->shutdown();
    if (config_manager) config_manager->shutdown();
    if (memory_manager) memory_manager->shutdown();
    if (event_bus) event_bus->shutdown();
    if (state_machine) state_machine->shutdown();
    if (logger) logger->shutdown();
    
    logger->log(LOG_INFO, "SystemManager", "System shutdown completed");
}

void SystemManager::reportError(const char* component, const char* error_msg, bool fatal) {
    context.total_errors++;
    if (fatal) {
        context.fatal_errors++;
    }
    
    logger->log(fatal ? LOG_CRITICAL : LOG_ERROR, "SystemManager", 
                "[%s] %s", component, error_msg);
    
    event_bus->publish(fatal ? SystemEvent::SYSTEM_ERROR : SystemEvent::SYSTEM_ERROR);
}

void SystemManager::recoverFromError() {
    consecutive_errors = 0;
    event_bus->publish(SystemEvent::SYSTEM_RECOVERY);
}

SystemState SystemManager::getCurrentState() const {
    return state_machine ? state_machine->getCurrentState() : SystemState::ERROR;
}