# Failure Recovery Specification

## ADDED Requirements

### Requirement: Circuit Breaker Pattern
The system SHALL implement the circuit breaker pattern with three states (CLOSED, OPEN, HALF_OPEN) to prevent cascading failures and enable graceful failure handling for critical components.

#### Scenario: Circuit breaker in CLOSED state
- **WHEN** component (WiFi/TCP/I2S) is operating normally
- **THEN** circuit breaker is in CLOSED state allowing all requests
- **AND** circuit breaker monitors failure count (resets every 60 seconds)
- **AND** circuit breaker tracks success rate for trend analysis

#### Scenario: Transition to OPEN state on failures
- **WHEN** component experiences 5 consecutive failures within 60 seconds
- **THEN** circuit breaker transitions to OPEN state
- **AND** circuit breaker immediately rejects all subsequent requests
- **AND** circuit breaker starts recovery timer (30 seconds default)
- **AND** system publishes circuit breaker opened event

#### Scenario: HALF_OPEN state for recovery testing
- **WHEN** circuit breaker has been OPEN for 30 seconds
- **THEN** circuit breaker transitions to HALF_OPEN state
- **AND** circuit breaker allows single test request to probe component health
- **AND** circuit breaker transitions to CLOSED on success
- **AND** circuit breaker transitions back to OPEN on failure with doubled recovery timer

#### Scenario: Prevent cascading failures
- **WHEN** WiFi circuit breaker is OPEN due to connection failures
- **THEN** TCP connection attempts are immediately rejected
- **AND** system avoids wasting resources on doomed connection attempts
- **AND** system publishes dependency failure event for monitoring

### Requirement: Automatic Degradation Modes
The system SHALL support four degradation modes (NORMAL, REDUCED_QUALITY, SAFE_MODE, RECOVERY) with automatic transitions based on system health to maintain operation under adverse conditions.

#### Scenario: NORMAL mode operation
- **WHEN** system health score is ≥80%
- **THEN** system operates in NORMAL mode
- **AND** audio streaming at full quality (16kHz, 16-bit, mono)
- **AND** all monitoring and telemetry features active
- **AND** all reliability features enabled

#### Scenario: Degrade to REDUCED_QUALITY mode
- **WHEN** system health score drops below 80% for 30 seconds
- **THEN** system transitions to REDUCED_QUALITY mode
- **AND** audio quality reduced to 8kHz, 8-bit to conserve bandwidth
- **AND** telemetry collection interval increased from 10s to 30s
- **AND** system publishes mode transition event

#### Scenario: Degrade to SAFE_MODE
- **WHEN** system health score drops below 60% for 30 seconds
- **OR** 2 consecutive component failures occur
- **THEN** system transitions to SAFE_MODE
- **AND** audio streaming only, minimal monitoring active
- **AND** predictive failure detection disabled to conserve resources
- **AND** circuit breaker thresholds relaxed to allow recovery attempts

#### Scenario: Enter RECOVERY mode
- **WHEN** 3 consecutive failures occur across any components
- **OR** system health score drops below 40%
- **THEN** system transitions to RECOVERY mode
- **AND** audio streaming paused
- **AND** system focuses on restoring connectivity and health
- **AND** system attempts component reinitialization sequence

#### Scenario: Transition back to higher mode
- **WHEN** in degraded mode and health score improves
- **THEN** system requires health score >85% for 60 seconds before upgrade
- **AND** system transitions one level at a time (RECOVERY → SAFE_MODE → REDUCED → NORMAL)
- **AND** system validates stability at each level before further upgrade

### Requirement: State Persistence for Crash Recovery
The system SHALL serialize critical state to EEPROM/Flash to enable recovery after unexpected crashes, power loss, or watchdog resets.

#### Scenario: Persist state periodically
- **WHEN** system detects significant state change
- **THEN** system serializes state using TLV (Type-Length-Value) format
- **AND** state includes: active WiFi network index, connection statistics, health history, degradation mode
- **AND** system writes to EEPROM with CRC checksum
- **AND** system rate-limits writes to max 1 write per 60 seconds to prevent flash wear

#### Scenario: Detect crash on startup
- **WHEN** system initializes after reset
- **THEN** system reads reset reason from ESP32 hardware registers
- **AND** system identifies crash types: power-on reset, watchdog timeout, brownout, exception
- **AND** system loads persisted state from EEPROM if crash detected
- **AND** system validates state integrity using CRC

#### Scenario: Restore state after crash
- **WHEN** valid persisted state is available after crash
- **THEN** system restores WiFi network preference from state
- **AND** system restores connection pool configuration
- **AND** system restores degradation mode (or enters SAFE_MODE if crash was severe)
- **AND** system publishes crash recovery event with crash context

#### Scenario: Handle corrupted state
- **WHEN** persisted state CRC validation fails
- **THEN** system discards corrupted state data
- **AND** system performs clean initialization with default configuration
- **AND** system logs state corruption event for diagnostics

### Requirement: Self-Healing Mechanisms
The system SHALL automatically detect and recover from 95% of common failures within 60 seconds without manual intervention.

#### Scenario: Automatic WiFi reconnection
- **WHEN** WiFi connection is lost unexpectedly
- **THEN** system attempts reconnection using adaptive backoff strategy
- **AND** system tries all configured networks in priority order
- **AND** system succeeds in reconnecting within 30 seconds (90% of cases)
- **AND** system publishes recovery success event

#### Scenario: Automatic TCP reconnection
- **WHEN** TCP connection to server fails or becomes unresponsive
- **THEN** system closes stale connection gracefully
- **AND** system activates backup connection from pool if available
- **AND** system re-establishes new TCP connection within 10 seconds
- **AND** system resumes audio streaming with minimal data loss

#### Scenario: Automatic I2S recovery
- **WHEN** I2S read errors exceed threshold (5 errors per minute)
- **THEN** system reinitializes I2S driver
- **AND** system verifies microphone connection
- **AND** system resumes audio capture within 5 seconds
- **AND** system logs recovery action for diagnostics

#### Scenario: Memory pressure recovery
- **WHEN** free heap drops below warning threshold (40KB)
- **THEN** system triggers garbage collection
- **AND** system reduces buffer sizes to conserve memory
- **AND** system transitions to REDUCED_QUALITY mode
- **AND** system restores NORMAL mode when free heap >80KB for 60 seconds

### Requirement: Safe Mode Operation
The system SHALL provide a minimal-functionality safe mode that ensures basic operation even under severe failure conditions or resource constraints.

#### Scenario: Enter safe mode on repeated failures
- **WHEN** system experiences 3 consecutive failures in any mode
- **THEN** system immediately transitions to SAFE_MODE
- **AND** system disables all non-essential features (telemetry, predictions, health checks)
- **AND** audio streaming continues with basic error handling only
- **AND** system attempts recovery every 60 seconds

#### Scenario: Safe mode feature set
- **WHEN** operating in SAFE_MODE
- **THEN** system supports: audio streaming, basic WiFi connection, simple error logging
- **AND** system disables: predictive failure detection, circuit breakers, connection pool, telemetry
- **AND** system uses minimal RAM (~5KB for safe mode features)
- **AND** system accepts serial commands for diagnostics and manual recovery

#### Scenario: Exit safe mode after recovery
- **WHEN** in SAFE_MODE and system health improves to >70% for 120 seconds
- **THEN** system transitions to REDUCED_QUALITY mode
- **AND** system gradually re-enables reliability features
- **AND** system validates stability before transitioning to NORMAL mode
