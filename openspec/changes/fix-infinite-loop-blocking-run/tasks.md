# Tasks: Fix Infinite Loop in SystemManager::run()

## Phase 1: Code Refactoring (Remove Blocking Loop)

### Task 1.1: Refactor SystemManager::run() to Non-Blocking
**Dependency**: None
**Effort**: 2 hours
**Validation**: Unit test + manual observation of serial responsiveness

- [ ] Remove `while (system_running)` blocking loop from `SystemManager::run()`
- [ ] Modify to execute one complete cycle per call
- [ ] Preserve all state machine logic (no behavioral changes)
- [ ] Add cycle timing calculation at end
- [ ] Keep watchdog reset at beginning
- [ ] **Verification**: Run `run()` in loop, verify returns within 100ms each time

**File**: `src/core/SystemManager.cpp` line 203-324

**Pseudo-code**:
```cpp
// Before: while (system_running) { ... delay(...); }
// After: Single iteration, return

void SystemManager::run() {
    cycle_start_time = millis();

    // ... existing state machine logic (unchanged) ...

    // Maintain timing
    unsigned long cycle_time = millis() - cycle_start_time;
    if (cycle_time < CYCLE_TIME_MS) {
        delay(CYCLE_TIME_MS - cycle_time);
    }
    // Return to Arduino loop
}
```

---

### Task 1.2: Remove Blocking Delays in handleWiFiConnection()
**Dependency**: Task 1.1
**Effort**: 1.5 hours
**Validation**: Verify WiFi connection succeeds over multiple iterations

- [ ] Review `NetworkManager::handleWiFiConnection()`
- [ ] Remove any blocking `delay()` or retry loops in WiFi handling
- [ ] Ensure WiFi connection check is non-blocking
- [ ] WiFi scan should complete over multiple iterations if needed
- [ ] **Verification**: Serial commands execute during WiFi scan

**File**: `src/network/NetworkManager.cpp`

---

### Task 1.3: Update Arduino loop() to Handle New run() Behavior
**Dependency**: Task 1.1
**Effort**: 0.5 hours
**Validation**: Visual inspection of loop() structure

- [ ] Verify `src/main.cpp::loop()` is already non-blocking
- [ ] Confirm serial command handling occurs after `systemManager.run()`
- [ ] Add minimal delay/yield if needed to prevent busy-wait
- [ ] **Verification**: CPU load drops from 100% to 10-20%

**File**: `src/main.cpp` line 84-111

---

## Phase 2: State Duration Tracking (Timeout Detection)

### Task 2.1: Add State Duration Tracking to StateMachine
**Dependency**: Task 1.1
**Effort**: 1.5 hours
**Validation**: Unit test for state duration calculation

- [ ] Add `entry_time` member to track when state was entered
- [ ] Add `getStateDuration()` method to calculate duration in current state
- [ ] Update `setState()` to record entry time on transition
- [ ] Handle `millis()` wraparound (49+ day overflow)
- [ ] **Verification**: Unit test verifies duration increases correctly

**File**: `src/core/StateMachine.h` and `StateMachine.cpp`

**Changes**:
```cpp
private:
    unsigned long state_entry_time;  // When current state was entered

public:
    uint32_t getStateDuration() const;  // Returns ms in current state
```

---

### Task 2.2: Define State Timeout Thresholds
**Dependency**: Task 2.1
**Effort**: 0.5 hours
**Validation**: Code review of timeout values

- [ ] Define timeout constants for each state (30s for WiFi, 10s for server, etc.)
- [ ] Store in `StateMachine` or separate configuration
- [ ] Document rationale for each timeout value
- [ ] **Verification**: Values match specification

**File**: `src/core/StateMachine.h` or `src/config.h`

```cpp
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;      // 30 seconds
const uint32_t SERVER_CONNECT_TIMEOUT_MS = 10000;    // 10 seconds
const uint32_t INITIALIZING_TIMEOUT_MS = 10000;      // 10 seconds
```

---

### Task 2.3: Implement Timeout Detection in SystemManager::run()
**Dependency**: Tasks 1.1, 2.1, 2.2
**Effort**: 1.5 hours
**Validation**: Manual test - let WiFi fail, observe timeout triggers

- [ ] In `SystemManager::run()`, check state duration against timeout
- [ ] On timeout, log diagnostic information (memory, CPU, errors)
- [ ] Transition to ERROR state automatically
- [ ] Trigger recovery process
- [ ] **Verification**: Manual test - WiFi timeout triggers after 30s

**File**: `src/core/SystemManager.cpp` line 238-311

**Pseudo-code**:
```cpp
if (state_machine->getStateDuration() > getStateTimeout(currentState)) {
    logger->warn("SystemManager", "State timeout detected");
    recordDiagnostics();  // Log for debugging
    state_machine->setState(SystemState::ERROR);
}
```

---

