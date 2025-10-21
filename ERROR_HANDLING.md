# Error Handling & Recovery Strategy

## Overview

This document outlines all error states, recovery mechanisms, and watchdog behavior for the ESP32 Audio Streamer v2.0. It provides a comprehensive guide for understanding system behavior during failures and recovery scenarios.

---

## System States

```
INITIALIZING
    ↓
CONNECTING_WIFI ←→ ERROR (recovery)
    ↓
CONNECTING_SERVER ←→ ERROR (recovery)
    ↓
CONNECTED (streaming) ←→ ERROR (recovery)
    ↓
DISCONNECTED → CONNECTING_SERVER
    ↓
MAINTENANCE (reserved for future use)
```

### State Descriptions

| State | Purpose | Timeout | Actions |
|-------|---------|---------|---------|
| **INITIALIZING** | System startup, I2S/network init | N/A | Initialize hardware, validate config |
| **CONNECTING_WIFI** | Establish WiFi connection | 30 sec (WIFI_TIMEOUT) | Retry WiFi connection |
| **CONNECTING_SERVER** | Establish TCP server connection | Exponential backoff (5-60s) | Exponential backoff reconnection |
| **CONNECTED** | Active audio streaming | N/A | Read I2S → Write TCP, monitor links |
| **DISCONNECTED** | Server lost during streaming | N/A | Attempt server reconnection |
| **ERROR** | System error state | N/A | Log error, wait 5s, retry WiFi |
| **MAINTENANCE** | Reserved for firmware updates | N/A | Currently unused |

---

## Error Classification

### Critical Errors (System Restart)

These errors trigger immediate recovery actions or system restart:

#### 1. **Configuration Validation Failure**
- **Trigger**: ConfigValidator returns false at startup
- **Cause**: Missing WiFi SSID, SERVER_HOST, SERVER_PORT, or invalid thresholds
- **Recovery**: Halt system, wait for configuration fix, log continuously
- **Code**: `setup()` → Config validation loop
- **Log Level**: CRITICAL

#### 2. **I2S Initialization Failure**
- **Trigger**: I2SAudio::initialize() returns false
- **Cause**: Pin conflict, I2S driver error, hardware issue
- **Recovery**: Halt system in ERROR state, restart required
- **Code**: `setup()` → I2S init check
- **Log Level**: CRITICAL
- **Solution**: Check pin configuration, try different I2S port, restart ESP32

#### 3. **Critical Low Memory**
- **Trigger**: Free heap < MEMORY_CRITICAL_THRESHOLD/2 (~10KB)
- **Cause**: Memory leak, unbounded allocation
- **Recovery**: Graceful shutdown → ESP.restart()
- **Code**: `checkMemoryHealth()` in main loop
- **Log Level**: CRITICAL
- **Frequency**: Every 60 seconds (MEMORY_CHECK_INTERVAL)

#### 4. **Watchdog Timeout**
- **Trigger**: Watchdog timer expires (10 sec without reset)
- **Cause**: Infinite loop, blocking operation, or deadlock
- **Recovery**: Hardware reset by watchdog timer
- **Code**: `esp_task_wdt_reset()` in main loop (must be fed frequently)
- **Note**: Watchdog is fed in every loop iteration

### Non-Critical Errors (Recovery Attempt)

These errors trigger automatic recovery without system restart:

#### 1. **WiFi Connection Timeout**
- **Trigger**: WiFi not connected after WIFI_TIMEOUT (30 sec)
- **Cause**: Network unreachable, wrong SSID/password, router issue
- **Recovery**: Transition to ERROR state → 5 sec delay → retry WiFi
- **Code**: `loop()` → CONNECTING_WIFI case
- **Log Level**: ERROR
- **Retry**: Exponential backoff via NetworkManager

