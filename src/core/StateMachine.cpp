#include "StateMachine.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"
#include "../network/NetworkManager.h"
#include "../monitoring/HealthMonitor.h"

bool StateMachine::initialize() {
    if (initialized) {
        return true;
    }
    
    // Configure default states
    configureDefaultStates();
    
    // Initialize statistics
    resetStatistics();
    
    // Clear history
    clearHistory();
    
    // Set initial state
    current_state = SystemState::INITIALIZING;
    previous_state = SystemState::INITIALIZING;
    state_entry_time = millis();
    last_transition_time = millis();
    
    initialized = true;
    
    // Log initialization
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "StateMachine", "StateMachine initialized - current state: %s",
                   getCurrentStateName().c_str());
    }
    
    return true;
}

void StateMachine::shutdown() {
    if (!initialized) {
        return;
    }
    
    // Log shutdown
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "StateMachine", "StateMachine shutting down - final state: %s",
                   getCurrentStateName().c_str());
        printStatistics();
    }
    
    // Clear callbacks
    removeCallbacks();
    
    // Clear configurations
    state_configs.clear();
    
    // Clear history
    clearHistory();
    
    initialized = false;
}

void StateMachine::configureState(const StateConfig& config) {
    state_configs[config.state] = config;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_DEBUG, "StateMachine", "Configured state %s with %u entry conditions, %u exit conditions",
                   getStateName(config.state).c_str(), 
                   config.entry_conditions.size(), 
                   config.exit_conditions.size());
    }
}

void StateMachine::configureDefaultStates() {
    configureInitializingState();
    configureConnectingWiFiState();
    configureConnectingServerState();
    configureConnectedState();
    configureDisconnectedState();
    configureErrorState();
    configureMaintenanceState();
}

void StateMachine::configureInitializingState() {
    StateConfig config(SystemState::INITIALIZING);
    config.withMaxDuration(10000)  // 10 seconds max
          .withAutoRecovery(true)
          .withEntryCondition(
              []() { return SystemManager::getInstance().isInitialized(); },
              "SystemManager must be initialized", 5000);
    
    configureState(config);
}

void StateMachine::configureConnectingWiFiState() {
    StateConfig config(SystemState::CONNECTING_WIFI);
    config.withMaxDuration(60000)  // 1 minute max
          .withAutoRecovery(true)
          .withEntryCondition(
              []() { return SystemManager::getInstance().getNetworkManager() != nullptr; },
              "NetworkManager must be available")
          .withExitCondition(
              []() { 
                  auto net_manager = SystemManager::getInstance().getNetworkManager();
                  return net_manager && net_manager->isWiFiConnected(); 
              },
              "WiFi connection established");
    
    configureState(config);
}

void StateMachine::configureConnectingServerState() {
    StateConfig config(SystemState::CONNECTING_SERVER);
    config.withMaxDuration(120000)  // 2 minutes max
          .withAutoRecovery(true)
          .withEntryCondition(
              []() { 
                  auto net_manager = SystemManager::getInstance().getNetworkManager();
                  return net_manager && net_manager->isWiFiConnected(); 
              },
              "WiFi must be connected")
          .withExitCondition(
              []() { 
                  auto net_manager = SystemManager::getInstance().getNetworkManager();
                  return net_manager && net_manager->isServerConnected(); 
              },
              "Server connection established");
    
    configureState(config);
}

void StateMachine::configureConnectedState() {
    StateConfig config(SystemState::CONNECTED);
    config.withMaxDuration(0)  // No timeout - can stay connected indefinitely
          .withAutoRecovery(true)
          .withEntryCondition(
              []() { 
                  auto net_manager = SystemManager::getInstance().getNetworkManager();
                  return net_manager && net_manager->isServerConnected(); 
              },
              "Server must be connected")
          .withExitCondition(
              []() { 
                  auto net_manager = SystemManager::getInstance().getNetworkManager();
                  return !net_manager || !net_manager->isServerConnected() || !net_manager->isWiFiConnected(); 
              },
              "Connection lost");
    
    configureState(config);
}

