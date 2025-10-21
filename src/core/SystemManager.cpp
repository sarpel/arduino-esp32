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
    context.uptime_ms = 0;
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
    
    logger->info( "SystemManager", "========================================");
    logger->info( "SystemManager", "ESP32 Audio Streamer v3.0 - System Startup");
    logger->info( "SystemManager", "Enhanced Architecture with Modular Design");
    logger->info( "SystemManager", "========================================");
    
    if (!initializeMemoryManager()) {
        logger->critical( "SystemManager", "MemoryManager initialization failed");
        return false;
    }
    
    if (!initializeConfigManager()) {
        logger->critical( "SystemManager", "ConfigManager initialization failed");
        return false;
    }
    
    if (!initializeEventBus()) {
        logger->critical( "SystemManager", "EventBus initialization failed");
        return false;
    }
    
    if (!initializeStateMachine()) {
        logger->critical( "SystemManager", "StateMachine initialization failed");
        return false;
    }
    
    if (!initializeAudioProcessor()) {
        logger->critical( "SystemManager", "AudioProcessor initialization failed");
        return false;
    }
    
    if (!initializeNetworkManager()) {
        logger->critical( "SystemManager", "NetworkManager initialization failed");
        return false;
    }
    
    if (!initializeHealthMonitor()) {
        logger->critical( "SystemManager", "HealthMonitor initialization failed");
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
    
    logger->info( "SystemManager", "System initialization completed successfully");
    logger->info( "SystemManager", "Free memory: %u bytes", context.free_memory);
    logger->info( "SystemManager", "Main loop frequency: %u Hz", MAIN_LOOP_FREQUENCY_HZ);
    
    return true;
}

bool SystemManager::initializeEventBus() {
    event_bus = std::unique_ptr<EventBus>(new EventBus());
    if (!event_bus->initialize()) {
        return false;
    }
    
    logger->info("SystemManager", "EventBus initialized");
    return true;
}

bool SystemManager::initializeStateMachine() {
    state_machine = std::unique_ptr<StateMachine>(new StateMachine());
    if (!state_machine->initialize()) {
        return false;
    }
    
    // Set up state change callback
    state_machine->onStateChange([this](SystemState from, SystemState to, StateTransitionReason reason) {
        context.previous_state = from;
        context.current_state = to;
        logger->info("SystemManager", "State transition from %d to %d",
                     static_cast<int>(from),
                     static_cast<int>(to));
    });
    
    logger->info("SystemManager", "StateMachine initialized");
    return true;
}

bool SystemManager::initializeAudioProcessor() {
    audio_processor = std::unique_ptr<AudioProcessor>(new AudioProcessor());
    if (!audio_processor->initialize()) {
        return false;
    }
    
    logger->info("SystemManager", "AudioProcessor initialized");
    return true;
}

bool SystemManager::initializeNetworkManager() {
    network_manager = std::unique_ptr<NetworkManager>(new NetworkManager());
    if (!network_manager->initialize()) {
        return false;
    }
    
    logger->info("SystemManager", "NetworkManager initialized");
    return true;
}

bool SystemManager::initializeHealthMonitor() {
    health_monitor = std::unique_ptr<HealthMonitor>(new HealthMonitor());
    if (!health_monitor->initialize()) {
        return false;
    }
    
    logger->info( "SystemManager", "HealthMonitor initialized");
    return true;
}

bool SystemManager::initializeLogger() {
    logger = std::unique_ptr<EnhancedLogger>(new EnhancedLogger());
    if (!logger->initialize()) {
        return false;
    }
    
    return true;
}

bool SystemManager::initializeConfigManager() {
    config_manager = std::unique_ptr<ConfigManager>(new ConfigManager());
    if (!config_manager->initialize()) {
        return false;
    }
    
    logger->info( "SystemManager", "ConfigManager initialized");
    return true;
}

bool SystemManager::initializeMemoryManager() {
    memory_manager = std::unique_ptr<MemoryManager>(new MemoryManager());
    if (!memory_manager->initialize()) {
        return false;
    }
    
    // Update initial memory stats
    updateMemoryStats();
    
    logger->info( "SystemManager", "MemoryManager initialized");
    return true;
}