#### 2. **WiFi Connection Lost**
- **Trigger**: NetworkManager::isWiFiConnected() returns false
- **Cause**: Router rebooted, WiFi interference, signal loss
- **Recovery**: Transition to CONNECTING_WIFI state
- **Code**: `loop()` → state machine checks
- **Log Level**: WARN
- **Detection**: Checked every loop iteration (~1ms)

#### 3. **TCP Server Connection Failure**
- **Trigger**: NetworkManager::connectToServer() returns false
- **Cause**: Server down, wrong host/port, firewall blocking
- **Recovery**: Exponential backoff reconnection (5s → 60s)
- **Code**: `loop()` → CONNECTING_SERVER case
- **Log Level**: WARN (backoff) / ERROR (final timeout)
- **Backoff Formula**: `min_delay * (2^attempts - 1)` capped at max_delay

#### 4. **TCP Connection Lost During Streaming**
- **Trigger**: NetworkManager::isServerConnected() returns false
- **Cause**: Server closed connection, network disconnect, TCP timeout
- **Recovery**: Transition to CONNECTING_SERVER → exponential backoff
- **Code**: `loop()` → CONNECTED case verification
- **Log Level**: WARN

#### 5. **I2S Read Failure**
- **Trigger**: I2SAudio::readDataWithRetry() returns false
- **Cause**: I2S DMA underrun, buffer empty, transient error
- **Recovery**: Retry immediately (up to I2S_MAX_READ_RETRIES = 3)
- **Code**: `loop()` → CONNECTED case I2S read
- **Log Level**: ERROR (after all retries exhausted)
- **Metric**: Tracked in stats.i2s_errors

#### 6. **TCP Write Failure**
- **Trigger**: NetworkManager::writeData() returns false
- **Cause**: Socket error, connection broken, buffer full
- **Recovery**: Transition to CONNECTING_SERVER → reconnect
- **Code**: `loop()` → CONNECTED case write failure
- **Log Level**: WARN
- **Metric**: Tracked by NetworkManager error counters

#### 7. **Memory Low (Warning)**
- **Trigger**: Free heap < MEMORY_WARN_THRESHOLD (40KB)
- **Cause**: Memory fragmentation, slow leak
- **Recovery**: Log warning, monitor closely
- **Code**: `checkMemoryHealth()` in main loop
- **Log Level**: WARN
- **Frequency**: Every 60 seconds (MEMORY_CHECK_INTERVAL)
- **Next Action**: If gets worse → potential restart

#### 8. **WiFi Signal Weak**
- **Trigger**: WiFi RSSI < RSSI_WEAK_THRESHOLD (-80 dBm)
- **Cause**: Poor signal strength, distance from router
- **Recovery**: Preemptive disconnection → force WiFi reconnect
- **Code**: `NetworkManager::monitorWiFiQuality()`
- **Log Level**: WARN
- **Frequency**: Every 10 seconds (RSSI_CHECK_INTERVAL)

---

## Watchdog Timer

### Configuration

- **Timeout**: 10 seconds (WATCHDOG_TIMEOUT_SEC)
- **Location**: `esp_task_wdt_reset()` called in main loop
- **Feed Frequency**: Every loop iteration (~1ms)

### Watchdog Behavior

```
Loop starts
    ↓
esp_task_wdt_reset() ← Timer reset to 0
    ↓
WiFi handling
    ↓
State machine processing
    ↓
Loop ends (< 10 sec elapsed) → SUCCESS
    ↓
Repeat

If loop blocks for > 10 sec:
    ↓
Watchdog timer expires
    ↓
Hardware reset (ESP32 restarts)
```

### Why Watchdog Expires

1. **Infinite loop** in any function
2. **Long blocking operation** (delay > 10 sec)
3. **Deadlock** between components
4. **Task getting stuck** on I/O operation

### Watchdog Recovery

When watchdog expires:
1. ESP32 hardware reset automatically
2. `setup()` runs again
3. Config validation runs
4. System reinitializes
5. System enters CONNECTING_WIFI state

---

## Error Recovery Flows

### Recovery Flow 1: Configuration Error

