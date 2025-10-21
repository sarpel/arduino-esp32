#include "HealthMonitor.h"
#include "../core/SystemManager.h"

HealthMonitor::HealthMonitor() 
    : initialized(false), enable_predictions(true), auto_recovery_enabled(true),
      last_health_check(0), total_checks(0), failed_checks(0), auto_recoveries(0),
      critical_events(0) {}

HealthMonitor::~HealthMonitor() {
    shutdown();
}

bool HealthMonitor::initialize() {
    if (initialized) {
        return true;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "HealthMonitor", "Initializing HealthMonitor");
    }
    
    // Initialize health checks
    initializeHealthChecks();
    
    // Reset statistics
    total_checks = 0;
    failed_checks = 0;
    auto_recoveries = 0;
    critical_events = 0;
    last_health_check = millis();
    
    initialized = true;
    
    if (logger) {
        logger->log(LOG_INFO, "HealthMonitor", "HealthMonitor initialized with %u health checks",
                   health_checks.size());
    }
    
    return true;
}

void HealthMonitor::shutdown() {
    if (!initialized) {
        return;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "HealthMonitor", "Shutting down HealthMonitor");
        printStatistics();
    }
    
    clearHealthChecks();
    clearPredictions();
    clearHealthHistory();
    
    initialized = false;
}

void HealthMonitor::initializeHealthChecks() {
    // Memory health check
    addHealthCheck(HealthCheck(
        "memory",
        []() {
            auto memory_manager = SystemManager::getInstance().getMemoryManager();
            if (!memory_manager) return true;
            return memory_manager->getFreeMemory() > 20000;  // At least 20KB free
        },
        []() {
            auto memory_manager = SystemManager::getInstance().getMemoryManager();
            if (!memory_manager) return String("Memory manager not available");
            return String("Free memory: ") + memory_manager->getFreeMemory() + " bytes";
        },
        HealthStatus::CRITICAL,
        10000  // Check every 10 seconds
    ));
    
    // CPU health check
    addHealthCheck(HealthCheck(
        "cpu",
        []() {
            const auto& context = SystemManager::getInstance().getContext();
            return context.cpu_load_percent < 85.0f;  // Less than 85% CPU load
        },
        []() {
            const auto& context = SystemManager::getInstance().getContext();
            return String("CPU load: ") + context.cpu_load_percent + "%";
        },
        HealthStatus::POOR,
        5000  // Check every 5 seconds
    ));
    
    // Network health check
    addHealthCheck(HealthCheck(
        "network",
        []() {
            auto network_manager = SystemManager::getInstance().getNetworkManager();
            if (!network_manager) return false;
            return network_manager->isWiFiConnected() && 
                   network_manager->getNetworkStability() > 0.3f;
        },
        []() {
            auto network_manager = SystemManager::getInstance().getNetworkManager();
            if (!network_manager) return String("Network manager not available");
            return String("WiFi: ") + (network_manager->isWiFiConnected() ? "connected" : "disconnected") +
                   ", Stability: " + network_manager->getNetworkStability();
        },
        HealthStatus::POOR,
        15000  // Check every 15 seconds
    ));
    
    // Audio health check
    addHealthCheck(HealthCheck(
        "audio",
        []() {
            auto audio_processor = SystemManager::getInstance().getAudioProcessor();
            if (!audio_processor) return true;
            return audio_processor->getAudioQualityScore() > 0.5f;
        },
        []() {
            auto audio_processor = SystemManager::getInstance().getAudioProcessor();
            if (!audio_processor) return String("Audio processor not available");
            return String("Audio quality: ") + audio_processor->getAudioQualityScore();
        },
        HealthStatus::FAIR,
        20000  // Check every 20 seconds
    ));
    
    // Temperature health check
    addHealthCheck(HealthCheck(
        "temperature",
        []() {
            const auto& context = SystemManager::getInstance().getContext();
            return context.temperature < 75.0f;  // Less than 75°C
        },
        []() {
            const auto& context = SystemManager::getInstance().getContext();
            return String("Temperature: ") + context.temperature + "°C";
        },
        HealthStatus::CRITICAL,
        30000  // Check every 30 seconds
    ));
}

SystemHealth HealthMonitor::checkSystemHealth() {
    if (!initialized) {
        return SystemHealth();
    }
    
    SystemHealth health = calculateOverallHealth();
    health.timestamp = millis();
    
    // Update history
    updateHealthHistory(health);
    
    // Generate predictions if enabled
    if (enable_predictions) {
        generatePredictions();
    }
    
    // Check for auto recovery
    if (auto_recovery_enabled && health.status >= HealthStatus::POOR) {
        attemptRecovery();
    }
    
    return health;
}

