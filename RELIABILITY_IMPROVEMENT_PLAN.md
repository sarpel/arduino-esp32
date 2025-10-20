# Reliability Improvement Plan - ESP32 Audio Streamer v2.0

**Date**: October 20, 2025  
**Status**: PROPOSED - Awaiting Review  
**Focus**: Reliability, Crash Prevention, Bootloop Prevention

---

## Executive Summary

This document outlines critical reliability improvements for the ESP32 Audio Streamer v2.0 to prevent crashes, bootloops, and enhance system stability. All proposed changes focus on **increasing reliability without adding unnecessary complexity**.

**Key Principles:**
- ✅ Prevent crashes and bootloops
- ✅ Improve error recovery
- ✅ Enhance system monitoring
- ❌ No unnecessary feature additions
- ❌ No complexity for complexity's sake

---

## Current State Analysis

### Strengths ✅
- Configuration validation at startup
- Memory leak detection
- TCP connection state machine
- Error classification (transient/permanent/fatal)
- Serial command interface
- Comprehensive documentation
- Watchdog protection

### Identified Reliability Gaps ⚠️

1. **Bootloop Prevention**: No explicit bootloop detection
2. **Crash Recovery**: Limited crash dump/analysis
3. **Resource Exhaustion**: No proactive resource monitoring beyond memory
4. **Error Accumulation**: No circuit breaker pattern for repeated failures
5. **State Corruption**: No state validation/recovery mechanisms
6. **Hardware Failures**: Limited hardware fault detection (I2S, WiFi chip)

---

## Priority 1: Bootloop Prevention (CRITICAL)

### Problem
System can enter infinite restart loops if:
- Config validation fails repeatedly
- I2S initialization fails
- Critical resources unavailable
- Watchdog triggers repeatedly

### Solution: Bootloop Detection & Safe Mode

**Implementation:**
```cpp
// Add to config.h
#define MAX_BOOT_ATTEMPTS 3
#define BOOT_WINDOW_MS 60000  // 1 minute

// Track boots in RTC memory (survives rests)
RTC_DATA_ATTR uint32_t boot_count = 0;
RTC_DATA_ATTR unsigned long last_boot_time = 0;

// In setup()
void detectBootloop() {
    unsigned long current_time = millis();
    
    // Check if within boot window
    if (current_time - last_boot_time < BOOT_WINDOW_MS) {
        boot_count++;
    } else {
        boot_count = 1;
    }
    
    last_boot_time = current_time;
    
    // Bootloop detected - enter safe mode
    if (boot_count >= MAX_BOOT_ATTEMPTS) {
        LOG_CRITICAL("Bootloop detected! Entering safe mode...");
        enterSafeMode();
    }
}

void enterSafeMode() {
    // Minimal initialization - serial only
    // Skip WiFi, I2S, network
    // Allow serial commands to diagnose/fix
    // Reset boot counter after 5 minutes of stability
}
```

**Files to Modify:**
- `src/main.cpp` - Add bootloop detection
- `src/config.h` - Add bootloop constants
- `src/safe_mode.h` (NEW) - Safe mode implementation

**Testing:**
- Force 3 quick restarts - verify safe mode activation
- Verify recovery after stability period
- Test serial commands in safe mode

---

## Priority 2: Crash Dump & Recovery (HIGH)

### Problem
When system crashes (panic, exception), no diagnostic information is preserved for analysis.

### Solution: ESP32 Core Dump to Flash

**Implementation:**
```ini
# platformio.ini
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DCONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH
    -DCONFIG_ESP32_COREDUMP_DATA_FORMAT_ELF

# Reserve flash partition for coredump
```

**Usage:**
```bash
# After crash, retrieve dump
pio run --target coredump

# Analyze with ESP-IDF tools
python $IDF_PATH/components/espcoredump/espcoredump.py info_corefile coredump.bin
```

**Files to Modify:**
- `platformio.ini` - Enable coredump
- `src/main.cpp` - Add crash recovery handler
- Add `CRASH_ANALYSIS.md` documentation

**Testing:**
- Force crash (null pointer, stack overflow)
- Verify coredump is saved
- Analyze and verify useful information

---

## Priority 3: Circuit Breaker Pattern (HIGH)

### Problem
Repeated failures can cause resource exhaustion (e.g., rapid WiFi reconnections draining battery, repeated I2S failures causing watchdog)

### Solution: Circuit Breaker for Critical Operations

