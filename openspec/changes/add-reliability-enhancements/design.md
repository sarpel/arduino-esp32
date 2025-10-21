# Technical Design: Reliability Enhancements

## Context

The ESP32 Audio Streamer requires production-grade reliability enhancements to achieve 99.5% uptime while operating under strict embedded system constraints:

**Hardware Constraints:**
- 320 KB RAM total (currently using <10% = ~32KB)
- 4 MB Flash (currently 59% used = ~1.6MB available)
- Dual-core 240 MHz Xtensa LX6 (cooperative multitasking)
- WiFi 2.4GHz only (no 5GHz band)
- 60-second watchdog timeout

**Software Constraints:**
- C++11 standard (Arduino framework)
- No std::make_unique (manual unique_ptr allocation)
- No true threading (FreeRTOS tasks only)
- Arduino macro conflicts (INPUT, OUTPUT enums)
- PlatformIO build system

**Stakeholders:**
- Developers: Need maintainable, modular reliability architecture
- Operators: Need visibility and automatic recovery
- End Users: Need reliable, uninterrupted audio streaming

## Goals / Non-Goals

**Goals:**
- Achieve 99.5% uptime with automatic failure recovery
- Provide comprehensive system health visibility
- Support graceful degradation under adverse conditions
- Maintain backward compatibility with existing functionality
- Keep resource overhead minimal (<12KB RAM, <45KB Flash, <5% CPU)
- Enable incremental feature adoption via configuration

**Non-Goals:**
- Cloud-based monitoring or remote management (future phase)
- Real-time audio quality enhancement during streaming
- Hardware fault tolerance (multiple microphones, redundant hardware)
- IPv6 or 5GHz WiFi support (hardware limitation)
- Support for non-ESP32 platforms

## Decisions

### Decision 1: Multi-WiFi Architecture

**What:** Implement MultiWiFiManager with priority-based network selection and automatic failover.

**Why:**
- Single WiFi network is a critical single point of failure
- Home/office environments typically have multiple access points
- Automatic failover provides seamless reliability without user intervention

**How:**
- Priority queue of WiFi credentials (2-5 networks, configurable)
- NetworkQualityMonitor tracks RSSI, packet loss, RTT per network
- Automatic switching when primary network quality drops below threshold
- Switchover time target: <5 seconds without audio loss

**Alternatives Considered:**
- Mesh WiFi support: Rejected due to complexity and limited ESP32 RAM
- Manual network selection: Rejected due to poor user experience
- WiFi roaming (802.11r): Rejected due to limited AP support in target environments

**Resource Impact:**
- RAM: ~4KB (WiFi credential storage + quality metrics)
- Flash: ~15KB (switching logic + quality monitoring)
- CPU: ~1% (periodic quality checks every 10s)

### Decision 2: Health Scoring Algorithm

**What:** Weighted composite health score (0-100%) with component-level tracking.

**Why:**
- Single metric simplifies decision-making for automatic recovery
- Component weights reflect criticality to audio streaming mission
- Enables trend-based predictive failure detection

**How:**
- Network health: 40% weight (RSSI, connectivity, packet loss)
- Memory health: 30% weight (heap usage, fragmentation)
- Audio health: 20% weight (I2S errors, buffer underruns)
- System health: 10% weight (uptime, CPU load, temperature)
- Computed every 10 seconds using exponential moving average

**Alternatives Considered:**
- Binary health (healthy/unhealthy): Rejected due to lack of nuance
- ML-based health prediction: Rejected due to RAM/CPU constraints
- Equal component weights: Rejected because network is most critical

**Resource Impact:**
- RAM: ~3KB (60s sliding window for trend analysis)
- Flash: ~12KB (scoring algorithms + trend analysis)
- CPU: ~2% (health computation every 10s)

### Decision 3: Circuit Breaker Pattern