```
Startup
    ↓
setup() runs
    ↓
ConfigValidator::validateAll()
    ↓
Validation FAILS (missing SSID/password/host)
    ↓
ERROR state
    ↓
Log CRITICAL every 5 seconds
    ↓
Await manual fix (update config.h)
    ↓
Restart ESP32 via button/command
    ↓
Validation passes
    ↓
Continue to I2S init
```

### Recovery Flow 2: WiFi Connection Lost

```
CONNECTED state (streaming)
    ↓
loop() calls NetworkManager::isWiFiConnected()
    ↓
Returns FALSE
    ↓
Transition to CONNECTING_WIFI
    ↓
Stop reading I2S
    ↓
Close server connection
    ↓
Loop → CONNECTING_WIFI state
    ↓
Attempt WiFi reconnect
    ↓
WiFi connects
    ↓
Transition to CONNECTING_SERVER
    ↓
Reconnect to server
    ↓
Transition to CONNECTED
    ↓
Resume streaming
```

### Recovery Flow 3: Server Connection Lost

```
CONNECTED state (streaming)
    ↓
loop() calls NetworkManager::isServerConnected()
    ↓
Returns FALSE
    ↓
Transition to CONNECTING_SERVER
    ↓
NetworkManager applies exponential backoff
    ↓
First attempt: wait 5s
    ↓
Second attempt: wait 10s
    ↓
Third attempt: wait 20s
    ↓
... up to 60s maximum
    ↓
Server connection succeeds
    ↓
Transition to CONNECTED
    ↓
Resume streaming
```

### Recovery Flow 4: I2S Read Failure

```
CONNECTED state (streaming)
    ↓
loop() calls I2SAudio::readDataWithRetry()
    ↓
Read attempt 1 FAILS
    ↓
Retry 2 FAILS
    ↓
Retry 3 FAILS
    ↓
readDataWithRetry() returns FALSE
    ↓
Increment stats.i2s_errors
    ↓
Log ERROR
    ↓
Continue in CONNECTED (don't disrupt server connection)
    ↓
Next loop iteration attempts read again
```

### Recovery Flow 5: Critical Memory Low

```
loop() executing
    ↓
checkMemoryHealth() called
    ↓
Free heap < MEMORY_CRITICAL_THRESHOLD/2 (~10KB)
    ↓
Log CRITICAL
    ↓
Call gracefulShutdown()
    ↓
  - Print stats
    ↓
  - Close server connection
    ↓
  - Stop I2S audio
    ↓
  - Disconnect WiFi
    ↓
ESP.restart()
    ↓
setup() runs again
    ↓
System reinitializes
```

---

## Error Metrics & Tracking

### Statistics Collected

| Metric | Updated | Tracked In |
|--------|---------|-----------|
| **Total bytes sent** | Every successful write | stats.total_bytes_sent |
| **I2S errors** | I2S read failure | stats.i2s_errors |
| **WiFi reconnects** | WiFi disconnection | NetworkManager::wifi_reconnect_count |
| **Server reconnects** | Server disconnection | NetworkManager::server_reconnect_count |
| **TCP errors** | TCP write/read failure | NetworkManager::tcp_error_count |
| **Uptime** | Calculated | stats.uptime_start |
| **Free heap** | Every stats print | ESP.getFreeHeap() |

### Statistics Output

```
=== System Statistics ===
Uptime: 3600 seconds (1.0 hours)
Data sent: 1048576 bytes (1.00 MB)
WiFi reconnects: 2
Server reconnects: 1
I2S errors: 0
TCP errors: 0
Free heap: 65536 bytes
========================
```

Printed every 5 minutes (STATS_PRINT_INTERVAL).

---

## Threshold Values & Configuration

### Memory Thresholds