void SystemManager::run() {
    if (!system_initialized) {
        logger->critical( "SystemManager", "System not initialized - cannot run");
        return;
    }
    
    if (!system_running) {
        logger->warn( "SystemManager", "System not running - starting now");
        system_running = true;
    }
    
    cycle_start_time = millis();
    
    // Feed watchdog
    esp_task_wdt_reset();
    
    // Check for emergency stop
    if (emergency_stop) {
        logger->critical( "SystemManager", "Emergency stop activated");
        emergencyShutdown();
        return;
    }
    
    // Check for state timeout
    SystemState current_state = state_machine->getCurrentState();
    uint32_t state_timeout = getStateTimeout(current_state);
    uint32_t state_duration = state_machine->getStateDuration();
    
    if (state_timeout > 0 && state_duration > state_timeout) {
        logger->warn( "SystemManager", "State timeout detected");
        logger->info( "SystemManager", "Current state: %s, Duration: %lu ms, Timeout: %lu ms",
                     state_machine->getCurrentStateName().c_str(), state_duration, state_timeout);
        
        // Log diagnostic information
        logger->info( "SystemManager", "Memory: Free=%lu bytes, CPU=%f%%",
                     context.free_memory, context.cpu_load_percent);
        logger->info( "SystemManager", "Network: WiFi=%s, Server=%s, RSSI=%d",
                     network_manager->isWiFiConnected() ? "connected" : "disconnected",
                     network_manager->isServerConnected() ? "connected" : "disconnected",
                     context.wifi_rssi);
        logger->info( "SystemManager", "Errors: Total=%u, Recovered=%u, Fatal=%u",
                     context.total_errors, context.recovered_errors, context.fatal_errors);
        
        // Transition to ERROR state for timeout
        state_machine->setState(SystemState::ERROR, StateTransitionReason::TIMEOUT);
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
                        logger->error( "SystemManager", "Too many audio errors - reinitializing");
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
    
    context.cycle_count++;
}

void SystemManager::updateContext() {
    // Update timing (uptime is tracked in milliseconds)
    static unsigned long system_start_time = millis();
    context.uptime_ms = millis() - system_start_time;
    
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


uint32_t SystemManager::getStateTimeout(SystemState state) const {
    switch (state) {
        case SystemState::INITIALIZING:
            return INITIALIZING_TIMEOUT_MS;
        case SystemState::CONNECTING_WIFI:
            return WIFI_CONNECT_TIMEOUT_MS;
        case SystemState::CONNECTING_SERVER:
            return SERVER_CONNECT_TIMEOUT_MS;
        case SystemState::CONNECTED:
            return 0;  // No timeout for CONNECTED state
        case SystemState::DISCONNECTED:
            return 0;  // No timeout for DISCONNECTED state
        case SystemState::ERROR:
            return ERROR_RECOVERY_TIMEOUT_MS;
        case SystemState::MAINTENANCE:
            return 60000;  // 60 seconds for maintenance
        default:
            return 30000;  // Default 30 second timeout
    }
}

void SystemManager::performHealthChecks() {
    if (!health_monitor) return;
    
    auto health_status = health_monitor->checkSystemHealth();
    
    if (health_status.memory_pressure > 0.8f) {
        event_bus->publish(SystemEvent::MEMORY_LOW, &health_status);
    }
    
    if (health_status.memory_pressure > 0.9f) {
        event_bus->publish(SystemEvent::MEMORY_CRITICAL, &health_status);
        // Trigger recovery on critical memory pressure
        if (!health_monitor->canAutoRecover() && 
            health_status.status == HealthStatus::CRITICAL) {
            health_monitor->initiateRecovery();
        }
    }
    
    static unsigned long last_cpu_warning = 0;
    if (health_status.cpu_load_percent > 0.95f) {
        if (millis() - last_cpu_warning > 60000) {
            event_bus->publish(SystemEvent::CPU_OVERLOAD, &health_status);
            last_cpu_warning = millis();
        }
    }
    
    // Execute one step of recovery if in progress (non-blocking)
    health_monitor->attemptRecovery();
}

void SystemManager::handleSystemEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::SYSTEM_ERROR:
            consecutive_errors++;
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                logger->critical( "SystemManager", "Too many consecutive errors - entering safe mode");
                enterSafeMode();
            }
            break;
            
        case SystemEvent::SYSTEM_RECOVERY:
            consecutive_errors = 0;
            logger->info( "SystemManager", "System recovered from error state");
            break;
            
        default:
            break;
    }
}

