# Error Loop Analysis: WiFi Failure Infinite Retry

## Symptom
System enters an infinite retry loop when WiFi initialization fails with error `0x3001` (task creation failed). Device becomes unresponsive to serial commands for extended periods.

## Root Cause Analysis

### 1. **Blocking Infinite Loop in SystemManager::run()**

**Location**: `src/core/SystemManager.cpp` line 217

```cpp
void SystemManager::run() {
    if (!system_running) return;

    while (system_running) {  // ← INFINITE BLOCKING LOOP
        // ... state machine operations ...
        // ... health checks ...
        // ... audio streaming ...
        delay(CYCLE_TIME_MS);
    }
    // This line never executes!
}
```

**Problem**: This loop never returns to the Arduino `loop()` function. The entire program is stuck in `SystemManager::run()`, unable to:
- Process serial commands
- Execute recovery operations
- Handle other events
- Return to main loop

### 2. **WiFi Task Creation Fails**

When the system transitions to `CONNECTING_WIFI` state:

```
[001224][INFO][NetworkManager] Initializing NetworkManager
E (1243) wifi_init: Failed to deinit Wi-Fi driver (0x3001)
E (1243) wifi_init: Failed to deinit Wi-Fi (0x3001)
```

Error `0x3001` = WiFi task creation failed (insufficient resources/memory)

### 3. **Infinite Retry Loop**

Because `run()` is a blocking loop, when WiFi fails:

```
Loop Iteration 0:
  ├─ State: CONNECTING_WIFI
  ├─ network_manager.handleWiFiConnection()
  │  └─ WiFi initialization fails (0x3001)
  └─ delay(10ms) → Go to iteration 1

Loop Iteration 1:
  ├─ State: CONNECTING_WIFI (unchanged)
  ├─ health_monitor.performHealthChecks()
  │  └─ Detect WiFi failure
  │  └─ Attempt recovery (BLOCKING)
  └─ delay(10ms) → Go to iteration 2

Loop Iteration 2-N:
  ├─ State: CONNECTING_WIFI (stuck)
  ├─ Retry WiFi initialization repeatedly
  ├─ Attempt recovery repeatedly (blocks recovery)
  └─ delay(10ms) → Go to next iteration

This repeats indefinitely, never returning to Arduino loop()
```

### 4. **Health Monitor Auto-Recovery Blocked**

Ironically, the auto-recovery mechanism is **blocked by the infinite loop**:

```
[015036][INFO][HealthMonitor] Attempting auto-recovery
[015036][CRITICAL][MemoryManager] Emergency cleanup initiated (#1)
[015037][WARN][MemoryManager] Entering emergency memory mode
...
[015069][INFO][MemoryManager] Exiting emergency memory mode
[015069][INFO][HealthMonitor] Auto-recovery completed
[WARN] CPU overload detected!
E (15076) wifi:create wifi task: failed to create task  ← Still fails!
```

The recovery runs **inside the blocking loop**, so it cannot give WiFi time to stabilize before retrying.

### 5. **CPU Overload Detected**

Because the loop is spinning rapidly without progress:

```
[WARN] CPU overload detected!
[WARN] CPU overload detected!
[WARN] CPU overload detected!  ← Repeated every iteration
```

The health monitor detects 100% CPU load (loop spinning without making progress).

### 6. **Serial Commands Unresponsive**

In `src/main.cpp`:

```cpp
void loop() {
    systemManager.run();  // ← BLOCKS HERE - never returns

    handleSerialCommands();  // Never executes!
}
```

When `run()` is stuck in blocking loop, `handleSerialCommands()` never executes. User typing commands gets no response.

## Timeline of Events

```
t=0s:    System boots
         Initialization completes successfully

t=1s:    State transition → CONNECTING_WIFI
         WiFi task creation fails (0x3001)

t=1-5s:  Stuck in SystemManager::run() blocking loop
         WiFi repeatedly fails
         Health monitor attempts recovery (blocked by loop)

t=5s:    AutoRecovery completes (but loop still running)
         WiFi retried but still fails

t=10s:   Health monitor detects CPU overload
         [WARN] CPU overload detected!

t=15s:   Still in CONNECTING_WIFI state, still retrying
         [WARN] CPU overload detected!
         User tries to type RECONNECT command
         → No response (handleSerialCommands never executes)

t=25-35s: Continuous retry loop
         WiFi task creation keeps failing
         Recovery keeps attempting, keeps being blocked
         CPU still at 100%

t=60s:   Watchdog timeout
         System would reboot (but current implementation prevents this)
```

## Why This Design Fails

### ❌ Blocking Loop Incompatible with Arduino Framework

Arduino expects:
```cpp
void loop() {
    // Do work for ~10ms
    // Return so system can handle other tasks
    // Called repeatedly
}
```

