#include "CircuitBreaker.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"

CircuitBreaker::CircuitBreaker(uint32_t fail_threshold, unsigned long timeout)
    : state(CircuitBreakerState::CLOSED), failure_count(0), success_count(0),
      state_change_time(millis()), timeout_ms(timeout), recovery_timeout_ms(60000),
      failure_threshold(fail_threshold), success_threshold(2) {}

bool CircuitBreaker::isRequestAllowed() {
    unsigned long current_time = millis();

    switch (state) {
        case CircuitBreakerState::CLOSED:
            return true;  // Allow all requests

        case CircuitBreakerState::OPEN:
            // Check if timeout expired
            if (current_time - state_change_time >= recovery_timeout_ms) {
                state = CircuitBreakerState::HALF_OPEN;
                state_change_time = current_time;
                auto logger = SystemManager::getInstance().getLogger();
                if (logger) {
                    logger->log(LogLevel::LOG_INFO, "CircuitBreaker", __FILE__, __LINE__,
                                "Transitioning to HALF_OPEN state");
                }
                return true;  // Allow test request
            }
            return false;  // Reject all requests

        case CircuitBreakerState::HALF_OPEN:
            return true;  // Allow single test request

        default:
            return false;
    }
}

void CircuitBreaker::recordSuccess() {
    auto logger = SystemManager::getInstance().getLogger();

    switch (state) {
        case CircuitBreakerState::CLOSED:
            failure_count = 0;  // Reset failures on success
            break;

        case CircuitBreakerState::HALF_OPEN:
            success_count++;
            if (success_count >= success_threshold) {
                state = CircuitBreakerState::CLOSED;
                failure_count = 0;
                success_count = 0;
                state_change_time = millis();
                if (logger) {
                    logger->log(LogLevel::LOG_INFO, "CircuitBreaker", __FILE__, __LINE__,
                                "Transitioning to CLOSED state");
                }
            }
            break;

        default:
            break;
    }
}

void CircuitBreaker::recordFailure() {
    unsigned long current_time = millis();
    auto logger = SystemManager::getInstance().getLogger();

    switch (state) {
        case CircuitBreakerState::CLOSED:
            failure_count++;
            if (failure_count >= failure_threshold) {
                state = CircuitBreakerState::OPEN;
                state_change_time = current_time;
                if (logger) {
                    logger->log(LogLevel::LOG_WARN, "CircuitBreaker", __FILE__, __LINE__,
                                "Circuit breaker OPEN after %u failures", failure_count);
                }
            }
            break;

        case CircuitBreakerState::HALF_OPEN:
            state = CircuitBreakerState::OPEN;
            state_change_time = current_time;
            success_count = 0;
            if (logger) {
                logger->log(LogLevel::LOG_WARN, "CircuitBreaker", __FILE__, __LINE__,
                            "Circuit breaker OPEN (test failed)");
            }
            break;

        default:
            break;
    }
}

void CircuitBreaker::reset() {
    state = CircuitBreakerState::CLOSED;
    failure_count = 0;
    success_count = 0;
    state_change_time = millis();

    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "CircuitBreaker", __FILE__, __LINE__,
                    "Circuit breaker reset");
    }
}

void CircuitBreaker::printStatus() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    const char* state_str = "UNKNOWN";
    switch (state) {
        case CircuitBreakerState::CLOSED:
            state_str = "CLOSED";
            break;
        case CircuitBreakerState::OPEN:
            state_str = "OPEN";
            break;
        case CircuitBreakerState::HALF_OPEN:
            state_str = "HALF_OPEN";
            break;
    }

    logger->log(LogLevel::LOG_INFO, "CircuitBreaker", __FILE__, __LINE__,
                "State: %s, Failures: %u, Successes: %u, Threshold: %u",
                state_str, failure_count, success_count, failure_threshold);
}
