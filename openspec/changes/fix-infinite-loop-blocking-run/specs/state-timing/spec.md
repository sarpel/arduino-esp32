# Specification: State Duration Tracking and Timeout Detection

## ADDED Requirements

### Requirement: Track State Entry Time

System SHALL record the entry time for each state and track the duration spent in each state.

#### Scenario: Track WiFi Connection Duration

State duration is tracked accurately across multiple iterations.

```text
t=0s: Transition to CONNECTING_WIFI
  └─ state_entry_time = millis()
  └─ state_duration = 0

t=5s: Check state duration
  └─ state_duration = 5000ms
  └─ Still < threshold (30000ms)
  └─ Continue in CONNECTING_WIFI

t=30s: Check state duration
  └─ state_duration = 30000ms
  └─ Equals timeout threshold
  └─ Trigger timeout handling

t=32s: Check state duration
  └─ state_duration = 32000ms
  └─ Exceeds timeout threshold
  └─ Transition to ERROR state
```

---

### Requirement: Define State Timeouts

Each state SHALL have a defined maximum duration before timeout is triggered.

#### Scenario: Different Timeouts for Different States

System applies different timeout thresholds based on state type.

```text
CONNECTING_WIFI timeout = 30 seconds (allow retries)
  └─ WiFi can be slow to scan/connect

CONNECTING_SERVER timeout = 10 seconds (server is local)
  └─ LAN server should respond quickly

INITIALIZING timeout = 10 seconds (shouldn't block)
  └─ Setup should complete immediately
```

---

### Requirement: Automatic Timeout Transition

When state duration exceeds timeout threshold, system SHALL automatically transition to ERROR state and trigger recovery.

#### Scenario: WiFi Connection Timeout

System stuck in CONNECTING_WIFI for 35 seconds automatically transitions to ERROR.

```text
System stuck in CONNECTING_WIFI for 35 seconds:

[001235][WARN][StateMachine] State timeout detected!
  ├─ Current state: CONNECTING_WIFI
  ├─ Duration: 35000ms
  ├─ Threshold: 30000ms
  ├─ Excess: 5000ms
  └─ Transitioning to ERROR state

[001236][INFO][SystemManager] Entering ERROR state
  └─ trigger recovery process
  └─ log diagnostic info (WiFi status, memory, errors)
```

---

### Requirement: State Duration Diagnostics

On state timeout, system SHALL log diagnostic information to aid debugging.

#### Scenario: Timeout Diagnostics

System logs comprehensive diagnostics when state timeout occurs.

```text
[001235][WARN][StateMachine] State timeout diagnostics:
  ├─ State: CONNECTING_WIFI
  ├─ Duration: 35000ms (timeout: 30000ms)
  ├─ Memory: 61 KB free (threshold: 50 KB)
  ├─ CPU Load: 95%
  ├─ WiFi Error: 0x3001 (task creation failed)
  ├─ Recovery Attempts: 2
  ├─ Last Error: Cannot create WiFi task
  └─ Recommended Action: Check memory fragmentation
```

---

### Requirement: Prevent Timeout False Positives

System SHALL NOT trigger timeout transitions during normal operation where state persistence is expected.

#### Scenario: Normal CONNECTED State

System in CONNECTED state persists indefinitely without timeout.

```text
System in CONNECTED state:
  ├─ Audio streaming normally
  ├─ No timeout threshold
  ├─ State duration monitored but not enforced
  └─ Continues indefinitely (until WiFi drops)
```

#### Scenario: Network Temporarily Slow

Server connection taking normal delay does not trigger timeout.

```text
Server connection taking 8 seconds (normal):
  └─ CONNECTING_SERVER timeout: 10 seconds
  └─ No timeout triggered
  └─ Connection succeeds at t=8s

But if server unreachable for 11 seconds:
  └─ At t=10s: Timeout triggers
  └─ Transition to ERROR for recovery
```

---

## Design Constraints

1. **Accurate Timing**: Timeout detection must occur within 1 second of threshold
2. **Per-State Logic**: Each state timeout is independent
3. **No Clock Wraparound**: Must handle `millis()` overflow (49 days)
4. **Low Overhead**: Duration tracking <16 bytes per state
5. **Observable**: All timeouts logged for debugging

## Implementation Notes

### State Duration Tracking

```cpp
struct StateData {
    SystemState current_state;
    unsigned long entry_time;
    uint32_t timeout_ms;
};

void updateStateDuration() {
    unsigned long current_time = millis();
    uint32_t state_duration = current_time - entry_time;

    if (state_duration > timeout_ms) {
        triggerStateTimeout(state_duration, timeout_ms);
    }
}
```

### State Timeout Configuration

```cpp
const uint32_t STATE_TIMEOUTS[] = {
    [SystemState::INITIALIZING] = 10000,
    [SystemState::CONNECTING_WIFI] = 30000,
    [SystemState::CONNECTING_SERVER] = 10000,
    [SystemState::CONNECTED] = 0,
    [SystemState::ERROR] = 5000,
    [SystemState::MAINTENANCE] = 10000,
    [SystemState::DISCONNECTED] = 5000,
};
```

## Validation Checklist

- [ ] State entry time recorded on transition
- [ ] State duration tracked accurately
- [ ] Timeout triggers at correct threshold
- [ ] Timeout triggers within 1 second accuracy
- [ ] Diagnostic information logged on timeout
- [ ] Clock wraparound handled correctly
- [ ] No false positives on normal CONNECTED state
- [ ] Manual override possible via RECONNECT command
