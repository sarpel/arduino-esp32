#ifndef AUTO_RECOVERY_H
#define AUTO_RECOVERY_H

#include <Arduino.h>
#include "../config.h"

// Recovery strategies
enum class RecoveryStrategy {
    RECONNECT_WIFI = 0,
    RECONNECT_TCP = 1,
    REINITIALIZE_I2S = 2,
    DEGRADE_MODE = 3,
    RESET_DEVICE = 4
};

// Automatic recovery coordination
class AutoRecovery {
private:
    uint32_t recovery_attempts;
    uint32_t successful_recoveries;
    unsigned long last_recovery_time;

public:
    AutoRecovery();

    // Determine recovery strategy based on failure type
    RecoveryStrategy getRecoveryStrategy(const String& component, const String& failure_reason);

    // Execute recovery
    bool executeRecovery(RecoveryStrategy strategy);

    // Statistics
    uint32_t getRecoveryAttempts() const { return recovery_attempts; }
    uint32_t getSuccessfulRecoveries() const { return successful_recoveries; }

    // Utility
    void printStatistics() const;

private:
    bool recoverWiFi();
    bool recoverTCP();
    bool recoverI2S();
    bool degrade();
};

#endif // AUTO_RECOVERY_H