void StateMachine::configureDisconnectedState() {
    StateConfig config(SystemState::DISCONNECTED);
    config.withMaxDuration(30000)  // 30 seconds max
          .withAutoRecovery(true)
          .withEntryCondition(
              []() { return true; },  // Can always enter disconnected state
              "Always allowed")
          .withExitCondition(
              []() { return true; },  // Can always exit to retry connection
              "Ready to reconnect");
    
    configureState(config);
}

void StateMachine::configureErrorState() {
    StateConfig config(SystemState::ERROR);
    config.withMaxDuration(60000)  // 1 minute max in error state
          .withAutoRecovery(true)
          .withEntryCondition(
              []() { return true; },  // Can always enter error state
              "Error condition detected")
          .withExitCondition(
              []() { 
                  auto health_monitor = SystemManager::getInstance().getHealthMonitor();
                  return health_monitor && health_monitor->canAutoRecover(); 
              },
              "Recovery conditions met", 30000);
    
    configureState(config);
}

void StateMachine::configureMaintenanceState() {
    StateConfig config(SystemState::MAINTENANCE);
    config.withMaxDuration(0)  // No timeout - manual exit required
          .withAutoRecovery(false)  // No auto-recovery from maintenance
          .withManualTransition(true)
          .withEntryCondition(
              []() { return true; },  // Can always enter maintenance
              "Maintenance mode requested");
    
    configureState(config);
}

bool StateMachine::setState(SystemState new_state, StateTransitionReason reason, const char* description) {
    if (!initialized) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_ERROR, "StateMachine", __FILE__, __LINE__, "Cannot set state - StateMachine not initialized");
        }
        return false;
    }
    
    // Check if this is actually a state change
    if (new_state == current_state) {
        return true;  // Already in this state
    }
    
    // Validate the transition
    if (!validateTransition(current_state, new_state, reason)) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_WARN, "StateMachine", __FILE__, __LINE__, "State transition %s → %s not allowed (reason: %u)",
                       getCurrentStateName().c_str(), getStateName(new_state).c_str(), 
                       static_cast<unsigned int>(reason));
        }
        return false;
    }
    
    // Check exit conditions for current state
    if (!checkExitConditions(current_state)) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_WARN, "StateMachine", __FILE__, __LINE__, "Cannot exit current state %s - conditions not met",
                       getCurrentStateName().c_str());
        }
        return false;
    }
    
    // Check entry conditions for new state
    if (!checkEntryConditions(new_state)) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_WARN, "StateMachine", __FILE__, __LINE__, "Cannot enter state %s - conditions not met",
                       getStateName(new_state).c_str());
        }
        return false;
    }
    
    // Perform the state transition
    SystemState old_state = current_state;
    previous_state = current_state;
    current_state = new_state;
    
    // Update timing
    last_transition_time = millis();
    state_entry_time = millis();
    
    // Record the transition
    StateTransition transition(old_state, new_state, reason, description);
    recordTransition(transition);
    
    // Update statistics
    if (enable_statistics) {
        updateStatistics(transition);
    }
    
    // Call callbacks
    if (enable_callbacks) {
        if (state_exit_callback) {
            state_exit_callback(old_state, description);
        }
        
        if (state_change_callback) {
            state_change_callback(old_state, new_state, reason);
        }
        
        if (state_entry_callback) {
            state_entry_callback(new_state, description);
        }
    }
    
    // Log the transition
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "State transition: %s → %s (reason: %u, desc: %s)",
                   getStateName(old_state).c_str(), getStateName(new_state).c_str(),
                   static_cast<unsigned int>(reason), description ? description : "none");
    }
    
    return true;
}

bool StateMachine::forceState(SystemState new_state, StateTransitionReason reason, const char* description) {
    if (!initialized) {
        return false;
    }
    
    // Override normal validation and force the state change
    SystemState old_state = current_state;
    previous_state = current_state;
    current_state = new_state;
    
    // Update timing
    last_transition_time = millis();
    state_entry_time = millis();
    
    // Record the transition
    StateTransition transition(old_state, new_state, reason, description);
    transition.successful = true;
    recordTransition(transition);
    
    // Update statistics
    if (enable_statistics) {
        updateStatistics(transition);
    }
    
    // Log the forced transition
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_WARN, "StateMachine", __FILE__, __LINE__, "Forced state transition: %s → %s (reason: %u, desc: %s)",
                   getStateName(old_state).c_str(), getStateName(new_state).c_str(),
                   static_cast<unsigned int>(reason), description ? description : "none");
    }
    
    return true;
}