void HealthMonitor::performHealthChecks() {
    if (!initialized) {
        return;
    }
    
    unsigned long current_time = millis();
    
    for (auto& check : health_checks) {
        // Check if it's time to run this check
        if (current_time - check.last_check >= check.check_interval) {
            performHealthCheck(check);
            check.last_check = current_time;
        }
    }
}

bool HealthMonitor::performHealthCheck(HealthCheck& check) {
    total_checks++;
    
    bool result = false;
    try {
        result = check.check_function();
    } catch (...) {
        result = false;
    }
    
    check.last_result = result;
    
    if (!result) {
        failed_checks++;
        
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LOG_WARN, "HealthMonitor", "Health check failed: %s", check.name.c_str());
        }
        
        // Handle critical failures
        if (check.failure_level >= HealthStatus::CRITICAL) {
            critical_events++;
            
            // Publish critical health event
            auto eventBus = SystemManager::getInstance().getEventBus();
            if (eventBus) {
                eventBus->publish(SystemEvent::SYSTEM_ERROR);
            }
        }
    }
    
    return result;
}

SystemHealth HealthMonitor::calculateOverallHealth() {
    SystemHealth health;
    
    // Get current system context
    const auto& context = SystemManager::getInstance().getContext();
    
    // CPU load
    health.cpu_load_percent = context.cpu_load_percent;
    
    // Memory pressure
    auto memory_manager = SystemManager::getInstance().getMemoryManager();
    if (memory_manager) {
        size_t free_mem = memory_manager->getFreeMemory();
        size_t total_mem = memory_manager->getTotalMemory();
        health.memory_pressure = 1.0f - (static_cast<float>(free_mem) / total_mem);
    }
    
    // Network stability
    auto network_manager = SystemManager::getInstance().getNetworkManager();
    if (network_manager) {
        health.network_stability = network_manager->getNetworkStability();
    }
    
    // Audio quality
    auto audio_processor = SystemManager::getInstance().getAudioProcessor();
    if (audio_processor) {
        health.audio_quality_score = audio_processor->getAudioQualityScore();
    }
    
    // Temperature
    health.temperature = context.temperature;
    
    // Calculate overall score
    health.overall_score = 1.0f;
    
    // Factor in CPU load
    if (health.cpu_load_percent > 50.0f) {
        health.overall_score *= (1.0f - (health.cpu_load_percent - 50.0f) / 100.0f);
    }
    
    // Factor in memory pressure
    health.overall_score *= (1.0f - health.memory_pressure * 0.5f);
    
    // Factor in network stability
    health.overall_score *= health.network_stability;
    
    // Factor in audio quality
    health.overall_score *= health.audio_quality_score;
    
    // Factor in temperature
    if (health.temperature > 60.0f) {
        health.overall_score *= (1.0f - (health.temperature - 60.0f) / 40.0f);
    }
    
    // Determine status
    health.status = determineHealthStatus(health);
    
    return health;
}

HealthStatus HealthMonitor::determineHealthStatus(const SystemHealth& health) {
    // Check critical thresholds first
    if (health.cpu_load_percent > thresholds.cpu_critical ||
        health.memory_pressure > thresholds.memory_critical ||
        health.network_stability < thresholds.network_critical ||
        health.audio_quality_score < thresholds.audio_critical ||
        health.temperature > thresholds.temperature_critical) {
        return HealthStatus::CRITICAL;
    }
    
    // Check overall score
    if (health.overall_score > 0.9f) return HealthStatus::EXCELLENT;
    if (health.overall_score > 0.7f) return HealthStatus::GOOD;
    if (health.overall_score > 0.5f) return HealthStatus::FAIR;
    if (health.overall_score > 0.3f) return HealthStatus::POOR;
    
    return HealthStatus::CRITICAL;
}

void HealthMonitor::updateHealthHistory(const SystemHealth& health) {
    health_history.push_back(health);
    
    // Keep only recent history
    while (health_history.size() > MAX_HISTORY_SIZE) {
        health_history.erase(health_history.begin());
    }
}

