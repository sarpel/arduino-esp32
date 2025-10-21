# ESP32 Audio Streamer - Operator's Guide

**Day-to-day operational guide for monitoring and maintaining the audio streaming system**

---

## Table of Contents

1. [Startup & Shutdown](#startup--shutdown)
2. [Daily Monitoring](#daily-monitoring)
3. [Health Check Procedures](#health-check-procedures)
4. [Responding to Alerts](#responding-to-alerts)
5. [Performance Optimization](#performance-optimization)
6. [Common Issues & Solutions](#common-issues--solutions)
7. [Emergency Procedures](#emergency-procedures)
8. [Maintenance Schedule](#maintenance-schedule)

---

## Startup & Shutdown

### Startup Sequence

1. **Power On Device**
   - Connect USB power or use external 5V supply
   - LED should blink (if equipped)
   - Serial output should appear

2. **Verify Startup**
   ```
   > Open serial monitor (9600 baud)
   > Watch for initialization messages
   > System should reach READY state in <10s
   ```

3. **Connect WiFi**
   ```
   > Wait for "WiFi connected" message
   > Device automatically connects to configured networks
   > If WiFi fails, check credentials and signal strength
   ```

4. **Verify Server Connection**
   ```
   > Look for "TCP connected to server" message
   > Audio streaming should begin automatically
   > Check METRICS command shows 0 errors
   ```

### Startup Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| No serial output | USB cable issue | Try different USB port |
| WiFi won't connect | Wrong credentials | Verify SSID and password |
| Server connection fails | Server down | Check server status |
| Audio not streaming | I2S issue | Verify hardware connections |

### Shutdown Procedure

1. **Graceful Shutdown**
   ```
   Send command: SHUTDOWN (if supported)
   Or: Simply disconnect power
   ```

2. **Recovery on Next Startup**
   - System restores previous state from EEPROM
   - Audio streaming resumes
   - Metrics preserved

### Power Loss Handling

- System automatically enters recovery mode on power restoration
- State persisted to non-volatile storage
- Up to 3 consecutive crashes trigger safe mode

---

## Daily Monitoring

### Morning Checklist

**Every morning, run these commands:**

```bash
HEALTH              # Check overall system health
NETWORK             # Verify WiFi and network status
MEMORY              # Check memory usage
METRICS             # Review uptime and error count
```

**Expected Results:**

| Check | Expected | Action if Not |
|-------|----------|--------------|
| HEALTH | Overall > 80% | Investigate component scores |
| NETWORK | Quality > 80%, CLOSED circuit | Review WiFi signal |
| MEMORY | Free > 100KB, Frag < 5% | Monitor memory usage |
| METRICS | Errors < 10, Availability > 99% | Review error log |

### Hourly Monitoring

**Every hour (optional for high-availability systems):**

```bash
TELEMETRY 20        # Check recent events
```

**Look for:**
- No ERROR or CRITICAL events
- If present, note timestamp and event type
- Check if system recovered automatically

### Real-Time Monitoring

**For active monitoring, use continuous mode:**

```bash
# Create monitoring script that runs:
# - HEALTH every 5 minutes
# - NETWORK every 10 minutes
# - METRICS every 30 minutes
```

### Performance Baseline

Record baseline metrics weekly:

| Metric | Week 1 | Week 2 | Week 3 | Trend |
|--------|--------|--------|--------|-------|
| Avg Latency (ms) | 12.5 | 12.8 | 13.2 | ↑ |
| Memory Free (KB) | 162 | 158 | 151 | ↓ |
| Error Count | 3 | 8 | 15 | ↑ Problem! |
| Availability (%) | 99.7 | 99.6 | 99.2 | ↓ |

---

## Health Check Procedures

### Weekly Health Audit

**Every Monday morning:**

1. **Full System Check**
   ```bash
   HEALTH
   NETWORK
   MEMORY
   METRICS
   EXPORT > audit_YYYY-MM-DD.json
   ```

2. **Review Audit Results**
   - Compare to previous week
   - Identify trends
   - Document anomalies

3. **Archive Logs**
   - Export telemetry events
   - Store for compliance
   - Backup to external storage

### Component Health Assessment

#### Network Health

Good RSSI values by location:
- Close to router (< 5m): -20 to -40 dBm
- Medium distance (5-15m): -40 to -70 dBm
- Far from router (> 15m): -70 to -90 dBm

```bash
NETWORK              # Check current RSSI
# If RSSI < -80, WiFi signal is weak
# Action: Move device closer or improve coverage
```

#### Memory Health

```bash
MEMORY               # Check memory usage

# Analysis:
# - Fragmentation > 10%: May need restart
# - Free < 50KB: Monitor closely
# - Free < 30KB: Stop audio, investigate leak
```

#### Audio Health

```bash
HEALTH               # Check Audio component score

# If Audio score dropping:
# - Check I2S connections
# - Verify no other USB devices interfering
# - Check microphone working
```

### Stress Test Procedure

**Run quarterly (or after updates):**

1. **24-Hour Stability Test**
   ```bash
   # Start test on Monday evening
   # Let system run for 24 hours without interruption
   # Monitor with TELEMETRY command hourly
   ```

2. **Analyze Results**
   - No ERROR or CRITICAL events
   - Memory stable (no growth)
   - No circuit breaker trips
   - Error count < 5 total

3. **Document Results**
   - Create test report
   - Compare to baseline
   - Note any changes

---

## Responding to Alerts

### WARNING Alert Response

**When you see a WARNING event:**

1. **Identify Issue**
   ```bash
   TELEMETRY 5         # See recent warnings
   HEALTH              # Check affected component
   ```

2. **Assess Severity**
   - Single event: Monitor, may be transient
   - Multiple events: Requires investigation
   - Increasing frequency: Fix needed

3. **Actions**

   **Network Quality Warning:**
   ```bash
   NETWORK             # Check quality score and RSSI
   # If < 50%: Move device closer to router
   # If multiple networks: Already failed over, check backup
   ```

   **Memory Usage Warning:**
   ```bash
   MEMORY              # Check free heap and fragmentation
   # If free < 80KB: Begin planning restart
   # If fragmentation > 15%: Restart recommended
   ```

   **Audio Warning:**
   ```bash
   HEALTH              # Check audio component score
   # If score dropping: Check I2S connections
   # If stable: May be temporary interference
   ```

4. **Escalate if Needed**
   - Still present after 10 minutes: Contact support
   - Multiple components affected: System restart needed
   - Recurring pattern: Investigate underlying cause

### ERROR Alert Response

**When you see an ERROR event:**

1. **Immediate Actions**
   ```bash
   TELEMETRY 10        # Get full error context
   NETWORK             # Check network status
   HEALTH              # Check system health
   ```

2. **Assessment Matrix**

   | Error Type | Impact | Action |
   |-----------|--------|--------|
   | WiFi connection failed | Temporary | Wait 30s, check auto-recovery |
   | TCP connection failed | Moderate | Check server status, may retry |
   | Memory allocation failed | Significant | Restart device |
   | I2S error | High | Check hardware, restart I2S |

3. **Recovery Steps**

   **For Network Errors:**
   ```bash
   # System should auto-recover in <60s
   # If not recovered after 2 minutes:
   # 1. Check server is running
   # 2. Verify WiFi credentials
   # 3. Consider manual failover command
   ```

   **For Memory Errors:**
   ```bash
   # Gracefully shutdown and restart
   # Monitor memory usage on restart
   # If repeats, investigate for memory leak
   ```

   **For Audio Errors:**
   ```bash
   # Check I2S connections
   # Restart device if needed
   # Verify microphone working
   ```

### CRITICAL Alert Response

**When you see a CRITICAL event:**

1. **Immediate Assessment**
   ```bash
   HEALTH              # Get overall system status
   METRICS             # Check availability
   TELEMETRY 20        # Get failure context
   ```

2. **Emergency Recovery**
   - Device enters RECOVERY mode automatically
   - Audio streaming pauses temporarily
   - System attempts self-healing (<60s)

3. **Manual Intervention**
   ```bash
   # If auto-recovery fails after 2 minutes:
   # 1. Restart device (power cycle)
   # 2. Check all hardware connections
   # 3. Review latest TELEMETRY events
   # 4. Contact technical support with EXPORT data
   ```

---

## Performance Optimization

### Tuning for Your Environment

#### Network Optimization

**If WiFi signal is weak:**
```cpp
// In config.h, increase health check frequency:
#define HEALTH_CHECK_INTERVAL_MS 5000    // More frequent checks

// Increase retry delays:
#define MAX_RETRY_DELAY 120000           // Longer timeout
```

**If network is very stable:**
```cpp
// Reduce overhead:
#define HEALTH_CHECK_INTERVAL_MS 20000   // Less frequent

// Faster recovery for good networks:
#define CIRCUIT_BREAKER_THRESHOLD 7      // Higher threshold
```

#### Memory Optimization

**If memory is tight:**
```cpp
// Reduce buffer sizes:
#define TELEMETRY_BUFFER_SIZE 512        // 512 bytes instead of 1KB

// Reduce event retention:
#define MAX_EVENTS_IN_BUFFER 25          // 25 events instead of 50
```

**If memory is plentiful:**
```cpp
// Increase buffer for better diagnostics:
#define TELEMETRY_BUFFER_SIZE 2048       // 2KB buffer

// Keep more metrics:
#define METRICS_HISTORY_SIZE 100         // Longer history
```

### Monitoring During Optimization

After any tuning changes:

1. **Verify Compilation**
   ```bash
   # Rebuild and deploy
   # Check compilation successful with no warnings
   ```

2. **Run Validation**
   ```bash
   # Wait 5 minutes
   HEALTH              # Should show healthy system
   METRICS             # Should show normal error rates
   ```

3. **Extended Monitoring**
   ```bash
   # Run for 24 hours
   # Check hourly with TELEMETRY command
   # Compare performance to baseline
   ```

---

## Common Issues & Solutions

### Issue: Health Score Slowly Decreasing

**Symptom**: Health 100% → 95% → 85% over hours

**Diagnosis**:
```bash
HEALTH              # Identify problem component
TELEMETRY 20        # Look for pattern in events
```

**Solutions by Component**:

**Network Score Degrading**:
- WiFi signal weakening: Move device closer
- Router dropping far connections: Reduce WiFi load
- Environmental interference: Check for other 2.4GHz devices

**Memory Score Degrading**:
- Fragmentation increasing: Restart device
- Heap usage growing: Investigate for memory leak
- Buffer pressure: Monitor allocation patterns

**Audio Score Degrading**:
- I2S errors increasing: Check microphone/connections
- Buffer underruns: Verify I2S clock stability
- Quality issues: Check audio hardware

**System Score Degrading**:
- Temperature increasing: Improve cooling
- CPU usage growing: Profile application
- Uptime low: Check for frequent errors

### Issue: Memory Leaks Suspected

**Symptoms**: Free memory decreases every hour

**Diagnostic Steps**:
```bash
# 1. Get baseline
MEMORY              # Note free heap, note time

# 2. Wait 1 hour
MEMORY              # Check if free heap decreased

# 3. Check growth rate
# Growth > 10KB/hour: Definite leak
# Growth 1-10KB/hour: Possible leak
# Growth < 1KB/hour: Normal variation
```

**Investigation**:
```bash
# Enable memory tracking
EXPORT | grep -i memory    # Detailed memory stats

# Monitor EventBus subscriptions - common leak source
# Check for malloc/free imbalance in recent code
# Profile with memory tests
```

**Resolution**:
- Code review recent changes
- Check object destructor calls
- Verify EventBus cleanup
- Run memory leak detection tests

### Issue: Frequent Network Failovers

**Symptoms**: System switches networks every 10-15 minutes

**Diagnosis**:
```bash
NETWORK             # Check quality score and RSSI
TELEMETRY 30        # Look for failover events
```

**Root Causes**:

| Cause | Indicator | Solution |
|-------|-----------|----------|
| Weak WiFi signal | RSSI < -80 | Move device or improve coverage |
| Interference | Quality drops suddenly | Reduce 2.4GHz interference |
| Router issue | Consistent pattern | Restart router, check logs |
| Timeout too aggressive | Frequent false fails | Increase CIRCUIT_BREAKER_TIMEOUT |

**Fixes**:
```cpp
// Increase fault tolerance
#define CIRCUIT_BREAKER_THRESHOLD 7      // More failures allowed

// Less sensitive quality detection
#define QUALITY_DEGRADED_THRESHOLD 50    // Higher threshold

// Allow longer recovery windows
#define CIRCUIT_BREAKER_TIMEOUT 60000    // 60 seconds
```

### Issue: Audio Cuts Out Randomly

**Symptoms**: Audio stops for 1-5 seconds, then resumes

**Diagnosis**:
```bash
HEALTH              # Check audio component score
TELEMETRY 10        # Look for I2S or buffer errors
NETWORK             # Check if correlated with network events
```

**Causes**:

| Cause | Indicator | Solution |
|-------|-----------|----------|
| Network failover | "Switched to network" in TELEMETRY | Improve WiFi stability |
| Buffer underrun | "I2S underrun" in events | Increase buffer size |
| CPU spike | Latency spike in METRICS | Reduce processing load |
| Memory shortage | Free < 80KB in MEMORY | Restart device |

---

## Emergency Procedures

### Device Unresponsive

**If device not responding to commands:**

1. **Check Serial Connection**
   - Verify USB cable connected
   - Try different USB port
   - Try different serial terminal software

2. **Soft Reset**
   - If any command gets response: Type `SHUTDOWN`
   - Wait 5 seconds
   - Power off and on

3. **Hard Reset** (if soft reset doesn't work)
   - Power off device
   - Wait 10 seconds
   - Power on device
   - Wait 30 seconds for full boot

4. **Recovery Mode**
   - If still unresponsive after hard reset
   - Device will enter recovery mode on startup
   - Automatic recovery should proceed

### Complete System Failure

**If nothing works:**

1. **Check Physical Issues**
   - All connections secure
   - No visible damage
   - Microphone functional (test separately)

2. **Serial Console Check**
   ```
   # At 9600 baud, you should see boot messages
   # If no output at all: Hardware issue likely
   ```

3. **Recovery Steps**
   - Power cycle 3 times (if crashes detected, enters safe mode)
   - Connect fresh USB power
   - Wait 2 minutes for full recovery

4. **If Still Failing**
   - Reflash firmware
   - Reset EEPROM
   - Factory reset (if available)

### Data Export for Support

**Before requesting support, export all diagnostics:**

```bash
EXPORT              # Generates JSON with all system data

# Copy JSON output and provide with support request
# Includes: health scores, metrics, events, hardware info
```

---

## Maintenance Schedule

### Daily Tasks

- [ ] Morning: Run HEALTH, NETWORK, METRICS checks
- [ ] Record any ERROR or CRITICAL events
- [ ] Check visually for any physical issues

### Weekly Tasks

- [ ] Monday morning: Full system audit
  - Run HEALTH, NETWORK, MEMORY, METRICS
  - Export and archive diagnostics
  - Review error trends

- [ ] Review telemetry for patterns
  - Look for recurring issues
  - Check if errors are trending up
  - Document any anomalies

### Monthly Tasks

- [ ] 1st of month: Performance baseline
  - Record METRICS values
  - Compare to previous month
  - Document any changes

- [ ] Mid-month: Extended monitoring
  - Run TELEMETRY-based health check
  - Identify any slow degradation
  - Plan preventive maintenance

- [ ] End of month: Report & planning
  - Create monthly operations report
  - Plan any needed optimizations
  - Schedule next quarter stress test

### Quarterly Tasks

- [ ] Run 24-hour stress test
- [ ] Update baseline metrics
- [ ] Review and optimize configuration
- [ ] Plan next quarter maintenance

### Annual Tasks

- [ ] Full system review
- [ ] Hardware inspection
- [ ] Performance benchmarking
- [ ] Plan upgrades if needed

---

## Support Contact Information

**Technical Support:**
- Email: support@example.com
- Reference: Include system ID and timestamps
- Attach: EXPORT output from EXPORT command

**Known Issues Registry:**
- Check project repository for known issues
- Search by error code or symptom
- Report new issues with full context

**Performance Reporting:**
- Submit metrics quarterly for analysis
- Help identify trends across deployments
- Contribute to system improvements

---

**For more information, see:**
- RELIABILITY_GUIDE.md - Technical details on reliability features
- TECHNICAL_REFERENCE.md - Complete system specifications
- README.md - Quick start guide