bool StateMachine::validateTransition(SystemState from, SystemState to, StateTransitionReason reason) {
    // Allow manual transitions
    if (reason == StateTransitionReason::MANUAL || reason == StateTransitionReason::EMERGENCY) {
        return true;
    }
    
    // Check if transition is valid based on state machine logic
    return isValidStateTransition(from, to);
}

bool StateMachine::isValidStateTransition(SystemState from, SystemState to) const {
    // Define valid state transitions
    switch (from) {
        case SystemState::INITIALIZING:
            return to == SystemState::CONNECTING_WIFI || to == SystemState::ERROR;
            
        case SystemState::CONNECTING_WIFI:
            return to == SystemState::CONNECTING_SERVER || to == SystemState::ERROR || 
                   to == SystemState::CONNECTING_WIFI;  // Allow self-transition for retry
                   
        case SystemState::CONNECTING_SERVER:
            return to == SystemState::CONNECTED || to == SystemState::ERROR || 
                   to == SystemState::CONNECTING_WIFI || to == SystemState::CONNECTING_SERVER;
                   
        case SystemState::CONNECTED:
            return to == SystemState::DISCONNECTED || to == SystemState::ERROR || 
                   to == SystemState::CONNECTING_WIFI || to == SystemState::CONNECTING_SERVER;
                   
        case SystemState::DISCONNECTED:
            return to == SystemState::CONNECTING_SERVER || to == SystemState::ERROR || 
                   to == SystemState::CONNECTING_WIFI;
                   
        case SystemState::ERROR:
            return to == SystemState::CONNECTING_WIFI || to == SystemState::MAINTENANCE ||
                   to == SystemState::ERROR;  // Allow self-transition for retry
                   
        case SystemState::MAINTENANCE:
            return to == SystemState::INITIALIZING || to == SystemState::CONNECTING_WIFI;
            
        default:
            return false;
    }
}

bool StateMachine::checkEntryConditions(SystemState state) {
    auto config_it = state_configs.find(state);
    if (config_it == state_configs.end()) {
        return true;  // No configuration means no restrictions
    }
    
    const auto& config = config_it->second;
    
    // Check each entry condition
    for (const auto& condition : config.entry_conditions) {
        if (condition.condition) {
            bool result = condition.condition();
            if (!result && condition.timeout_ms > 0) {
                // Wait for condition with timeout
                unsigned long start_time = millis();
                while (!result && (millis() - start_time) < condition.timeout_ms) {
                    delay(100);
                    result = condition.condition();
                }
            }
            
            if (!result) {
                auto logger = SystemManager::getInstance().getLogger();
                if (logger) {
                    logger->log(LogLevel::LOG_WARN, "StateMachine", __FILE__, __LINE__, "Entry condition failed for state %s: %s",
                               getStateName(state).c_str(), condition.description);
                }
                return false;
            }
        }
    }
    
    return true;
}

bool StateMachine::checkExitConditions(SystemState state) {
    if (state == SystemState::INITIALIZING) {
        state = current_state;  // Use current state if not specified
    }
    
    auto config_it = state_configs.find(state);
    if (config_it == state_configs.end()) {
        return true;  // No configuration means no restrictions
    }
    
    const auto& config = config_it->second;
    
    // Check each exit condition
    for (const auto& condition : config.exit_conditions) {
        if (condition.condition) {
            bool result = condition.condition();
            if (!result && condition.timeout_ms > 0) {
                // Wait for condition with timeout
                unsigned long start_time = millis();
                while (!result && (millis() - start_time) < condition.timeout_ms) {
                    delay(100);
                    result = condition.condition();
                }
            }
            
            if (!result) {
                auto logger = SystemManager::getInstance().getLogger();
                if (logger) {
                    logger->log(LogLevel::LOG_WARN, "StateMachine", __FILE__, __LINE__, "Exit condition failed for state %s: %s",
                               getStateName(state).c_str(), condition.description);
                }
                return false;
            }
        }
    }
    
    return true;
}

