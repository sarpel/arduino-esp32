# OpenSpec Proposal Summary: Fix Infinite Loop in SystemManager::run()

## Change ID
`fix-infinite-loop-blocking-run`

## Problem

The ESP32 Audio Streamer system enters an **unrecoverable error loop** when WiFi initialization fails:

**Symptoms** (from terminal log):
```
[WARN] CPU overload detected! (repeated every 10 seconds)
E (5003) wifi:create wifi task: failed to create task
[015036][INFO][HealthMonitor] Attempting auto-recovery
[CRITICAL][MemoryManager] Emergency cleanup initiated (#1)
...infinite retry loop...
```

**Root Cause**:
- `SystemManager::run()` contains a blocking `while (system_running)` loop (line 217)
- This infinite loop prevents Arduino `loop()` from returning
- Serial commands cannot execute
- Health monitor recovery attempts are blocked
- System appears frozen while retrying

## Solution

**Refactor `SystemManager::run()` from blocking infinite loop to non-blocking event-driven design**:

```cpp
// BEFORE (blocking infinite loop)
void SystemManager::run() {
    while (system_running) {  // ← BLOCKS FOREVER
        // State machine logic
        // Network operations
        // Audio streaming
        delay(CYCLE_TIME_MS);
    }
    // Never reaches here
}

// AFTER (non-blocking single iteration)
void SystemManager::run() {
    // Execute one complete cycle
    // State machine logic
    // Network operations
    // Audio streaming

    // Calculate remaining sleep time
    // Return to Arduino loop after 10-50ms
}
```

## Key Benefits

✅ **System Remains Responsive**: Serial commands (RECONNECT, STATUS, etc.) execute immediately
✅ **Recovery Works**: Health monitor can attempt recovery asynchronously
✅ **No CPU Spin**: Reduces CPU from 100% to ~10-15% during failures
✅ **Graceful Degradation**: System recovers from WiFi errors automatically
✅ **Backward Compatible**: No API changes, same state machine behavior

## Specifications Included

### 1. Blocking Loop Removal (`specs/blocking-loop-removal/spec.md`)
- Remove infinite `while (system_running)` loop
- Execute one iteration per Arduino `loop()` call
- Return within 100ms
- Preserve state machine transitions
- Maintain watchdog reset frequency
- Enable serial command processing

### 2. Async Recovery (`specs/async-recovery/spec.md`)
- Implement step-based (non-blocking) recovery
- Execute recovery over multiple 1-2 second intervals
- Exponential backoff between retry attempts
- Distribute health checks across iterations
- Limit recovery attempts to prevent infinite loops
- Graceful failure escalation

### 3. State Duration Tracking (`specs/state-timing/spec.md`)
- Track time spent in each state
- Define timeout thresholds (30s for WiFi, 10s for server, etc.)
- Automatically transition to ERROR on timeout
- Log diagnostic information on timeout
- Handle `millis()` wraparound correctly
- Prevent stuck states

## Implementation Plan

**5 Phases | 16 hours total work**:

### Phase 1: Code Refactoring (4 hours)
1. Remove blocking loop from `SystemManager::run()`
2. Remove blocking delays in WiFi connection handling
3. Update main loop structure (should already be non-blocking)

### Phase 2: State Duration Tracking (4 hours)
4. Add state entry time tracking to `StateMachine`
5. Define timeout thresholds for each state
6. Implement timeout detection in `SystemManager::run()`
7. Add diagnostic logging on timeout

### Phase 3: Async Recovery (4 hours)
8. Add recovery iteration state to `HealthMonitor`
9. Refactor recovery to step-based execution
10. Distribute health checks across iterations
11. Implement recovery attempt limits

### Phase 4: Testing (3 hours)
- Unit tests for non-blocking loop
- Integration tests for WiFi failure scenario
- Serial command responsiveness tests
- Load tests for CPU/memory during failure
- Manual hardware testing

### Phase 5: Documentation (1 hour)
- Update code documentation
- Update architecture documentation
- Merge to main branch

## Expected Outcomes

**Before Fix**:
- System freezes on WiFi failure
- Serial commands unresponsive (>30s delay)
- CPU stuck at 100% in retry loop
- Auto-recovery blocked by infinite loop
- User must power cycle device

**After Fix**:
- System remains responsive during WiFi retry
- Serial commands respond in <100ms
- CPU load ~10-15% during failure
- Auto-recovery executes asynchronously
- State timeout triggers after 30s
- User can issue RECONNECT to retry
- System recovers gracefully without power cycle

## Files Created

```
openspec/changes/fix-infinite-loop-blocking-run/
├── proposal.md                           # Problem statement & solution overview
├── design.md                             # Detailed architectural design
├── tasks.md                              # 14 implementation tasks with dependencies
└── specs/
    ├── blocking-loop-removal/
    │   └── spec.md                       # Remove infinite loop specification
    ├── async-recovery/
    │   └── spec.md                       # Step-based recovery specification
    └── state-timing/
        └── spec.md                       # Timeout detection specification
```

## Next Steps

1. ✅ **Review this proposal** - Check architecture, specifications, and tasks
2. ⏭️ **Validate proposal** - Run `openspec validate fix-infinite-loop-blocking-run --strict`
3. ⏭️ **Approve specification** - Review and accept all requirement scenarios
4. ⏭️ **Begin implementation** - Start with Phase 1 (refactoring)
5. ⏭️ **Execute Phase 2-3** - Add timeout detection and async recovery
6. ⏭️ **Complete Phase 4** - Comprehensive testing
7. ⏭️ **Deploy** - Merge to main branch after all tests pass

## Questions?

Refer to:
- `proposal.md` - Problem details and solution rationale
- `design.md` - Architectural reasoning and component interactions
- `specs/*/spec.md` - Detailed requirements with scenario examples
- `tasks.md` - Step-by-step implementation with validation criteria

---

**Change Status**: Proposal ready for review
**Priority**: Critical (blocks production use)
**Effort**: 16 hours (8-10 with parallelization)
**Risk**: Low (non-breaking, pure refactoring)
