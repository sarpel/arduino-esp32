#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include "SystemTypes.h"

// State transition reasons
enum class StateTransitionReason {
    NORMAL,           // Normal state progression
    TIMEOUT,          // State timeout exceeded
    ERROR_CONDITION,  // Error detected
    RECOVERY,         // Error recovery
    MANUAL,           // Manual intervention
    EMERGENCY,        // Emergency condition
    AUTOMATIC         // Automatic transition
};

// State entry/exit conditions
struct StateCondition {
    std::function<bool()> condition;
    const char* description;
    unsigned long timeout_ms;
    
    StateCondition(std::function<bool()> cond, const char* desc, unsigned long timeout = 0)
        : condition(cond), description(desc), timeout_ms(timeout) {}
};

// State configuration
struct StateConfig {
    SystemState state;
    std::vector<StateCondition> entry_conditions;
    std::vector<StateCondition> exit_conditions;
    unsigned long max_duration_ms;
    bool allow_manual_transition;
    bool auto_recovery_enabled;
    
    StateConfig(SystemState s) 
        : state(s), max_duration_ms(0), allow_manual_transition(true), 
          auto_recovery_enabled(true) {}
    
    StateConfig& withEntryCondition(std::function<bool()> condition, 
                                   const char* description, unsigned long timeout = 0) {
        entry_conditions.emplace_back(condition, description, timeout);
        return *this;
    }
    
    StateConfig& withExitCondition(std::function<bool()> condition, 
                                  const char* description, unsigned long timeout = 0) {
        exit_conditions.emplace_back(condition, description, timeout);
        return *this;
    }
    
    StateConfig& withMaxDuration(unsigned long duration_ms) {
        max_duration_ms = duration_ms;
        return *this;
    }
    
    StateConfig& withManualTransition(bool allowed) {
        allow_manual_transition = allowed;
        return *this;
    }
    
    StateConfig& withAutoRecovery(bool enabled) {
        auto_recovery_enabled = enabled;
        return *this;
    }
};

// State transition information
struct StateTransition {
    SystemState from_state;
    SystemState to_state;
    StateTransitionReason reason;
    unsigned long transition_time;
    const char* description;
    bool successful;
    
    StateTransition(SystemState from, SystemState to, StateTransitionReason r, 
                   const char* desc = "")
        : from_state(from), to_state(to), reason(r), 
          transition_time(millis()), description(desc), successful(true) {}
};

// State machine statistics
struct StateMachineStats {
    uint32_t total_transitions;
    uint32_t successful_transitions;
    uint32_t failed_transitions;
    uint32_t timeout_transitions;
    uint32_t error_transitions;
    std::map<SystemState, uint32_t> state_entry_counts;
    std::map<SystemState, uint64_t> state_durations_ms;
    SystemState current_state;
    SystemState previous_state;
    unsigned long current_state_start_time;
    unsigned long last_transition_time;
    
    StateMachineStats() 
        : total_transitions(0), successful_transitions(0), failed_transitions(0),
          timeout_transitions(0), error_transitions(0), current_state(SystemState::INITIALIZING),
          previous_state(SystemState::INITIALIZING), current_state_start_time(millis()),
          last_transition_time(millis()) {}
};

class StateMachine {
private:
    // Current state
    SystemState current_state;
    SystemState previous_state;
    unsigned long state_entry_time;
    unsigned long last_transition_time;
    
    // State configurations
    std::map<SystemState, StateConfig> state_configs;
    
    // State transition history
    std::vector<StateTransition> transition_history;
    static constexpr size_t MAX_HISTORY_SIZE = 50;
    
    // Callbacks
    std::function<void(SystemState, SystemState, StateTransitionReason)> state_change_callback;
    std::function<void(SystemState, const char*)> state_entry_callback;
    std::function<void(SystemState, const char*)> state_exit_callback;
    
    // Statistics
    StateMachineStats stats;
    
    // Configuration
    bool initialized;
    bool enable_history;
    bool enable_statistics;
    bool enable_callbacks;
    
    // Timing
    static constexpr unsigned long STATE_DEBOUNCE_MS = 100;
    static constexpr unsigned long DEFAULT_TIMEOUT_MS = 30000;
    