bool StateMachine::hasStateTimedOut(SystemState state) const {
    if (state == SystemState::INITIALIZING) {
        state = current_state;  // Use current state if not specified
    }
    
    auto config_it = state_configs.find(state);
    if (config_it == state_configs.end() || config_it->second.max_duration_ms == 0) {
        return false;  // No timeout configured
    }
    
    return (millis() - state_entry_time) >= config_it->second.max_duration_ms;
}

bool StateMachine::isStateValid(SystemState state) const {
    return state >= SystemState::INITIALIZING && state <= SystemState::MAINTENANCE;
}

void StateMachine::recordTransition(const StateTransition& transition) {
    if (!enable_history) {
        return;
    }
    
    transition_history.push_back(transition);
    cleanupHistory();
}

void StateMachine::cleanupHistory() {
    while (transition_history.size() > MAX_HISTORY_SIZE) {
        transition_history.erase(transition_history.begin());
    }
}

void StateMachine::updateStatistics(const StateTransition& transition) {
    if (!enable_statistics) {
        return;
    }
    
    stats.total_transitions++;
    
    if (transition.successful) {
        stats.successful_transitions++;
    } else {
        stats.failed_transitions++;
    }
    
    // Update state entry count
    stats.state_entry_counts[transition.to_state]++;
    
    // Update state duration
    unsigned long duration = transition.transition_time - state_entry_time;
    stats.state_durations_ms[transition.from_state] += duration;
    
    // Update timing
    stats.last_transition_time = transition.transition_time;
    stats.current_state = transition.to_state;
    stats.previous_state = transition.from_state;
    
    // Count by reason
    switch (transition.reason) {
        case StateTransitionReason::TIMEOUT:
            stats.timeout_transitions++;
            break;
        case StateTransitionReason::ERROR_CONDITION:
            stats.error_transitions++;
            break;
        default:
            break;
    }
}

String StateMachine::getStateName(SystemState state) const {
    switch (state) {
        case SystemState::INITIALIZING: return "INITIALIZING";
        case SystemState::CONNECTING_WIFI: return "CONNECTING_WIFI";
        case SystemState::CONNECTING_SERVER: return "CONNECTING_SERVER";
        case SystemState::CONNECTED: return "CONNECTED";
        case SystemState::DISCONNECTED: return "DISCONNECTED";
        case SystemState::ERROR: return "ERROR";
        case SystemState::MAINTENANCE: return "MAINTENANCE";
        default: return "UNKNOWN_STATE";
    }
}

const char* StateMachine::getStateDescription(SystemState state) const {
    switch (state) {
        case SystemState::INITIALIZING: return "System initialization in progress";
        case SystemState::CONNECTING_WIFI: return "Attempting to connect to WiFi network";
        case SystemState::CONNECTING_SERVER: return "Attempting to connect to audio server";
        case SystemState::CONNECTED: return "Connected and streaming audio data";
        case SystemState::DISCONNECTED: return "Disconnected from server, ready to reconnect";
        case SystemState::ERROR: return "System error detected, recovery in progress";
        case SystemState::MAINTENANCE: return "System in maintenance mode";
        default: return "Unknown state";
    }
}

SystemState StateMachine::getStateFromName(const char* name) const {
    if (strcmp(name, "INITIALIZING") == 0) return SystemState::INITIALIZING;
    if (strcmp(name, "CONNECTING_WIFI") == 0) return SystemState::CONNECTING_WIFI;
    if (strcmp(name, "CONNECTING_SERVER") == 0) return SystemState::CONNECTING_SERVER;
    if (strcmp(name, "CONNECTED") == 0) return SystemState::CONNECTED;
    if (strcmp(name, "DISCONNECTED") == 0) return SystemState::DISCONNECTED;
    if (strcmp(name, "ERROR") == 0) return SystemState::ERROR;
    if (strcmp(name, "MAINTENANCE") == 0) return SystemState::MAINTENANCE;
    return SystemState::INITIALIZING;  // Default
}

