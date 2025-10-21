#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#include <Arduino.h>
#include "../config.h"

// Circuit breaker states
enum class CircuitBreakerState {
    CLOSED = 0,      // Normal operation
    OPEN = 1,        // Reject requests
    HALF_OPEN = 2    // Test single request
};

// Circuit breaker for preventing cascading failures
class CircuitBreaker {
private:
    CircuitBreakerState state;
    uint32_t failure_count;
    uint32_t success_count;
    unsigned long state_change_time;
    unsigned long timeout_ms;
    unsigned long recovery_timeout_ms;

    uint32_t failure_threshold;
    uint32_t success_threshold;

public:
    CircuitBreaker(uint32_t fail_threshold = 5, unsigned long timeout = 30000);

    // Check if request should be allowed
    bool isRequestAllowed();

    // Record success/failure
    void recordSuccess();
    void recordFailure();

    // State management
    CircuitBreakerState getState() const { return state; }
    void reset();

    // Configuration
    void setFailureThreshold(uint32_t threshold) { failure_threshold = threshold; }
    void setTimeout(unsigned long timeout) { timeout_ms = timeout; }
    void setRecoveryTimeout(unsigned long timeout) { recovery_timeout_ms = timeout; }

    // Utility
    void printStatus() const;
};

#endif // CIRCUIT_BREAKER_H
