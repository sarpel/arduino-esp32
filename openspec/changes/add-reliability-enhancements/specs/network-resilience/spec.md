# Network Resilience Specification

## ADDED Requirements

### Requirement: Multi-WiFi Network Support
The system SHALL support configuration of 2-5 WiFi networks with priority-based selection and automatic failover to maintain network connectivity.

#### Scenario: Configure multiple WiFi networks
- **WHEN** user configures 3 WiFi networks with priorities (1=highest, 3=lowest)
- **THEN** system stores credentials securely in priority order
- **AND** system attempts connection starting with highest priority network

#### Scenario: Automatic failover to backup network
- **WHEN** primary WiFi network connection fails or quality degrades below threshold
- **THEN** system automatically switches to next available network within 5 seconds
- **AND** audio streaming resumes without manual intervention

#### Scenario: Return to primary network when recovered
- **WHEN** backup network is active and primary network becomes available with good quality
- **THEN** system waits for stable connection (60s minimum on backup)
- **AND** system switches back to primary network during low-traffic period

### Requirement: Network Quality Monitoring
The system SHALL continuously monitor network quality metrics (RSSI, packet loss, RTT) and track quality trends for each configured WiFi network.

#### Scenario: Monitor WiFi signal strength
- **WHEN** connected to WiFi network
- **THEN** system measures RSSI (Received Signal Strength Indicator) every 10 seconds
- **AND** system computes exponential moving average of RSSI over 60 seconds
- **AND** system publishes quality degradation event when RSSI drops below -80 dBm

#### Scenario: Detect packet loss
- **WHEN** transmitting audio data over TCP connection
- **THEN** system tracks successful vs failed packet transmissions
- **AND** system computes packet loss percentage over 60-second window
- **AND** system triggers failover when packet loss exceeds 5% threshold

#### Scenario: Measure round-trip latency
- **WHEN** TCP connection is established
- **THEN** system measures round-trip time (RTT) using TCP keepalive probes
- **AND** system maintains sliding window of last 10 RTT measurements
- **AND** system publishes latency warning when RTT exceeds 200ms threshold

### Requirement: Connection Pool Management
The system SHALL maintain a pool of TCP connections including primary and backup connections to enable fast failover and connection redundancy.

#### Scenario: Establish backup connection
- **WHEN** primary TCP connection is stable for 30 seconds
- **THEN** system pre-establishes backup connection to server
- **AND** backup connection remains idle in keepalive mode
- **AND** backup connection is ready for immediate activation on primary failure

#### Scenario: Failover to backup connection
- **WHEN** primary TCP connection fails or becomes unresponsive
- **THEN** system activates backup connection within 1 second
- **AND** system resumes audio streaming on backup connection
- **AND** system attempts to re-establish new backup connection

#### Scenario: Connection pool health management
- **WHEN** connection in pool becomes stale (no activity for 120 seconds)
- **THEN** system sends keepalive probe to verify connection liveness
- **AND** system removes dead connections from pool
- **AND** system creates new connections to maintain pool size

### Requirement: Adaptive Reconnection Strategies
The system SHALL implement intelligent reconnection logic that adapts based on failure patterns, network conditions, and historical success rates.

#### Scenario: Exponential backoff for transient failures
- **WHEN** WiFi connection fails due to temporary issue (timeout, signal loss)
- **THEN** system uses exponential backoff starting at 5 seconds
- **AND** backoff doubles on each failure (5s, 10s, 20s, 40s) up to maximum 60 seconds
- **AND** system adds random jitter (±20%) to prevent synchronized reconnection storms

#### Scenario: Fast retry for known-good networks
- **WHEN** WiFi network has >90% success rate in last 24 hours
- **THEN** system uses shorter backoff intervals (3s, 6s, 12s)
- **AND** system increases retry attempts before failing over to backup network
- **AND** system maintains network success history for adaptive behavior

#### Scenario: Network quality-based strategy selection
- **WHEN** reconnecting after failure
- **THEN** system prioritizes networks with higher recent success rates
- **AND** system skips networks with RSSI below -85 dBm threshold
- **AND** system attempts stronger networks first regardless of configured priority

### Requirement: Seamless Network Switching
The system SHALL perform network transitions with minimal audio interruption, maintaining streaming continuity during WiFi network changes.

#### Scenario: Graceful connection migration
- **WHEN** switching from primary to backup WiFi network
- **THEN** system buffers audio data during transition (up to 2 seconds)
- **AND** system completes network switch within 5 seconds maximum
- **AND** audio streaming resumes with <500ms perceptible gap

#### Scenario: Preserve connection state during switch
- **WHEN** network switch is initiated
- **THEN** system preserves audio streaming offset and sequence numbers
- **AND** system serializes connection state to EEPROM
- **AND** system restores state after reconnection to minimize server synchronization

#### Scenario: Handle failed network switch
- **WHEN** failover to backup network fails
- **THEN** system attempts next available network in priority order
- **AND** system enters RECOVERY degradation mode after all networks fail
- **AND** system continues retry attempts with exponential backoff