### Task 2.4: Add Diagnostic Logging on State Timeout
**Dependency**: Task 2.3
**Effort**: 1 hour
**Validation**: Review log output contains required information

- [ ] Log state name, duration, timeout threshold
- [ ] Log current memory statistics (free heap, fragmentation)
- [ ] Log CPU load percentage
- [ ] Log recent errors encountered
- [ ] Log recovery attempt count
- [ ] **Verification**: Manual test produces detailed timeout logs

**File**: `src/core/SystemManager.cpp`

---

## Phase 3: Async Recovery Implementation

### Task 3.1: Add Recovery Iteration State to HealthMonitor
**Dependency**: None (parallel with Phase 2)
**Effort**: 1 hour
**Validation**: Recovery state tracked correctly

- [ ] Add recovery phase tracking (IDLE, CLEANUP, DEFRAG, RETRY, FAILED)
- [ ] Add recovery attempt counter
- [ ] Add last attempt time tracking
- [ ] Add exponential backoff delay calculation
- [ ] **Verification**: Recovery state transitions as expected

**File**: `src/monitoring/HealthMonitor.h` and `HealthMonitor.cpp`

---

### Task 3.2: Refactor Recovery to Step-Based Execution
**Dependency**: Task 3.1
**Effort**: 2 hours
**Validation**: Recovery executes over multiple iterations

- [ ] Modify `attemptRecovery()` to execute one step per call
- [ ] Step 1: Emergency memory cleanup
- [ ] Step 2: Memory defragmentation (if needed)
- [ ] Step 3: Verify recovery success
- [ ] Implement exponential backoff between retry steps
- [ ] **Verification**: Each `attemptRecovery()` call completes in <50ms

**File**: `src/monitoring/HealthMonitor.cpp`

**Current (blocking)**:
```cpp
void attemptRecovery() {
    while (needs_recovery) {
        emergencyCleanup();
        delay(100);
        defragmentMemory();
        delay(100);
        // ... etc
    }
}
```

**New (step-based)**:
```cpp
void attemptRecovery() {
    switch (recovery_phase) {
        case RECOVERY_CLEANUP:
            emergencyCleanup();
            recovery_phase = RECOVERY_DEFRAG;
            break;
        case RECOVERY_DEFRAG:
            defragmentMemory();
            recovery_phase = RECOVERY_RETRY;
            break;
        // ...
    }
}
```

---

### Task 3.3: Distribute Health Checks Across Iterations
**Dependency**: None (parallel with Phase 2)
**Effort**: 1 hour
**Validation**: Each check executes once every 5 seconds

- [ ] Modify `performHealthChecks()` to track check index
- [ ] Execute one check per iteration, cycle through all checks
- [ ] Calculate overall health every 5 seconds (50 iterations at 100 Hz)
- [ ] No blocking delays in health checks
- [ ] **Verification**: Each iteration takes <50ms, no blocking

**File**: `src/monitoring/HealthMonitor.cpp`

---

### Task 3.4: Implement Recovery Attempt Limits
**Dependency**: Task 3.2
**Effort**: 1 hour
**Validation**: Recovery stops after max attempts

- [ ] Define maximum recovery attempts (e.g., 3)
- [ ] Track recovery attempt counter
- [ ] After max attempts exceeded, transition to ERROR state
- [ ] Log failure with diagnostic information
- [ ] Require manual intervention (RECONNECT command) to retry
- [ ] **Verification**: Recovery stops after 3 failed attempts

**File**: `src/monitoring/HealthMonitor.cpp`

---

## Phase 4: Testing and Validation

### Task 4.1: Unit Tests for Non-Blocking Loop
**Dependency**: Task 1.1
**Effort**: 1.5 hours
**Validation**: All unit tests pass

- [ ] Test `SystemManager::run()` returns within 100ms
- [ ] Test state machine transitions occur correctly
- [ ] Test watchdog reset happens every iteration
- [ ] Test loop frequency averages 80-120 Hz
- [ ] Test no blocking delays in any component

**File**: `tests/unit/test_system_manager_non_blocking.cpp`

---

### Task 4.2: Integration Test - WiFi Failure Scenario
**Dependency**: Tasks 1.1, 2.3, 3.2
**Effort**: 2 hours
**Validation**: Test passes on hardware

- [ ] Setup: WiFi configured to non-existent network
- [ ] Expected: System enters CONNECTING_WIFI, timeout at 30s
- [ ] Verify: Serial commands execute during WiFi retry
- [ ] Verify: Health monitor attempts recovery asynchronously
- [ ] Verify: No infinite loop or system freeze
- [ ] Verify: CPU load stays <50% during retry

**File**: `tests/integration/test_wifi_failure_recovery.cpp`

---

### Task 4.3: Integration Test - Serial Command Responsiveness
**Dependency**: Task 1.1
**Effort**: 1.5 hours
**Validation**: Commands respond immediately

