#ifndef DEGRADATION_MANAGER_H
#define DEGRADATION_MANAGER_H

#include <Arduino.h>
#include "../config.h"

// Degradation modes
enum class DegradationMode {
    NORMAL = 0,            // Full features, 16kHz/16-bit audio
    REDUCED_QUALITY = 1,   // 8kHz/8-bit audio, reduced telemetry
    SAFE_MODE = 2,         // Audio streaming only
    RECOVERY = 3           // No streaming, focus on recovery
};

// Manages system degradation under adverse conditions
class DegradationManager {
private:
    DegradationMode current_mode;
    DegradationMode previous_mode;
    unsigned long last_mode_change;
    unsigned long mode_transition_delay;

    // Thresholds
    int health_degrade_threshold;      // Health < 80% → REDUCED_QUALITY
    int health_safe_threshold;         // Health < 60% → SAFE_MODE
    int health_recovery_threshold;     // 3 consecutive failures → RECOVERY
    int health_restore_threshold;      // Health > 85% for 60s → restore

    uint32_t consecutive_failures;

public:
    DegradationManager();

    // Mode management
    void updateMode(int current_health_score);
    void setMode(DegradationMode new_mode);
    void recordFailure();
    void recordSuccess();

    // Mode queries
    DegradationMode getCurrentMode() const { return current_mode; }
    DegradationMode getPreviousMode() const { return previous_mode; }
    bool isFeatureEnabled(const String& feature_name) const;

    // Configuration
    void setHealthThresholds(int degrade, int safe_mode, int recovery, int restore);

    // Utility
    void printStatus() const;

private:
    bool shouldTransition(int health_score, DegradationMode from, DegradationMode to);
    String modeToString(DegradationMode mode) const;
};

#endif // DEGRADATION_MANAGER_H
