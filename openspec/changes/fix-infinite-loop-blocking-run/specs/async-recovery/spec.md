# Specification: Async Recovery Implementation

## ADDED Requirements

### Requirement: Step-Based Recovery Execution
Recovery operations SHALL execute incrementally (one step per 1-2 seconds) rather than blocking in tight loops.

#### Scenario: Memory Defragmentation During WiFi Failure
WiFi task creation fails with insufficient memory (0x3001). Recovery executes asynchronously across multiple iterations.

```
WiFi task creation fails with insufficient memory (0x3001)

Iteration 0 (t=0s):
  └─ health_monitor.attemptRecovery()
     └─ Step 1: Trigger emergency cleanup
     └─ [Return to main loop]

Iteration 50 (t=5s):
  └─ health_monitor.attemptRecovery()
     └─ Step 2: Perform memory defragmentation
     └─ [Return to main loop]

Iteration 100 (t=10s):
  └─ health_monitor.attemptRecovery()
     └─ Step 3: Verify memory available
     └─ [Return to main loop]

Iteration 101 (t=10.1s):
  └─ network_manager.handleWiFiConnection()
     └─ Retry WiFi task creation
     └─ [Should succeed now]
```

---

### Requirement: Recovery Iteration Rate Limiting
Recovery operations SHALL be rate-limited to prevent CPU overload and allow system stabilization between attempts.

#### Scenario: Exponential Backoff in Recovery
Recovery attempts are spaced apart with increasing delays between attempts.

```
First recovery failure (t=0s):
  └─ Attempt immediately

Second recovery failure (t=1s):
  └─ Wait 1 second before next attempt

Third recovery failure (t=3s):
  └─ Wait 2 seconds before next attempt

Fourth recovery failure (t=6s):
  └─ Wait 4 seconds before next attempt
  └─ Cap at 10 second maximum backoff
```

---

### Requirement: Health Check Iteration Tracking
Health monitor SHALL track which check is being performed on each iteration to distribute CPU load evenly.

#### Scenario: Distributed Health Checks
Each health check executes in a separate iteration over a 5-second cycle.

```
Iteration 0:
  └─ Check 0: CPU load

Iteration 1:
  └─ Check 1: Memory pressure

Iteration 2:
  └─ Check 2: Network stability

Iteration 3:
  └─ Check 3: Audio quality

Iteration 4:
  └─ Check 4: WiFi signal strength

Iteration 5:
  └─ Check 0: CPU load (cycle repeats)
```

---

### Requirement: Recovery State Machine
Recovery operations SHALL follow a defined state machine to prevent infinite recovery loops and detect unrecoverable conditions.

#### Scenario: Recovery State Transitions
System transitions through recovery states in response to health issues.

```
RECOVERY_IDLE
├─ Health issue detected → RECOVERY_CLEANUP

RECOVERY_CLEANUP
├─ Complete successfully → RECOVERY_DEFRAG
└─ Fail → RECOVERY_FAILED

RECOVERY_DEFRAG
├─ Complete successfully → RECOVERY_RETRY
└─ Fail → RECOVERY_FAILED

RECOVERY_RETRY
├─ Wait time elapsed → RECOVERY_CLEANUP (retry)
└─ Max retries exceeded → RECOVERY_FAILED

RECOVERY_FAILED
└─ Escalate to system error/restart
```

---

### Requirement: Limit Recovery Attempts
System SHALL limit recovery attempts to a maximum (e.g., 3 attempts per health check failure) to prevent infinite recovery loops.

#### Scenario: Max Recovery Attempts Exceeded
System gracefully escalates to error state after maximum recovery attempts.

```
Attempt 1: Emergency cleanup → WiFi still fails
Attempt 2: Memory defragmentation → WiFi still fails
Attempt 3: Reset audio processor → WiFi still fails

After 3 failed attempts:
  └─ Transition to ERROR state
  └─ Log critical failure
  └─ Escalate to user via serial
  └─ Wait for manual intervention (RECONNECT command)
```

---

## Design Constraints

1. **Non-Blocking**: Each `attemptRecovery()` call must complete in <50ms
2. **Idempotent**: Recovery operations must be safe to repeat
3. **Observable**: Log all recovery steps for debugging
4. **Bounded**: Must not retry infinitely
5. **Graceful**: System must degrade gracefully, not crash

## Implementation Notes

### Recovery State Tracking
```cpp
struct RecoveryState {
    RecoveryPhase current_phase;
    uint32_t attempt_count;
    unsigned long last_attempt_time;
    uint16_t backoff_delay_ms;
};
```

## Validation Checklist

- [ ] Recovery operations execute incrementally
- [ ] No CPU overload during recovery
- [ ] Recovery attempts limited to maximum
- [ ] Exponential backoff implemented
- [ ] Health checks distributed across iterations
- [ ] Recovery state transitions correct
- [ ] System doesn't restart unnecessarily