- [ ] Setup: System attempting WiFi connection
- [ ] Action: Send RECONNECT command via serial
- [ ] Expected: Command processes within 100ms
- [ ] Expected: State transitions immediately
- [ ] Verify: No blocking delays prevent serial processing

**File**: `tests/integration/test_serial_responsiveness.cpp`

---

### Task 4.4: Integration Test - Recovery Process
**Dependency**: Tasks 3.2, 3.4
**Effort**: 2 hours
**Validation**: Recovery executes asynchronously

- [ ] Setup: Simulate memory pressure (WiFi task fails)
- [ ] Verify: Recovery cleanup executes over multiple iterations
- [ ] Verify: Exponential backoff applied correctly
- [ ] Verify: Max attempts enforced
- [ ] Verify: System remains responsive during recovery

**File**: `tests/integration/test_recovery_async.cpp`

---

### Task 4.5: Load Test - CPU and Memory During Failure
**Dependency**: All Phase 1-3 tasks
**Effort**: 1.5 hours
**Validation**: Performance metrics meet requirements

- [ ] Setup: System in continuous WiFi failure + recovery
- [ ] Measure: CPU load (target: <50%)
- [ ] Measure: Memory usage (target: no increase)
- [ ] Measure: Loop frequency (target: 80-120 Hz)
- [ ] Measure: Serial command latency (target: <100ms)
- [ ] Duration: Run for 5 minutes
- [ ] Verify: No memory leaks or degradation

**File**: `tests/performance/test_failure_recovery_load.cpp`

---

### Task 4.6: Manual Testing - WiFi Failure Recovery
**Dependency**: All Phase 1-3 tasks
**Effort**: 1 hour
**Validation**: System behaves as expected in real hardware

- [ ] Device: ESP32-DevKit
- [ ] Test: Power on with WiFi disabled
- [ ] Observe: Serial output shows state transitions
- [ ] Test: Issue RECONNECT command
- [ ] Observe: System responds immediately
- [ ] Test: Re-enable WiFi
- [ ] Observe: System connects successfully
- [ ] Log: Verify timeout and recovery messages appear

---

## Phase 5: Documentation and Deployment

### Task 5.1: Update Code Documentation
**Dependency**: All Phase 1-4 tasks
**Effort**: 1 hour
**Validation**: Documentation reviewed and approved

- [ ] Document `SystemManager::run()` non-blocking behavior
- [ ] Document state timeouts and recovery process
- [ ] Add inline comments explaining iteration logic
- [ ] Update class documentation for HealthMonitor recovery phases
- [ ] **File**: Doxygen comments in `.h` files

---

### Task 5.2: Update Architecture Documentation
**Dependency**: All Phase 1-4 tasks
**Effort**: 1 hour
**Validation**: Architecture docs are clear and accurate

- [ ] Update `ARCHITECTURE.md` to explain non-blocking loop
- [ ] Add diagram showing iteration flow
- [ ] Document state timeout thresholds
- [ ] Explain recovery process and failure scenarios
- [ ] **File**: `docs/ARCHITECTURE.md`

---

### Task 5.3: Merge to Main Branch
**Dependency**: All tests passing
**Effort**: 0.5 hours
**Validation**: PR approved and merged

- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Code review completed
- [ ] Create git commit with detailed message
- [ ] Merge to `main` branch
- [ ] Tag release version

---

## Parallelization Opportunities

**Can run in parallel**:
- Phase 2 (State Timing) and Phase 3 (Async Recovery) are independent
- Tasks within each phase can often run in parallel after dependencies are met
- Testing (Phase 4) can begin as soon as refactoring (Phase 1) is complete

**Recommended parallelization**:
```
Phase 1 (sequential):    1.1 → 1.2 → 1.3 (4 hours)

Phase 2 (parallel):      2.1 || 3.1 (parallel non-dependent work)
                         2.2 → 2.3 → 2.4 (depends on 2.1)
                         3.2 → 3.3 → 3.4 (depends on 3.1)

Phase 4 (parallel):      4.1 || 4.2 || 4.3 || 4.4 || 4.5 (some dependencies)
                         4.6 (after others complete)

Total: ~16 hours → ~8-10 hours with parallelization
```

---

## Success Criteria

✅ **All tasks complete when**:
1. All unit tests pass
2. All integration tests pass
3. Manual hardware test succeeds
4. Serial commands respond immediately during WiFi failure
5. System recovers from WiFi error without freezing
6. CPU load <50% during recovery
7. No infinite loops or blocking delays detected
8. Documentation updated
9. Code merged to main branch

---

## Rollback Plan

If critical issues found during testing:
1. Revert Phase 1 changes (return to blocking loop)
2. System returns to previous state
3. Issue investigation and fix
4. Re-implement with corrections

**No data loss**: Audio streaming and network operations designed to resume gracefully.
