# Fix Infinite Loop: Main Specification

## ADDED Requirements

#### Requirement: Non-Blocking SystemManager::run()

The `SystemManager::run()` method SHALL execute exactly one complete system cycle per invocation and MUST return control to the Arduino `loop()` function within 100ms.

#### Scenario: Single Iteration Completes

```
void loop() {
    systemManager.run();  // One cycle only
    handleSerialCommands();  // Executes within 100ms
}
```

---

#### Requirement: State Duration Tracking with Timeout

System SHALL track time in each state and automatically transition to ERROR state when duration exceeds thresholds (30s for WiFi, 10s for server).

#### Scenario: WiFi Timeout Detection

```text
t=30s: In CONNECTING_WIFI state
       Duration reaches 30s threshold
       Transition to ERROR state automatically
```

---

#### Requirement: Async Recovery Operations

Recovery operations SHALL execute incrementally over multiple iterations rather than blocking, with exponential backoff between attempts.

#### Scenario: Memory Recovery Steps

```text
Iteration 0: Emergency cleanup
Iteration 100: Memory defragmentation
Iteration 200: Verify recovery success
(each ~1 second apart at 100Hz iteration rate, non-blocking)
```

---

#### Requirement: Serial Command Responsiveness

Serial commands MUST be processable within <100ms even when WiFi is failing.

#### Scenario: RECONNECT During Failure

```text
System in CONNECTING_WIFI state trying to connect
User types: RECONNECT
Within 100ms: Command processes and state transitions
```

---

#### Requirement: Watchdog Reset Maintenance

Watchdog timer MUST be reset every iteration to prevent timeout even during continuous WiFi failures.

#### Scenario: Watchdog Reset During Retry

```
Every iteration in CONNECTING_WIFI:
  ├─ Reset watchdog
  ├─ Attempt WiFi connection
  └─ Repeat (no 60s timeout)
```