void HealthMonitor::generatePredictions() {
    if (health_history.size() < 5) {
        return;  // Need more data for predictions
    }
    
    predictions.clear();
    
    // Analyze trends in health data
    const SystemHealth& latest = health_history.back();
    
    // Predict memory issues
    if (latest.memory_pressure > 0.7f) {
        float probability = (latest.memory_pressure - 0.7f) / 0.3f;
        uint32_t time_to_failure = static_cast<uint32_t>((1.0f - probability) * 300);  // 0-300 seconds
        
        predictions.emplace_back(
            "memory",
            "memory_exhaustion",
            probability,
            time_to_failure,
            "Reduce memory usage or restart system"
        );
    }
    
    // Predict CPU overload
    if (latest.cpu_load_percent > 80.0f) {
        float probability = (latest.cpu_load_percent - 80.0f) / 20.0f;
        uint32_t time_to_failure = static_cast<uint32_t>((1.0f - probability) * 120);  // 0-120 seconds
        
        predictions.emplace_back(
            "cpu",
            "cpu_overload",
            probability,
            time_to_failure,
            "Reduce processing load or optimize code"
        );
    }
    
    // Predict network issues
    if (latest.network_stability < 0.4f) {
        float probability = (0.4f - latest.network_stability) / 0.4f;
        uint32_t time_to_failure = static_cast<uint32_t>((1.0f - probability) * 60);  // 0-60 seconds
        
        predictions.emplace_back(
            "network",
            "connection_failure",
            probability,
            time_to_failure,
            "Check network configuration and signal strength"
        );
    }
}

bool HealthMonitor::canAutoRecover() const {
    if (!auto_recovery_enabled) {
        return false;
    }
    
    // Check if we can recover from current state
    auto latest_health = getLatestHealth();
    
    // Can recover from poor health but not critical
    return latest_health.status == HealthStatus::POOR;
}

void HealthMonitor::attemptRecovery() {
    if (!canAutoRecover()) {
        return;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "HealthMonitor", "Attempting auto-recovery");
    }
    
    auto_recoveries++;
    
    // Perform recovery actions based on health issues
    auto latest_health = getLatestHealth();
    
    if (latest_health.memory_pressure > 0.6f) {
        // Memory recovery
        auto memory_manager = SystemManager::getInstance().getMemoryManager();
        if (memory_manager) {
            memory_manager->emergencyCleanup();
        }
    }
    
    if (latest_health.cpu_load_percent > 70.0f) {
        // CPU recovery - reduce load
        // This could involve reducing processing frequency
        auto eventBus = SystemManager::getInstance().getEventBus();
        if (eventBus) {
            eventBus->publish(SystemEvent::CPU_OVERLOAD);
        }
    }
    
    if (latest_health.network_stability < 0.5f) {
        // Network recovery
        auto network_manager = SystemManager::getInstance().getNetworkManager();
        if (network_manager) {
            network_manager->switchToBestWiFiNetwork();
        }
    }
    
    if (logger) {
        logger->log(LOG_INFO, "HealthMonitor", "Auto-recovery completed");
    }
}

void HealthMonitor::addHealthCheck(const HealthCheck& check) {
    health_checks.push_back(check);
}

void HealthMonitor::removeHealthCheck(const String& name) {
    health_checks.erase(
        std::remove_if(health_checks.begin(), health_checks.end(),
            [&name](const HealthCheck& check) { return check.name == name; }),
        health_checks.end()
    );
}

void HealthMonitor::clearHealthChecks() {
    health_checks.clear();
}

bool HealthMonitor::runHealthCheck(const String& name) {
    for (auto& check : health_checks) {
        if (check.name == name) {
            return performHealthCheck(check);
        }
    }
    return false;
}

void HealthMonitor::clearPredictions() {
    predictions.clear();
}

void HealthMonitor::clearHealthHistory() {
    health_history.clear();
}

SystemHealth HealthMonitor::getLatestHealth() const {
    return health_history.empty() ? SystemHealth() : health_history.back();
}

void HealthMonitor::printHealthStatus() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    auto latest_health = getLatestHealth();
    
    logger->log(LOG_INFO, "HealthMonitor", "=== Health Status ===");
    logger->log(LOG_INFO, "HealthMonitor", "Overall Score: %.2f", latest_health.overall_score);
    logger->log(LOG_INFO, "HealthMonitor", "Status: %s", getHealthStatusString(latest_health.status));
    logger->log(LOG_INFO, "HealthMonitor", "CPU Load: %.1f%%", latest_health.cpu_load_percent);
    logger->log(LOG_INFO, "HealthMonitor", "Memory Pressure: %.2f", latest_health.memory_pressure);
    logger->log(LOG_INFO, "HealthMonitor", "Network Stability: %.2f", latest_health.network_stability);
    logger->log(LOG_INFO, "HealthMonitor", "Audio Quality: %.2f", latest_health.audio_quality_score);
    logger->log(LOG_INFO, "HealthMonitor", "Temperature: %.1f°C", latest_health.temperature);
    logger->log(LOG_INFO, "HealthMonitor", "Predicted Failures: %u", latest_health.predicted_failures);
    logger->log(LOG_INFO, "HealthMonitor", "==================");
}

