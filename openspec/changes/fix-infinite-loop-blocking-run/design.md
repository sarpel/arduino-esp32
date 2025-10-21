# Design: Non-Blocking SystemManager::run() Architecture

## Current Architecture Problem

```
Arduino loop()
├─ systemManager.run()  ← **BLOCKING INFINITE LOOP**
│  └─ while (system_running) {
│     ├─ Network operations (WiFi retry, server connect)
│     ├─ Health checks
│     └─ Audio streaming
│     └─ [NEVER RETURNS - blocks main loop]
└─ handleSerialCommands()  ← Never executes
```

**Impact**: When WiFi fails, the entire system freezes in retry loop. Serial commands cannot execute. Health monitor cannot run recovery. Watchdog timeout is only prevented by loop iteration counter.

## Target Architecture

```
Arduino loop()
├─ systemManager.run()  ← **ONE ITERATION PER CALL**
│  ├─ Feed watchdog
│  ├─ Update context (CPU, memory, network stats)
│  ├─ Process one state machine transition
│  ├─ Perform one health check iteration
│  └─ [RETURNS after ~10-50ms work]
├─ handleSerialCommands()  ← Now executes every loop
└─ yield() / delay(10)  ← Prevents busy-wait
```

**Benefit**: System remains responsive to serial commands. Recovery operations can execute asynchronously. Graceful degradation when WiFi fails.

## State Machine Iteration Model

### Current (Blocking)
```
CONNECTING_WIFI
├─ Retry WiFi connection in tight loop
├─ Block until connected or max retries
└─ Only then return to Arduino loop
```

### Proposed (Non-Blocking)
```
Per loop() call:
├─ CONNECTING_WIFI iteration #N
│  ├─ Attempt WiFi scan
│  ├─ Check if connected
│  └─ Return to Arduino loop
│
├─ CONNECTING_WIFI iteration #N+1
│  ├─ Attempt WiFi connection
│  └─ Return to Arduino loop
│
├─ ... (repeated state until connected)
│
└─ Once connected → transition to CONNECTING_SERVER
```

**Key**: Multiple iterations occur per second (target: 100 Hz). Each iteration is non-blocking.

## Implementation Strategy

### 1. Remove Blocking Loop

**Before:**
```cpp
void SystemManager::run() {
    while (system_running) {  // ← BLOCKING INFINITE LOOP
        // State machine logic
        // Health checks
        // Audio streaming
        delay(CYCLE_TIME_MS);
    }
}
```

**After:**
```cpp
void SystemManager::run() {
    // One complete cycle per call
    // State machine logic
    // Health checks
    // Audio streaming

    // Calculate sleep time
    // Return to Arduino loop
}
```

### 2. Preserve Timing Control

**Time Management**:
- Target: 100 Hz loop frequency (10ms per iteration)
- Arduino `loop()` naturally runs at 1000+ Hz
- Each `run()` call completes in ~10-50ms
- Sleep/yield prevents CPU overload

**Implementation**:
```cpp
unsigned long cycle_time = millis() - cycle_start_time;
if (cycle_time < CYCLE_TIME_MS) {
    delay(CYCLE_TIME_MS - cycle_time);  // Sleep to maintain 100 Hz
}
```

### 3. Add State for Async Recovery

**Current Health Monitor** (blocks during recovery):
```
performHealthChecks() {
    while (needs_recovery) {
        attemptRecovery();
        delay(100);
    }
}
```

**Proposed** (one step per iteration):
```
performHealthChecks() {
    if (needs_recovery && can_attempt_now()) {
        attemptRecovery();  // One step only
    }
}
```

## Component Integration

### SystemManager Changes
- Remove `while (system_running)` loop
- Add state tracking for multi-step operations (WiFi retry, recovery)
- Preserve state machine, event bus, health monitor integration

### HealthMonitor Changes
- Change `canAutoRecover()` from blocking to step-based
- Add recovery iteration counter
- Limit recovery attempts per time window (exponential backoff)

### NetworkManager Changes
- No changes needed (already non-blocking)
- `handleWiFiConnection()` already designed for per-iteration calls

### StateMachine Changes
- Add timing state (track duration in each state)
- Detect stuck states (> 30 seconds in CONNECTING_WIFI)
- Trigger error state on timeout

## Recovery Flow Example

### Scenario: WiFi Task Creation Fails

```
Iteration 0: CONNECTING_WIFI
  ├─ network_manager.handleWiFiConnection()
  │  └─ WiFi task creation fails (0x3001)
  └─ [Return to Arduino loop - serial commands responsive]

Iteration 1: CONNECTING_WIFI
  ├─ health_monitor.performHealthChecks()
  │  └─ Detect WiFi task failure
  └─ [Schedule recovery attempt]

Iteration 2: CONNECTING_WIFI
  ├─ health_monitor.canAutoRecover()
  │  └─ Attempt recovery (defrag memory, reduce load)
  └─ [Return, allow system to stabilize]

Iteration 3: CONNECTING_WIFI
  ├─ network_manager.handleWiFiConnection()
  │  └─ Retry WiFi task creation
  └─ [If successful, transition to next state]
```

**Key Improvements**:
- System doesn't freeze during retry
- Recovery operations can execute
- Serial commands work (e.g., user can issue `RECONNECT`)
- Watchdog resets regularly

## Performance Impact

### CPU Load
- **Before**: 100% in `systemManager.run()` loop
- **After**: ~10-15% average (busy for 10-50ms, sleep for remainder)

### Memory
- **Before**: Stack usage in while loop
- **After**: Minimal (~8 bytes for iteration tracking)

### Latency
- **Audio streaming**: Same (<10ms buffering)
- **State transitions**: Slightly slower (up to 100ms, acceptable)
- **Serial commands**: Now <100ms response

### Responsiveness
- **Serial commands**: Immediate
- **Recovery operations**: 1-2 seconds
- **Network reconnect**: Graceful (exponential backoff)

## Backward Compatibility

✅ **Maintained**:
- State machine public interface unchanged
- Same state transitions
- Same timeout behavior
- Same error handling logic

⚠️ **Minor Changes**:
- Timing becomes approximate (10-50ms variance)
- Recovery is asynchronous (different timing)
- State duration tracking needed

## Validation Strategy

1. **Unit Tests**: Verify single iteration completes
2. **Integration Tests**: WiFi failure scenario → no freeze
3. **Load Tests**: CPU load remains <20%
4. **Recovery Tests**: System recovers from WiFi failure
5. **Serial Tests**: Commands execute during WiFi retry

## Related Specifications

- `specs/blocking-loop-removal/spec.md` - Remove blocking loop
- `specs/async-recovery/spec.md` - Implement async recovery
- `specs/state-timing/spec.md` - Add state duration tracking