**What:** Three-state circuit breaker (CLOSED, OPEN, HALF_OPEN) to prevent cascading failures.

**Why:**
- Prevents repeated failed connection attempts from exhausting resources
- Allows system to "fail fast" and recover gracefully
- Standard pattern proven in distributed systems

**How:**
- CLOSED: Normal operation, failures counted
- OPEN: After N failures (configurable, default 5), reject requests immediately
- HALF_OPEN: After timeout, allow single test request
- Transition back to CLOSED on success, back to OPEN on failure
- Separate circuit breakers for WiFi, TCP, I2S components

**Alternatives Considered:**
- Retry with exponential backoff only: Current implementation, insufficient for cascading failures
- Rate limiting: Rejected as doesn't prevent cascading failures
- Bulkhead pattern: Rejected due to single-core limitation

**Resource Impact:**
- RAM: ~1KB (circuit breaker state per component)
- Flash: ~8KB (state machine logic)
- CPU: <1% (state tracking overhead)

### Decision 4: State Persistence Strategy

**What:** Serialize critical state to EEPROM/Flash for crash recovery using simple key-value format.

**Why:**
- ESP32 can crash due to power loss, memory corruption, watchdog timeout
- Restoring state enables faster recovery and better user experience
- Reduces data loss and connection re-establishment time

**How:**
- Store: WiFi network index, connection stats, health history
- Format: Simple TLV (Type-Length-Value) for space efficiency
- Write: On state changes (rate-limited to prevent flash wear)
- Read: During initialization after crash detection
- CRC validation to detect corruption

**Alternatives Considered:**
- No persistence: Rejected due to poor recovery experience
- Full state snapshot: Rejected due to flash wear and write time
- SD card storage: Rejected due to additional hardware requirement

**Resource Impact:**
- RAM: ~512 bytes (serialization buffer)
- Flash/EEPROM: ~1KB (persistent state storage)
- CPU: ~1% (periodic state writes)

### Decision 5: Degradation Mode Strategy

**What:** Four-level degradation hierarchy with automatic transitions.

**Why:**
- Allows system to continue operating at reduced capability rather than failing completely
- Provides graceful degradation path under resource constraints
- Enables recovery without full system restart

**Modes:**
1. **NORMAL**: Full features, 16kHz/16-bit audio, all monitoring active
2. **REDUCED_QUALITY**: 8kHz/8-bit audio, reduced telemetry, conserve bandwidth
3. **SAFE_MODE**: Audio streaming only, minimal monitoring, basic error handling
4. **RECOVERY**: No streaming, focus on restoring connectivity and health

**Transition Logic:**
- Health score < 80%: NORMAL → REDUCED_QUALITY
- Health score < 60%: REDUCED_QUALITY → SAFE_MODE
- 3 consecutive failures: Any mode → RECOVERY
- Health score > 85% for 60s: Transition back to higher mode

**Alternatives Considered:**
- Binary mode (normal/safe): Rejected due to lack of gradual degradation
- Five or more modes: Rejected due to complexity without clear benefit

**Resource Impact:**
- RAM: ~256 bytes (mode state and thresholds)
- Flash: ~5KB (mode transition logic)
- CPU: <1% (mode evaluation)

## Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         SystemManager                           │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │               EventBus (Publish-Subscribe)               │   │
│  └───────────────────────┬──────────────────────────────────┘   │
│                          │                                      │
│  ┌───────────┬───────────┼──────────┬──────────┬──────────┐    │
│  │           │           │          │          │          │    │
│  ▼           ▼           ▼          ▼          ▼          ▼    │
│┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────┐ │
││MultiWiFi││Health   ││Circuit  ││Telemetry││Auto     ││State│ │
││Manager  ││Monitor  ││Breaker  ││Collector││Recovery ││Mgr  │ │
│└─────────┘└─────────┘└─────────┘└─────────┘└─────────┘└─────┘ │
│     │          │          │          │          │               │
│     ▼          ▼          ▼          ▼          ▼               │
│┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────────┐        │
││Network  ││Trend    ││Degrade  ││Metrics  ││State    │        │
││Quality  ││Analyzer ││Manager  ││Tracker  ││Serializer│       │
││Monitor  ││         ││         ││         ││         │        │
│└─────────┘└─────────┘└─────────┘└─────────┘└─────────┘        │
│     │          │          │          │          │               │
│     └──────────┴──────────┴──────────┴──────────┘               │
│                          │                                      │
│                          ▼                                      │
│                 ┌─────────────────┐                             │
│                 │  Memory Manager │                             │
│                 │ (Pool Allocator)│                             │
│                 └─────────────────┘                             │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow

**Health Monitoring Flow:**
```
1. Components publish health metrics → EventBus
2. HealthMonitor subscribes to metrics events
3. HealthMonitor computes component scores (network, memory, audio, system)
4. HealthMonitor computes weighted composite score (0-100%)
5. TrendAnalyzer maintains 60s sliding window of scores
6. PredictiveDetector analyzes trends for anomalies
7. If prediction confidence > 80%, publish warning event
8. AutoRecovery subscribes to warnings and triggers mitigation
```

**Network Failover Flow:**
```
1. NetworkQualityMonitor tracks RSSI, packet loss, RTT every 10s
2. Quality score drops below threshold (RSSI < -80dBm or packet loss > 5%)
3. MultiWiFiManager receives quality degradation event
4. Circuit breaker for current network transitions to OPEN
5. MultiWiFiManager selects next priority network
6. Disconnect from current network (graceful if possible)
7. Connect to backup network (<5s target)
8. Re-establish TCP connection via ConnectionPool
9. Resume audio streaming with minimal interruption
```

**Failure Recovery Flow:**
```
1. Component (WiFi/TCP/I2S) fails and publishes failure event
2. CircuitBreaker for component opens (reject subsequent requests)
3. HealthMonitor updates component health score
4. If composite health < 60%, DegradationManager transitions to SAFE_MODE
5. TelemetryCollector logs failure event with context
6. AutoRecovery determines recovery strategy based on failure type
7. StateSerializer persists current state to EEPROM
8. Execute recovery procedure (reconnect, reinitialize, degrade)
9. Circuit breaker transitions to HALF_OPEN for test request
10. On success, restore to previous degradation mode
```

## Risks / Trade-offs

### Risk 1: RAM Exhaustion
**Description:** Reliability features add ~12KB RAM overhead, which could cause memory pressure under peak usage.

**Probability:** Medium
**Impact:** High (system crash, watchdog reset)

**Mitigation:**
- Extensive memory profiling at each implementation phase
- Configurable feature disable via compile-time flags
- Memory pool allocation to prevent fragmentation
- Stress testing with continuous operation for 72+ hours
- Watchdog protection for memory threshold violations

**Trade-off:** Accept 12KB overhead for significant reliability gains

### Risk 2: Flash Wear from State Persistence
**Description:** Frequent EEPROM/Flash writes can exceed write cycle limits (100K-1M cycles).

**Probability:** Low
**Impact:** Medium (state persistence failure)

**Mitigation:**
- Rate-limit writes to max 1 write per 60 seconds
- Only write on significant state changes (not periodic)
- Use wear leveling if available on ESP32 partition
- Monitor write counts via diagnostics interface
- Alert when approaching write cycle limit (80% threshold)

**Trade-off:** Balance recovery speed vs flash longevity (60s write interval)

### Risk 3: Network Switching Audio Gaps
**Description:** WiFi network switching may cause brief audio interruption (target <5s).

**Probability:** Medium
**Impact:** Low (brief service interruption)

**Mitigation:**
- Pre-connect to backup network when quality degrades
- Buffering strategy to minimize perceptible gaps
- Graceful TCP disconnect with connection draining
- Fast reconnection via ConnectionPool
- User notification of degraded mode