**Implementation:**
```cpp
// Add to config.h
#define CIRCUIT_BREAKER_FAILURE_THRESHOLD 5
#define CIRCUIT_BREAKER_TIMEOUT_MS 30000  // 30 seconds
#define CIRCUIT_BREAKER_HALF_OPEN_ATTEMPTS 1

enum CircuitState {
    CLOSED,      // Normal operation
    OPEN,        // Failures exceeded - stop trying
    HALF_OPEN    // Testing if service recovered
};

class CircuitBreaker {
private:
    CircuitState state = CLOSED;
    uint32_t failure_count = 0;
    unsigned long last_failure_time = 0;
    unsigned long circuit_open_time = 0;

public:
    bool shouldAttempt() {
        if (state == CLOSED) return true;
        
        if (state == OPEN) {
            // Check if timeout expired
            if (millis() - circuit_open_time > CIRCUIT_BREAKER_TIMEOUT_MS) {
                state = HALF_OPEN;
                failure_count = 0;
                return true;
            }
            return false;  // Circuit still open
        }
        
        // HALF_OPEN - allow limited attempts
        return failure_count < CIRCUIT_BREAKER_HALF_OPEN_ATTEMPTS;
    }
    
    void recordSuccess() {
        state = CLOSED;
        failure_count = 0;
    }
    
    void recordFailure() {
        failure_count++;
        last_failure_time = millis();
        
        if (state == HALF_OPEN) {
            // Failed during recovery - reopen circuit
            state = OPEN;
            circuit_open_time = millis();
            LOG_WARN("Circuit breaker reopened after failed recovery");
        } else if (failure_count >= CIRCUIT_BREAKER_FAILURE_THRESHOLD) {
            // Too many failures - open circuit
            state = OPEN;
            circuit_open_time = millis();
            LOG_ERROR("Circuit breaker OPEN - too many failures (%u)", failure_count);
        }
    }
};
```

**Apply to:**
- WiFi reconnection
- Server reconnection
- I2S reinitialization

**Files to Modify:**
- `src/circuit_breaker.h` (NEW)
- `src/network.cpp` - Apply to WiFi/TCP
- `src/i2s_audio.cpp` - Apply to I2S init

**Testing:**
- Force 5 quick WiFi failures - verify circuit opens
- Verify recovery after timeout
- Test under real network conditions

---

## Priority 4: State Validation & Recovery (MEDIUM)

### Problem
State corruption can occur if:
- WiFi reports connected but isn't
- TCP state doesn't match actual connection
- System state doesn't reflect reality

### Solution: Periodic State Validation

**Implementation:**
```cpp
// Add to main loop (every 10 seconds)
void validateSystemState() {
    // Validate WiFi state
    bool wifi_connected = WiFi.status() == WL_CONNECTED;
    bool state_says_wifi = NetworkManager::isWiFiConnected();
    
    if (wifi_connected != state_says_wifi) {
        LOG_ERROR("State corruption detected: WiFi actual=%d, state=%d", 
                  wifi_connected, state_says_wifi);
        // Force state sync
        if (!wifi_connected) {
            systemState.setState(SystemState::CONNECTING_WIFI);
        }
    }
    
    // Validate TCP state
    NetworkManager::validateConnection();  // Already implemented
    
    // Validate system resources
    validateResources();
}

void validateResources() {
    // Check task stack usage
    UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(NULL);
    if (stack_high_water < 512) {
        LOG_ERROR("Stack nearly exhausted: %u bytes remaining", stack_high_water);
    }
    
    // Check for blocked tasks (future: FreeRTOS task monitoring)
}
```

**Files to Modify:**
- `src/main.cpp` - Add state validation
- `src/state_validator.h` (NEW)

**Testing:**
- Force state mismatches
- Verify automatic recovery
- Monitor under load

---

## Priority 5: Proactive Resource Monitoring (MEDIUM)

### Problem
Only memory is monitored. Other resources can be exhausted:
- CPU usage
- Task stack space
- Network buffers
- Flash wear

### Solution: Comprehensive Resource Monitor

**Implementation:**
```cpp
class ResourceMonitor {
public:
    struct Resources {
        uint32_t free_heap;
        uint32_t largest_free_block;
        float cpu_usage_pct;
        uint32_t min_stack_remaining;
        uint32_t network_buffers_used;
    };
    
    static Resources measure() {
        Resources r;
        r.free_heap = ESP.getFreeHeap();
        r.largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
        r.cpu_usage_pct = measureCPU();
        r.min_stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        r.network_buffers_used = /* TCP buffer check */;
        return r;
    }
    
    static bool isHealthy(const Resources& r) {
        if (r.free_heap < MEMORY_CRITICAL_THRESHOLD) return false;
        if (r.largest_free_block < 1024) return false;  // Fragmentation
        if (r.cpu_usage_pct > 95.0) return false;
        if (r.min_stack_remaining < 512) return false;
        return true;
    }
};
```

**Files to Create:**
- `src/resource_monitor.h`
- `src/resource_monitor.cpp`

**Files to Modify:**
- `src/main.cpp` - Integrate resource monitoring

**Testing:**
- Stress test with high CPU load
- Monitor under various conditions
- Verify warnings trigger appropriately