Current code does:
```cpp
void loop() {
    systemManager.run();  // Never returns!
}
```

### ❌ Recovery Blocked by Loop It Tries to Recover From

The health monitor runs inside the blocking loop, so:
- It can't give WiFi time to stabilize (loop immediately retries)
- It can't escape the state to try different recovery approach
- It can't allow serial commands to intervene

### ❌ No Escape Path

Once WiFi fails and loop blocks:
- No serial commands → can't issue RECONNECT
- No recovery escape → stuck in retry
- No timeout → stuck indefinitely (except watchdog)
- Only solution: power cycle

## Solution

**Remove the blocking loop** and make `run()` execute one iteration per call:

```cpp
void SystemManager::run() {
    // Execute ONE cycle only
    // ... state machine logic ...
    // ... health checks ...
    // ... audio streaming ...

    // Calculate timing
    unsigned long cycle_time = millis() - cycle_start_time;
    if (cycle_time < CYCLE_TIME_MS) {
        delay(CYCLE_TIME_MS - cycle_time);
    }

    // Return to Arduino loop after 10-50ms
}
```

With this change:

```
t=0s:    System boots

t=1s:    State transition → CONNECTING_WIFI

t=1-5s:  Iterations 0-500 in CONNECTING_WIFI
         Each iteration:
         ├─ Attempt WiFi connection (one try)
         ├─ Return to Arduino loop
         ├─ System can execute serial commands
         └─ Repeat

t=3s:    User types: RECONNECT
         ├─ Serial command executes within 100ms
         ├─ State transitions to CONNECTING_WIFI
         └─ Retry with fresh start

t=30s:   State timeout detected
         ├─ Still in CONNECTING_WIFI
         ├─ Duration: 30 seconds
         └─ Transition to ERROR state

t=30-35s: Recovery executes asynchronously
         ├─ Step 1: Memory cleanup
         ├─ Step 2: Defragmentation
         ├─ Step 3: Verify recovery
         └─ Each step in separate iteration

t=35s:   Retry WiFi after recovery
         ├─ WiFi task creation succeeds (memory freed)
         └─ Transition to CONNECTING_SERVER
```

## Detailed Fix Components

### 1. Remove Blocking Loop
- Delete `while (system_running)` loop structure
- Execute state machine logic once per call
- Return to Arduino loop after each iteration

### 2. Add State Duration Tracking
- Track time in CONNECTING_WIFI (timeout: 30s)
- Track time in CONNECTING_SERVER (timeout: 10s)
- Transition to ERROR on timeout

### 3. Async Recovery
- Execute recovery steps asynchronously (one per iteration)
- Use exponential backoff between retry attempts
- Limit recovery attempts (max 3) to prevent infinite loops

### 4. Serial Command Processing
- Now executes during WiFi retry (responsive system)
- User can issue RECONNECT, STATUS, REBOOT, etc.
- System responds within <100ms

## Expected Behavior After Fix

**When WiFi fails**:
```
[001354][INFO][StateMachine] State transition: INITIALIZING → CONNECTING_WIFI
[WARN] WiFi connection failed (error 0x3001)

[User types: STATUS]
[INFO] System Status:
  State: CONNECTING_WIFI (duration: 2s)
  Memory: 67 KB free
  CPU Load: 15%
  [Responsive immediately!]

[After 30 seconds]
[001374][WARN][StateMachine] State timeout detected!
  Duration: 30000ms (timeout: 30000ms)
  Transitioning to ERROR state

[001375][INFO][HealthMonitor] Attempting auto-recovery (step 1)
[001385][INFO][HealthMonitor] Attempting auto-recovery (step 2)
[001395][INFO][HealthMonitor] Attempting auto-recovery (step 3)

[User types: RECONNECT]
[INFO] Reconnecting...
[Transitions back to CONNECTING_WIFI]

[After recovery and retry]
[001420][INFO][NetworkManager] WiFi connected!
[001420][INFO][StateMachine] State transition: CONNECTING_WIFI → CONNECTING_SERVER
```

## Impact

| Metric | Before | After |
|--------|--------|-------|
| Serial command latency | >30s (unresponsive) | <100ms |
| CPU load on failure | 100% | 10-15% |
| Recovery capability | Blocked | Asynchronous |
| State machine stuck | Yes (until watchdog) | No (timeout after 30s) |
| User intervention | Power cycle only | RECONNECT command |

---

## References

- OpenSpec Proposal: `openspec/changes/fix-infinite-loop-blocking-run/proposal.md`
- Design Document: `openspec/changes/fix-infinite-loop-blocking-run/design.md`
- Specifications: `openspec/changes/fix-infinite-loop-blocking-run/specs/*/spec.md`
- Implementation Tasks: `openspec/changes/fix-infinite-loop-blocking-run/tasks.md`
