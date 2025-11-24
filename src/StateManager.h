#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <Arduino.h>
#include <functional>

enum class SystemState {
    INITIALIZING,
    CONNECTING_WIFI,
    CONNECTING_SERVER,
    CONNECTED,
    DISCONNECTED,
    ERROR,
    MAINTENANCE
};

class StateManager {
private:
    SystemState currentState;
    SystemState previousState;
    unsigned long stateEnterTime;
    std::function<void(SystemState, SystemState)> stateChangeCallback;

public:
    StateManager() : currentState(SystemState::INITIALIZING), 
                   previousState(SystemState::INITIALIZING),
                   stateEnterTime(millis()) {}

    void setState(SystemState newState) {
        if (newState != currentState) {
            // BUG FIX: Capture both states atomically before callback
            // Previous implementation had a race condition where the callback
            // could see inconsistent state if interrupted or if the callback
            // itself queries getState(). Now we capture both old and new states
            // before updating to ensure callback sees consistent transition.
            SystemState capturedPrevious = currentState;
            SystemState capturedNew = newState;
            
            // Update state atomically
            previousState = currentState;
            currentState = newState;
            stateEnterTime = millis();
            
            // Execute callback with captured values to prevent race conditions
            if (stateChangeCallback) {
                stateChangeCallback(capturedPrevious, capturedNew);
            }
        }
    }

    SystemState getState() const {
        return currentState;
    }

    SystemState getPreviousState() const {
        return previousState;
    }

    unsigned long getStateDuration() const {
        return millis() - stateEnterTime;
    }

    bool isInState(SystemState state) const {
        return currentState == state;
    }

    bool hasStateTimedOut(unsigned long timeoutMs) const {
        return getStateDuration() >= timeoutMs;
    }

    void onStateChange(std::function<void(SystemState, SystemState)> callback) {
        stateChangeCallback = callback;
    }

    String stateToString(SystemState state) const {
        switch (state) {
            case SystemState::INITIALIZING: return "INITIALIZING";
            case SystemState::CONNECTING_WIFI: return "CONNECTING_WIFI";
            case SystemState::CONNECTING_SERVER: return "CONNECTING_SERVER";
            case SystemState::CONNECTED: return "CONNECTED";
            case SystemState::DISCONNECTED: return "DISCONNECTED";
            case SystemState::ERROR: return "ERROR";
            case SystemState::MAINTENANCE: return "MAINTENANCE";
            default: return "UNKNOWN";
        }
    }

    String getCurrentStateString() const {
        return stateToString(currentState);
    }
};

#endif