void HealthMonitor::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "HealthMonitor", "=== Health Monitor Statistics ===");
    logger->log(LOG_INFO, "HealthMonitor", "Total checks: %u", total_checks);
    logger->log(LOG_INFO, "HealthMonitor", "Failed checks: %u", failed_checks);
    logger->log(LOG_INFO, "HealthMonitor", "Success rate: %.1f%%", 
               total_checks > 0 ? (1.0f - static_cast<float>(failed_checks) / total_checks) * 100.0f : 100.0f);
    logger->log(LOG_INFO, "HealthMonitor", "Auto recoveries: %u", auto_recoveries);
    logger->log(LOG_INFO, "HealthMonitor", "Critical events: %u", critical_events);
    logger->log(LOG_INFO, "HealthMonitor", "Health checks: %u", health_checks.size());
    logger->log(LOG_INFO, "HealthMonitor", "Predictions: %u", predictions.size());
    logger->log(LOG_INFO, "HealthMonitor", "History size: %u", health_history.size());
    logger->log(LOG_INFO, "HealthMonitor", "================================");
}

void HealthMonitor::printPredictions() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "HealthMonitor", "=== Failure Predictions ===");
    
    if (predictions.empty()) {
        logger->log(LOG_INFO, "HealthMonitor", "No failure predictions at this time");
    } else {
        for (const auto& prediction : predictions) {
            logger->log(LOG_INFO, "HealthMonitor", "%s: %s (%.1f%%) in ~%u seconds",
                       prediction.component.c_str(),
                       prediction.failure_type.c_str(),
                       prediction.probability * 100.0f,
                       prediction.time_to_failure_seconds);
            logger->log(LOG_INFO, "HealthMonitor", "  Action: %s", prediction.recommended_action.c_str());
        }
    }
    
    logger->log(LOG_INFO, "HealthMonitor", "==========================");
}

String HealthMonitor::getHealthStatusString(HealthStatus status) const {
    switch (status) {
        case HealthStatus::EXCELLENT: return "EXCELLENT";
        case HealthStatus::GOOD: return "GOOD";
        case HealthStatus::FAIR: return "FAIR";
        case HealthStatus::POOR: return "POOR";
        case HealthStatus::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

bool HealthMonitor::isSystemHealthy() const {
    auto latest_health = getLatestHealth();
    return latest_health.status < HealthStatus::POOR;
}

HealthStatus HealthMonitor::getSystemHealthStatus() const {
    return getLatestHealth().status;
}

bool HealthMonitor::predictFailure(uint32_t time_horizon_seconds) {
    if (!enable_predictions) {
        return false;
    }
    
    // Simple failure prediction based on current trends
    auto latest_health = getLatestHealth();
    
    // Check if any metrics are trending toward failure
    if (latest_health.memory_pressure > 0.6f ||
        latest_health.cpu_load_percent > 75.0f ||
        latest_health.network_stability < 0.4f ||
        latest_health.audio_quality_score < 0.6f) {
        return true;
    }
    
    return false;
}

float HealthMonitor::getComponentHealthScore(const String& component) const {
    auto latest_health = getLatestHealth();
    
    if (component == "cpu") {
        return 1.0f - (latest_health.cpu_load_percent / 100.0f);
    } else if (component == "memory") {
        return 1.0f - latest_health.memory_pressure;
    } else if (component == "network") {
        return latest_health.network_stability;
    } else if (component == "audio") {
        return latest_health.audio_quality_score;
    }
    
    return 1.0f;  // Unknown component, assume healthy
}

std::vector<String> HealthMonitor::getUnhealthyComponents() const {
    std::vector<String> unhealthy;
    
    auto latest_health = getLatestHealth();
    
    if (latest_health.cpu_load_percent > 70.0f) {
        unhealthy.push_back("cpu");
    }
    
    if (latest_health.memory_pressure > 0.7f) {
        unhealthy.push_back("memory");
    }
    
    if (latest_health.network_stability < 0.5f) {
        unhealthy.push_back("network");
    }
    
    if (latest_health.audio_quality_score < 0.7f) {
        unhealthy.push_back("audio");
    }
    
    if (latest_health.temperature > 70.0f) {
        unhealthy.push_back("temperature");
    }
    
    return unhealthy;
}