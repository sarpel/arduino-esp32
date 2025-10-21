# Fix Infinite Loop in SystemManager::run()

## Problem Statement

The system enters an **unrecoverable error loop** when WiFi initialization fails:

1. **Root Cause**: `SystemManager::run()` contains a blocking `while (system_running)` loop (line 217)
   - This freezes the main Arduino loop, preventing serial commands and watchdog resets
   - WiFi task creation fails with error `0x3001` due to insufficient resources
   - Health monitor attempts auto-recovery in a tight loop
   - CPU overload detected repeatedly (every ~10 seconds)
   - System cannot recover because the infinite loop blocks recovery attempts

2. **Current Symptom**:
   ```
   [WARN] CPU overload detected! (repeated every 10s)
   E (5003) wifi:create wifi task: failed to create task
   [015036][INFO][HealthMonitor] Attempting auto-recovery
   [CRITICAL][MemoryManager] Emergency cleanup initiated (#1)
   ...infinite retry loop...
   ```

3. **Scope**: Architecture affects entire system—WiFi, health monitoring, and main control flow

## Why

This change is critical for production readiness:
- **System Reliability**: Current design makes system unrecoverable when WiFi fails (power cycle required)
- **User Experience**: Serial commands become unresponsive for 30+ seconds
- **Architecture Correctness**: Arduino framework expects `loop()` to return frequently
- **Resource Efficiency**: Enables graceful degradation instead of 100% CPU spin
- **Observability**: Prevents watchdog-induced resets by improving recovery capability

## Solution Overview

**Refactor `SystemManager::run()` from blocking loop to event-driven non-blocking design**:
- Remove `while (system_running)` blocking loop
- Make `run()` perform **one iteration** per Arduino loop cycle
- Preserve state machine transitions and recovery logic
- Enable serial command handling and graceful degradation

## Design Rationale

### Why This Matters
- **Arduino Framework**: Expects non-blocking `loop()` function that returns frequently
- **Watchdog Timer**: 60-second timeout requires regular resets (done in `run()`)
- **Recovery Path**: Blocking loop prevents health monitor from executing recovery
- **Observability**: Serial commands become unresponsive

### Key Changes
1. **Remove blocking while loop** → One iteration per `loop()` call
2. **Preserve state machine logic** → Same transitions, non-blocking
3. **Add iteration counter** → Track cycles per second without blocking
4. **Keep watchdog reset** → Prevent timeout during normal operation

### Trade-offs
| Aspect | Blocking Loop | Non-Blocking Iteration |
|--------|---------------|----------------------|
| Timing control | CPU frequency dependent | Slight variation (10-50ms) |
| Loop frequency | Exact (via CYCLE_TIME_MS) | Target-based, actual ~100 Hz |
| Responsiveness | None during WiFi retry | Full responsiveness |
| Recovery capability | Blocked | Enabled (async retry) |
| Resource consumption | Lower (no extra state) | Minimal (4-8 bytes state) |

## Specifications

See: `openspec/changes/fix-infinite-loop-blocking-run/specs/*/spec.md`

## Tasks

See: `openspec/changes/fix-infinite-loop-blocking-run/tasks.md`

## Implementation Notes

- **Compatibility**: Maintains backward compatibility with existing state machine
- **Testing**: Requires integration tests for WiFi failure scenarios
- **Validation**: CPU load should drop significantly when WiFi fails (from 100% to ~10%)
