#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "../core/SystemManager.h"

// Health status levels
enum class HealthStatus {
    EXCELLENT = 0,
    GOOD = 1,
    FAIR = 2,
    POOR = 3,
    CRITICAL = 4
};

// Recovery phase tracking
enum class RecoveryPhase {
    RECOVERY_IDLE = 0,
    RECOVERY_CLEANUP = 1,
    RECOVERY_DEFRAG = 2,
    RECOVERY_RETRY = 3,
    RECOVERY_FAILED = 4
};

// System health metrics
struct SystemHealth {
    float overall_score;           // 0.0 to 1.0
    float cpu_load_percent;
    float memory_pressure;
    float network_stability;
    float audio_quality_score;
    float temperature;
    uint32_t predicted_failures;
    HealthStatus status;
    unsigned long timestamp;
    
    SystemHealth() : overall_score(1.0f), cpu_load_percent(0.0f), memory_pressure(0.0f),
                    network_stability(1.0f), audio_quality_score(1.0f), temperature(0.0f),
                    predicted_failures(0), status(HealthStatus::EXCELLENT), timestamp(0) {}
};

// Health check component
struct HealthCheck {
    String name;
    std::function<bool()> check_function;
    std::function<String()> get_details;
    HealthStatus failure_level;
    unsigned long last_check;
    unsigned long check_interval;
    bool last_result;
    
    HealthCheck(const String& n, std::function<bool()> func, std::function<String()> details,
                HealthStatus level, unsigned long interval)
        : name(n), check_function(func), get_details(details), failure_level(level),
          last_check(0), check_interval(interval), last_result(true) {}
};

// Predictive analytics
struct FailurePrediction {
    String component;
    String failure_type;
    float probability;
    uint32_t time_to_failure_seconds;
    String recommended_action;
    unsigned long predicted_at;
    
    FailurePrediction(const String& comp, const String& type, float prob, uint32_t time,
                     const String& action)
        : component(comp), failure_type(type), probability(prob), 
          time_to_failure_seconds(time), recommended_action(action), predicted_at(millis()) {}
};

class HealthMonitor {
private:
    // Health checks
    std::vector<HealthCheck> health_checks;
    
    // Failure predictions
    std::vector<FailurePrediction> predictions;
    
    // Health history
    std::vector<SystemHealth> health_history;
    static constexpr size_t MAX_HISTORY_SIZE = 100;
    
    // Configuration
    bool initialized;
    bool enable_predictions;
    bool auto_recovery_enabled;
    unsigned long last_health_check;
    
    // Statistics
    uint32_t total_checks;
    uint32_t failed_checks;
    uint32_t auto_recoveries;
    uint32_t critical_events;
    
    // Thresholds
    struct HealthThresholds {
        float cpu_critical;
        float memory_critical;
        float network_critical;
        float audio_critical;
        float temperature_critical;

        HealthThresholds() : cpu_critical(90.0f), memory_critical(0.9f),
                           network_critical(0.3f), audio_critical(0.5f),
                           temperature_critical(80.0f) {}
    } thresholds;

    // Recovery state tracking
    RecoveryPhase recovery_phase;
    uint32_t recovery_attempt_count;
    unsigned long last_recovery_attempt;
    static constexpr uint32_t MAX_RECOVERY_ATTEMPTS = 3;
    static constexpr uint32_t RECOVERY_RETRY_DELAY_MS = 5000;  // 5 second exponential backoff base
    uint32_t next_recovery_delay_ms;
    
    // Internal methods
    void initializeHealthChecks();
    bool performHealthCheck(HealthCheck& check);
    void updateHealthHistory(const SystemHealth& health);
    SystemHealth calculateOverallHealth();
    void generatePredictions();
    bool canAutoRecoverFromFailure(const String& component);
    void performAutoRecovery(const String& component);
    HealthStatus determineHealthStatus(const SystemHealth& health);
    float calculateFailureProbability(const SystemHealth& health, const String& component);
    
public:
    HealthMonitor();
    ~HealthMonitor();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // Health monitoring
    SystemHealth checkSystemHealth();
    void performHealthChecks();
    bool isSystemHealthy() const;
    HealthStatus getSystemHealthStatus() const;
    
    // Health checks
    void addHealthCheck(const HealthCheck& check);
    void removeHealthCheck(const String& name);
    void clearHealthChecks();
    bool runHealthCheck(const String& name);
    std::vector<HealthCheck> getHealthChecks() const { return health_checks; }
    
    // Predictive analytics
    void enablePredictions(bool enable) { enable_predictions = enable; }
    bool arePredictionsEnabled() const { return enable_predictions; }
    std::vector<FailurePrediction> getFailurePredictions() const { return predictions; }
    void clearPredictions();
    
    // Auto recovery
    void enableAutoRecovery(bool enable) { auto_recovery_enabled = enable; }
    bool isAutoRecoveryEnabled() const { return auto_recovery_enabled; }
    bool canAutoRecover() const;
    void initiateRecovery();  // Start the recovery process
    void attemptRecovery();   // Execute one step of recovery (non-blocking)
    
    // Thresholds
    void setThresholds(const HealthThresholds& new_thresholds) { thresholds = new_thresholds; }
    const HealthThresholds& getThresholds() const { return thresholds; }
    
    // Statistics
    uint32_t getTotalChecks() const { return total_checks; }
    uint32_t getFailedChecks() const { return failed_checks; }
    uint32_t getAutoRecoveries() const { return auto_recoveries; }
    uint32_t getCriticalEvents() const { return critical_events; }
    
    // Health history
    std::vector<SystemHealth> getHealthHistory() const { return health_history; }
    SystemHealth getLatestHealth() const;
    void clearHealthHistory();
    
    // Utility
    void printHealthStatus() const;
    void printStatistics() const;
    void printPredictions() const;
    String getHealthStatusString(HealthStatus status) const;
    
    // Advanced features
    bool predictFailure(uint32_t time_horizon_seconds);
    float getComponentHealthScore(const String& component) const;
    std::vector<String> getUnhealthyComponents() const;
};

// Global health monitor access
#define HEALTH_MONITOR() (SystemManager::getInstance().getHealthMonitor())

#endif // HEALTH_MONITOR_H