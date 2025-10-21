# Specification: Remove Blocking while(system_running) Loop

## ADDED Requirements

### Requirement: Non-Blocking Iteration
The `SystemManager::run()` method SHALL execute exactly one complete system cycle per invocation and MUST return control to the Arduino `loop()` function within 100ms.

#### Scenario: Single Iteration Completes
When `systemManager.run()` is called from Arduino `loop()`, it SHALL return within 100ms to allow other operations.

```cpp
void loop() {
    systemManager.run();  // One cycle only
    // Loop returns here after 10-50ms
    handleSerialCommands();  // Can now execute
}
```

#### Scenario: No Blocking Delays During Recovery
When WiFi initialization fails with error 0x3001, the system SHALL continue executing iterations without blocking in retry loops.

```
[Expected] Iteration N: WiFi task creation fails (0x3001)
[Expected] Iteration N+1: Serial command processes RECONNECT
[Expected] Iteration N+2: Health monitor executes recovery
[NOT Expected] Blocked in retry loop for 30 seconds
```

---

### Requirement: Preserve State Machine Transitions
The state machine transitions (INITIALIZING → CONNECTING_WIFI → CONNECTING_SERVER → CONNECTED) SHALL function identically to the blocking implementation, achieving same transitions with same logic.

#### Scenario: WiFi Connection Succeeds
Multiple iterations in CONNECTING_WIFI state successfully transition to CONNECTING_SERVER.

```
Multiple iterations in CONNECTING_WIFI state:
  Iteration 0-2: Scan networks, attempt connection
  Iteration 3: Check connection status → success
  Iteration 4: Transition to CONNECTING_SERVER
```

#### Scenario: State Timeout Detection
System SHALL detect states that exceed reasonable duration (e.g., >30 seconds in CONNECTING_WIFI) and transition to ERROR state for recovery.

---

### Requirement: Maintain Watchdog Reset Frequency
The watchdog timer MUST be reset regularly (every 10ms or less) to prevent timeout during normal operation.

#### Scenario: Watchdog Reset During WiFi Retry
Even if WiFi continuously fails, the watchdog timer continues to be reset preventing timeout.

```
Even if WiFi continuously fails:
  Iteration 0: Reset watchdog, attempt connection
  Iteration 1: Reset watchdog, check status
  Iteration 2: Reset watchdog, log retry
  ...continues indefinitely without 60-second timeout
```

---

### Requirement: Enable Serial Command Processing
Serial commands (RECONNECT, STATUS, REBOOT, etc.) MUST be processable even when WiFi is failing, without blocking for extended periods.

#### Scenario: RECONNECT Command During WiFi Retry
User can issue commands while WiFi is failing and the system responds immediately.

```
User types: RECONNECT
  Within 100ms: handleSerialCommands() processes input
  Within 200ms: State machine transitions to CONNECTING_WIFI
  Result: System responds immediately, not blocked
```

---

### Requirement: Timing Accuracy Within Tolerance
System main loop frequency SHALL maintain approximately 100 Hz (±20%) average frequency over 10-second windows.

#### Scenario: Loop Frequency Measurement
System maintains consistent loop frequency even during error conditions.

```
Target: 100 Hz (10ms per iteration)
Acceptable: 80-120 Hz average
Over 10 seconds: ~1000-1200 iterations
```

---

## Design Constraints

1. **No Breaking Changes**: Public API of `SystemManager::run()` must remain compatible
2. **Same Error Logic**: Error handling and recovery must work identically
3. **No Busy-Wait**: Must not consume 100% CPU during normal operation
4. **Memory Minimal**: State tracking overhead <16 bytes

## Implementation Notes

### Loop Structure Change
```cpp
// BEFORE (blocking)
void SystemManager::run() {
    while (system_running) {
        // ... state machine ...
        delay(CYCLE_TIME_MS);
    }
}

// AFTER (non-blocking)
void SystemManager::run() {
    // ... state machine logic (same) ...
    unsigned long cycle_time = millis() - cycle_start_time;
    if (cycle_time < CYCLE_TIME_MS) {
        delay(CYCLE_TIME_MS - cycle_time);
    }
}
```

## Validation Checklist

- [ ] `run()` returns within 100ms
- [ ] Serial commands execute during WiFi retry
- [ ] Watchdog reset occurs every iteration
- [ ] Loop frequency averages 80-120 Hz
- [ ] All state transitions work as before
- [ ] No infinite loops or blocking calls in `run()`