    // Internal methods
    bool validateTransition(SystemState from, SystemState to, StateTransitionReason reason);
    bool checkEntryConditions(SystemState state);
    bool checkExitConditions(SystemState state);
    void recordTransition(const StateTransition& transition);
    void updateStatistics(const StateTransition& transition);
    void cleanupHistory();
    const StateConfig* getStateConfig(SystemState state) const;
    bool isValidStateTransition(SystemState from, SystemState to) const;
    
public:
    StateMachine()
        : current_state(SystemState::INITIALIZING),
          previous_state(SystemState::INITIALIZING),
          state_entry_time(millis()),
          last_transition_time(millis()),
          initialized(false),
          enable_history(true),
          enable_statistics(true),
          enable_callbacks(true) {}
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // State configuration
    void configureState(const StateConfig& config);
    void configureDefaultStates();
    void clearStateConfiguration(SystemState state);
    
    // State management
    bool setState(SystemState new_state, StateTransitionReason reason = StateTransitionReason::NORMAL, 
                  const char* description = "");
    bool forceState(SystemState new_state, StateTransitionReason reason = StateTransitionReason::MANUAL,
                    const char* description = "");
    
    // State queries
    SystemState getCurrentState() const { return current_state; }
    SystemState getPreviousState() const { return previous_state; }
    unsigned long getStateDuration() const { return millis() - state_entry_time; }
    unsigned long getTimeSinceLastTransition() const { return millis() - last_transition_time; }
    
    // State condition checking
    bool canEnterState(SystemState state) const;
    bool canExitState(SystemState state = SystemState::INITIALIZING) const;
    bool hasStateTimedOut(SystemState state = SystemState::INITIALIZING) const;
    bool isStateValid(SystemState state) const;
    
    // State information
    String getStateName(SystemState state) const;
    String getCurrentStateName() const { return getStateName(current_state); }
    String getPreviousStateName() const { return getStateName(previous_state); }
    const char* getStateDescription(SystemState state) const;
    SystemState getStateFromName(const char* name) const;
    
    // Callback management
    void onStateChange(std::function<void(SystemState, SystemState, StateTransitionReason)> callback);
    void onStateEntry(std::function<void(SystemState, const char*)> callback);
    void onStateExit(std::function<void(SystemState, const char*)> callback);
    void removeCallbacks();
    
    // History management
    void enableHistory(bool enable) { enable_history = enable; }
    void clearHistory();
    std::vector<StateTransition> getTransitionHistory() const { return transition_history; }
    StateTransition getLastTransition() const;
    size_t getHistorySize() const { return transition_history.size(); }
    
    // Statistics
    void enableStatistics(bool enable) { enable_statistics = enable; }
    void resetStatistics();
    const StateMachineStats& getStatistics() const { return stats; }
    uint32_t getTransitionCount() const { return stats.total_transitions; }
    uint32_t getStateEntryCount(SystemState state) const;
    unsigned long getTotalTimeInState(SystemState state) const;
    float getStateSuccessRate(SystemState state) const;
    float getOverallSuccessRate() const;
    
    // Utility
    void printCurrentState() const;
    void printStatistics() const;
    void printHistory() const;
    bool isInState(SystemState state) const { return current_state == state; }
    bool wasInState(SystemState state) const { return previous_state == state; }
    
    // Validation
    bool validateStateMachine() const;
    std::vector<String> getValidationErrors() const;
    
private:
    // Default state configurations
    void configureInitializingState();
    void configureConnectingWiFiState();
    void configureConnectingServerState();
    void configureConnectedState();
    void configureDisconnectedState();
    void configureErrorState();
    void configureMaintenanceState();
};

// Global state machine access
#define STATE_MACHINE() (SystemManager::getInstance().getStateMachine())

// Convenience macros for state checking
#define IS_IN_STATE(state) (STATE_MACHINE()->isInState(state))
#define WAS_IN_STATE(state) (STATE_MACHINE()->wasInState(state))
#define CURRENT_STATE_NAME() (STATE_MACHINE()->getCurrentStateName())

#endif // STATE_MACHINE_H