| Threshold | Value | Action |
|-----------|-------|--------|
| MEMORY_WARN_THRESHOLD | 40,000 bytes | Log WARN, continue monitoring |
| MEMORY_CRITICAL_THRESHOLD | 20,000 bytes | Log CRITICAL, consider restart |
| Critical Emergency | < 10,000 bytes | Graceful shutdown → restart |

### WiFi Thresholds

| Parameter | Value | Notes |
|-----------|-------|-------|
| WIFI_TIMEOUT | 30,000 ms | Abort WiFi connection if takes > 30s |
| WIFI_RETRY_DELAY | 500 ms | Delay between retry attempts |
| WIFI_MAX_RETRIES | 20 | Max retry count |
| RSSI_WEAK_THRESHOLD | -80 dBm | Force reconnect if signal weaker |

### Server Reconnection Backoff

| Attempt | Backoff Wait | Cumulative Time |
|---------|-------------|-----------------|
| 1 | 5 sec | 5 sec |
| 2 | 10 sec | 15 sec |
| 3 | 20 sec | 35 sec |
| 4 | 40 sec | 75 sec |
| 5+ | 60 sec (max) | +60 sec per attempt |

Formula: `min(5s * (2^attempts - 1), 60s)`

---

## Logging Levels

### Log Level Hierarchy

```
CRITICAL: System critical error requiring immediate attention
ERROR: System error, recovery in progress or failed
WARN: Warning condition, system operational but degraded
INFO: Informational message, normal operation
(DEBUG: Detailed debug info - compile-time disabled)
```

### Error Log Examples

```
[CRITICAL] Configuration validation failed - WiFi SSID is empty
[CRITICAL] I2S initialization failed - cannot continue
[CRITICAL] Critical low memory: 8192 bytes - system may crash
[ERROR] WiFi connection timeout
[ERROR] I2S read failed after retries
[WARN] Memory low: 35000 bytes
[WARN] WiFi lost during streaming
[WARN] WiFi signal weak: -85 dBm
[WARN] Data transmission failed
[INFO] State transition: CONNECTING_WIFI → CONNECTED
[INFO] WiFi connected - IP: 192.168.1.100
[INFO] === System Statistics ===
```

---

## Debugging Tips

### Reading Error Logs

1. **Look for CRITICAL messages first** - indicate system halt conditions
2. **Check state transitions** - show what was happening when error occurred
3. **Count ERROR/WARN messages** - frequency indicates stability issues
4. **Monitor stats** - identify patterns (e.g., increasing error counts)

### Common Issues & Solutions

| Issue | Indicator | Solution |
|-------|-----------|----------|
| WiFi connects then disconnects | Frequent "WiFi lost" messages | Check WiFi password, signal strength, router stability |
| Server never connects | "CONNECTING_SERVER" state, increasing backoff | Check SERVER_HOST and SERVER_PORT in config |
| I2S read errors | i2s_errors counter increasing | Check INMP441 wiring, I2S pin configuration |
| Memory keeps decreasing | Free heap trending down | Potential memory leak, restart system |
| Watchdog resets frequently | System restarts every ~10 seconds | Find blocking code, add yield delays |
| Very high WiFi reconnects | Counter > 10 in short time | WiFi interference, router issue, move closer |

### Enable Debug Output

Edit `src/logger.h` to enable DEBUG level:

```cpp
#define LOG_DEBUG(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
```

Recompile and reupload for detailed debug messages.

---

## Future Enhancements

1. **RTC Memory Tracking** - Record restart causes in RTC memory for persistence across reboots
2. **Telemetry System** - Send error statistics to cloud for analysis
3. **Adaptive Recovery** - Adjust backoff timings based on error patterns
4. **Self-Healing** - Automatically adjust parameters based on recurring errors
5. **OTA Updates** - Update code remotely to fix known issues

---

## See Also

- `src/config.h` - Configuration constants and thresholds
- `src/config_validator.h` - Configuration validation logic
- `src/StateManager.h` - State machine implementation
- `src/logger.h` - Logging macros and levels
- `src/main.cpp` - Error handling in main loop