void StateMachine::onStateChange(std::function<void(SystemState, SystemState, StateTransitionReason)> callback) {
    state_change_callback = callback;
}

void StateMachine::onStateEntry(std::function<void(SystemState, const char*)> callback) {
    state_entry_callback = callback;
}

void StateMachine::onStateExit(std::function<void(SystemState, const char*)> callback) {
    state_exit_callback = callback;
}

void StateMachine::removeCallbacks() {
    state_change_callback = nullptr;
    state_entry_callback = nullptr;
    state_exit_callback = nullptr;
}

void StateMachine::clearHistory() {
    transition_history.clear();
}

StateTransition StateMachine::getLastTransition() const {
    return transition_history.empty() ? StateTransition(current_state, current_state, StateTransitionReason::NORMAL) : transition_history.back();
}

void StateMachine::resetStatistics() {
    stats = StateMachineStats();
    stats.current_state = current_state;
    stats.previous_state = previous_state;
    stats.current_state_start_time = state_entry_time;
    stats.last_transition_time = last_transition_time;
}

uint32_t StateMachine::getStateEntryCount(SystemState state) const {
    auto it = stats.state_entry_counts.find(state);
    return (it != stats.state_entry_counts.end()) ? it->second : 0;
}

unsigned long StateMachine::getTotalTimeInState(SystemState state) const {
    auto it = stats.state_durations_ms.find(state);
    return (it != stats.state_durations_ms.end()) ? it->second : 0;
}

float StateMachine::getStateSuccessRate(SystemState state) const {
    uint32_t entries = getStateEntryCount(state);
    if (entries == 0) return 0.0f;
    
    // Count successful transitions to this state
    uint32_t successful_entries = 0;
    for (const auto& transition : transition_history) {
        if (transition.to_state == state && transition.successful) {
            successful_entries++;
        }
    }
    
    return (successful_entries * 100.0f) / entries;
}

float StateMachine::getOverallSuccessRate() const {
    if (stats.total_transitions == 0) return 0.0f;
    return (stats.successful_transitions * 100.0f) / stats.total_transitions;
}

void StateMachine::printCurrentState() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "=== Current State ===");
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "State: %s", getCurrentStateName().c_str());
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Description: %s", getStateDescription(current_state));
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Duration: %lu ms", getStateDuration());
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Previous: %s", getPreviousStateName().c_str());
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "====================");
}

void StateMachine::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "=== State Machine Statistics ===");
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Total transitions: %u", stats.total_transitions);
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Successful: %u (%.1f%%)", 
               stats.successful_transitions, getOverallSuccessRate());
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Failed: %u", stats.failed_transitions);
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Timeout transitions: %u", stats.timeout_transitions);
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Error transitions: %u", stats.error_transitions);
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Time in current state: %lu ms", getStateDuration());
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Time since last transition: %lu ms", getTimeSinceLastTransition());
    
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "--- State Entry Counts ---");
    for (const auto& pair : stats.state_entry_counts) {
        logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "%s: %u entries", 
                   getStateName(pair.first).c_str(), pair.second);
    }
    
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "=============================");
}

void StateMachine::printHistory() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "=== State Transition History ===");
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "Showing last %u transitions:", transition_history.size());
    
    for (size_t i = 0; i < transition_history.size(); i++) {
        const auto& transition = transition_history[i];
        logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "%u: %s → %s (reason: %u, success: %s, time: %lu)",
                   i, getStateName(transition.from_state).c_str(),
                   getStateName(transition.to_state).c_str(),
                   static_cast<unsigned int>(transition.reason),
                   transition.successful ? "yes" : "no",
                   transition.transition_time);
    }
    
    logger->log(LogLevel::LOG_INFO, "StateMachine", __FILE__, __LINE__, "===============================");
}

bool StateMachine::validateStateMachine() const {
    // Basic validation - can be extended
    return !state_configs.empty() && initialized;
}

std::vector<String> StateMachine::getValidationErrors() const {
    std::vector<String> errors;
    
    if (!initialized) {
        errors.push_back("StateMachine not initialized");
    }
    
    if (state_configs.empty()) {
        errors.push_back("No state configurations defined");
    }
    
    return errors;
}