void SystemManager::handleAudioEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::AUDIO_PROCESSING_ERROR:
            context.audio_errors++;
            logger->error( "SystemManager", "Audio processing error detected");
            break;
            
        case SystemEvent::AUDIO_QUALITY_DEGRADED:
            logger->warn( "SystemManager", "Audio quality degraded");
            break;
            
        default:
            break;
    }
}

void SystemManager::handleNetworkEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::NETWORK_DISCONNECTED:
            context.connection_drops++;
            logger->warn( "SystemManager", "Network connection lost");
            break;
            
        default:
            break;
    }
}

void SystemManager::handleHealthEvent(SystemEvent event, const void* data) {
    switch (event) {
        case SystemEvent::MEMORY_CRITICAL:
            logger->critical( "SystemManager", "Critical memory situation detected");
            memory_manager->emergencyCleanup();
            break;
            
        default:
            break;
    }
}

void SystemManager::handleErrors() {
    logger->error( "SystemManager", "System in error state - attempting recovery");
    
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
    logger->critical( "SystemManager", "Entering safe mode - minimal functionality");
    
    // Disable non-critical components
    if (audio_processor) audio_processor->setSafeMode(true);
    if (network_manager) network_manager->setSafeMode(true);
    
    // Set minimal operational state
    state_machine->setState(SystemState::MAINTENANCE);
}

void SystemManager::emergencyShutdown() {
    logger->critical( "SystemManager", "Emergency shutdown initiated");
    
    system_running = false;
    
    // Graceful component shutdown
    if (network_manager) network_manager->shutdown();
    if (audio_processor) audio_processor->shutdown();
    if (health_monitor) health_monitor->shutdown();
    if (logger) logger->shutdown();
    
    logger->critical( "SystemManager", "Emergency shutdown completed");
}

void SystemManager::shutdown() {
    logger->info( "SystemManager", "System shutdown initiated");
    
    system_running = false;
    
    // Print final statistics
    logger->info( "SystemManager", "========================================");
    logger->info( "SystemManager", "Final System Statistics:");
    logger->info( "SystemManager", "Uptime: %lu seconds", context.uptime_ms / 1000);
    logger->info( "SystemManager", "Cycles completed: %u", context.cycle_count);
    logger->info( "SystemManager", "Audio samples processed: %u", context.audio_samples_processed);
    logger->info( "SystemManager", "Bytes sent: %u", context.bytes_sent);
    logger->info( "SystemManager", "Total errors: %u", context.total_errors);
    logger->info( "SystemManager", "Fatal errors: %u", context.fatal_errors);
    logger->info( "SystemManager", "========================================");
    
    // Graceful component shutdown
    if (network_manager) network_manager->shutdown();
    if (audio_processor) audio_processor->shutdown();
    if (health_monitor) health_monitor->shutdown();
    if (config_manager) config_manager->shutdown();
    if (memory_manager) memory_manager->shutdown();
    if (event_bus) event_bus->shutdown();
    if (state_machine) state_machine->shutdown();
    if (logger) logger->shutdown();
    
    logger->info( "SystemManager", "System shutdown completed");
}

void SystemManager::reportError(const char* component, const char* error_msg, bool fatal) {
    context.total_errors++;
    if (fatal) {
        context.fatal_errors++;
    }
    
    if (fatal) {
        logger->critical("SystemManager", "[%s] %s", component, error_msg);
    } else {
        logger->error("SystemManager", "[%s] %s", component, error_msg);
    }
    
    event_bus->publish(fatal ? SystemEvent::SYSTEM_ERROR : SystemEvent::SYSTEM_ERROR);
}

void SystemManager::recoverFromError() {
    consecutive_errors = 0;
    event_bus->publish(SystemEvent::SYSTEM_RECOVERY);
}

SystemState SystemManager::getCurrentState() const {
    return state_machine ? state_machine->getCurrentState() : SystemState::ERROR;
}