---

## Priority 6: Hardware Fault Detection (MEDIUM)

### Problem
Hardware failures (I2S microphone, WiFi chip) aren't distinguished from software errors.

### Solution: Hardware Health Checks

**Implementation:**
```cpp
// I2S Hardware Check
bool checkI2SMicrophoneHardware() {
    // Read I2S status registers
    // Check for clock signals (if possible)
    // Verify DMA is functioning
    
    // Attempt small test read
    uint8_t test_buffer[64];
    size_t bytes_read;
    
    for (int i = 0; i < 3; i++) {
        if (i2s_read(I2S_PORT, test_buffer, sizeof(test_buffer), 
                     &bytes_read, pdMS_TO_TICKS(100)) == ESP_OK) {
            if (bytes_read > 0) {
                return true;  // Hardware responding
            }
        }
        delay(10);
    }
    
    LOG_ERROR("I2S hardware appears non-responsive");
    return false;
}

// WiFi Hardware Check
bool checkWiFiHardware() {
    // Check WiFi chip communication
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        LOG_ERROR("WiFi chip not responding");
        return false;
    }
    return true;
}
```

**Files to Modify:**
- `src/i2s_audio.cpp` - Add hardware checks
- `src/network.cpp` - Add WiFi hardware check
- `src/hardware_monitor.h` (NEW)

**Testing:**
- Test with disconnected microphone
- Test with disabled WiFi
- Verify appropriate error messages

---

## Priority 7: Graceful Degradation (LOW)

### Problem
System is all-or-nothing. Could continue partial operation if some features fail.

### Solution: Degraded Operation Modes

**Implementation:**
```cpp
enum OperationMode {
    FULL_OPERATION,       // All features working
    DEGRADED_NO_AUDIO,    // Network works, I2S failed
    DEGRADED_NO_NETWORK,  // I2S works, network failed  
    SAFE_MODE             // Minimal operation only
};

// Allow system to continue with reduced functionality
// E.g., if I2S fails but network works, accept remote commands
// If network fails but I2S works, log locally
```

**Files to Create:**
- `src/operation_mode.h`

**Files to Modify:**
- `src/main.cpp` - Support degraded modes

**Testing:**
- Disable I2S - verify network still works
- Disable network - verify I2S monitoring works
- Verify appropriate mode detection

---

## Implementation Roadmap

### Phase 1: Critical Reliability (Week 1)
- [ ] Bootloop detection and safe mode
- [ ] Circuit breaker pattern
- [ ] Crash dump configuration

### Phase 2: Enhanced Monitoring (Week 2)
- [ ] State validation
- [ ] Resource monitoring
- [ ] Hardware fault detection

### Phase 3: Graceful Degradation (Week 3)
- [ ] Operation modes
- [ ] Partial functionality support
- [ ] Extended testing

---

## Testing Strategy

### Unit Tests
- Bootloop detection logic
- Circuit breaker state transitions
- State validation routines

### Integration Tests
- Bootloop under real conditions
- Circuit breaker with real network failures
- Resource monitoring under load

### Stress Tests
- Continuous operation for 48+ hours
- Rapid restart cycles
- Resource exhaustion scenarios
- Hardware disconnect/reconnect

---

## Success Metrics

✅ **Zero bootloops** in 48-hour stress test  
✅ **Crash recovery** with actionable dump data  
✅ **Circuit breaker** prevents resource exhaustion  
✅ **State validation** catches and fixes corruption  
✅ **Resource monitoring** provides early warnings  
✅ **Hardware detection** identifies physical failures  

---

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| RTC memory data loss | Validate RTC data integrity on read |
| Safe mode prevents normal operation | Auto-exit after stability period |
| Circuit breaker too aggressive | Tunable thresholds via config |
| Performance overhead | Minimize checks, run only periodically |
| False positives | Comprehensive logging for debugging |

---

## Documentation Updates

- [ ] Update `ERROR_HANDLING.md` with new patterns
- [ ] Add `BOOTLOOP_PREVENTION.md`
- [ ] Update `TROUBLESHOOTING.md` with safe mode
- [ ] Document circuit breaker behavior
- [ ] Add crash dump analysis guide

---

## Next Steps

1. **Review this plan** - Validate approach and priorities
2. **Approve selected improvements** - Which to implement first?
3. **Create detailed tasks** - Break down into implementable chunks
4. **Implement Phase 1** - Start with critical reliability
5. **Test thoroughly** - Validate each improvement
6. **Deploy incrementally** - Roll out in stages

---

**Status**: 🟡 **AWAITING REVIEW**

Please review and provide feedback on:
1. Priority order - agree with critical items?
2. Scope - too much/too little?
3. Specific implementations - any concerns?
4. Timeline - realistic estimates?