**Trade-off:** Brief interruption vs catastrophic failure from network loss

### Risk 4: Predictive Detection False Positives
**Description:** Overly sensitive trend analysis may trigger unnecessary recovery actions.

**Probability:** Medium
**Impact:** Low (unnecessary mode transitions)

**Mitigation:**
- Configurable sensitivity thresholds
- Require multiple confirming signals before action
- Hysteresis in mode transitions (different up/down thresholds)
- Extensive testing under varied network conditions
- Telemetry for false positive/negative rate monitoring

**Trade-off:** Some false positives acceptable to catch real failures early

## Migration Plan

### Phase 1: Network Resilience (Weeks 1-3)
**Implementation:**
1. Create MultiWiFiManager class with priority queue
2. Implement NetworkQualityMonitor (RSSI, packet loss, RTT)
3. Add ConnectionPool for primary + backup connections
4. Implement automatic failover logic
5. Add configuration for multiple WiFi credentials

**Testing:**
- Unit tests for WiFi switching logic
- Integration tests for failover scenarios
- Network simulation tests (disconnect, weak signal)
- 24-hour stability test with forced failovers

**Rollback:** Feature flag in config.h to disable multi-WiFi

### Phase 2: Health Monitoring (Weeks 4-5)
**Implementation:**
1. Create HealthMonitor with component-level scoring
2. Implement TrendAnalyzer with sliding window
3. Add PredictiveDetector for anomaly detection
4. Create pluggable health check framework
5. Add health metrics to serial diagnostics

**Testing:**
- Unit tests for health scoring algorithms
- Trend analysis validation tests
- Prediction accuracy tests with known failure patterns
- Integration with existing monitoring

**Rollback:** Health monitoring passive only (no action triggers)

### Phase 3: Failure Recovery (Weeks 6-7)
**Implementation:**
1. Create CircuitBreaker pattern for WiFi/TCP/I2S
2. Implement DegradationManager with mode transitions
3. Add StateSerializer for EEPROM persistence
4. Create AutoRecovery with recovery strategies
5. Integrate with health monitoring for triggers

**Testing:**
- Circuit breaker state transition tests
- Degradation mode transition tests
- State persistence/recovery tests
- Crash recovery simulation tests
- 72-hour stress test with induced failures

**Rollback:** Circuit breaker passive mode (monitor only)

### Phase 4: Observability (Week 8-9)
**Implementation:**
1. Create TelemetryCollector with circular buffer
2. Implement MetricsTracker for KPIs
3. Enhance DiagnosticsInterface with new commands
4. Add event logging for critical failures
5. Create comprehensive diagnostics report

**Testing:**
- Telemetry collection validation
- Metrics accuracy verification
- Diagnostics command testing
- Buffer overflow handling tests

**Rollback:** Observability optional (minimal telemetry)

### Validation Criteria
Each phase must pass before proceeding:
- [ ] Zero compilation errors/warnings
- [ ] All unit tests passing
- [ ] Integration tests passing
- [ ] Memory usage within budget (+3KB max per phase)
- [ ] No performance regression (>5% CPU overhead)
- [ ] 24-hour stability test passing
- [ ] Documentation updated

## Open Questions

1. **Q:** Should we support dynamic WiFi credential updates via serial/BLE?
   **A:** Deferred to future enhancement (out of scope for this change)

2. **Q:** What is the optimal health check interval (currently 10s)?
   **A:** Will be tunable via configuration, validated through testing

3. **Q:** Should circuit breaker thresholds be per-component or global?
   **A:** Per-component for fine-grained control, with global override

4. **Q:** How should we handle EEPROM write failures?
   **A:** Log error, continue operation without persistence (graceful degradation)

5. **Q:** Should health scores be exposed via web API in future?
   **A:** Yes, but deferred to future web interface enhancement
