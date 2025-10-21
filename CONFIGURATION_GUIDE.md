# ESP32 Audio Streamer - Configuration Guide

**Complete reference for configuring reliability features and system behavior**

---

## Table of Contents

1. [Quick Start Configuration](#quick-start-configuration)
2. [Network Configuration](#network-configuration)
3. [Health Monitoring Configuration](#health-monitoring-configuration)
4. [Failure Recovery Configuration](#failure-recovery-configuration)
5. [Performance Tuning](#performance-tuning)
6. [Hardware-Specific Configuration](#hardware-specific-configuration)
7. [Advanced Configuration](#advanced-configuration)

---

## Quick Start Configuration

### Default Configuration (Out of Box)

The system comes with conservative defaults optimized for stability:

```cpp
// config.h - Default values
#define ENABLE_MULTI_WIFI 1
#define ENABLE_QUALITY_MONITORING 1
#define ENABLE_HEALTH_MONITORING 1
#define ENABLE_CIRCUIT_BREAKER 1
#define ENABLE_AUTO_RECOVERY 1
#define ENABLE_TELEMETRY 1

#define HEALTH_CHECK_INTERVAL_MS 10000    // 10 seconds
#define CIRCUIT_BREAKER_THRESHOLD 5
#define CIRCUIT_BREAKER_TIMEOUT 30000     // 30 seconds
#define MAX_RETRY_DELAY 60000             // 60 seconds
```

### Configuration for Different Scenarios

#### Scenario 1: Stable, High-Bandwidth Network

**Use case**: Server room, corporate office with good WiFi

```cpp
// More aggressive recovery (fewer false alarms)
#define CIRCUIT_BREAKER_THRESHOLD 7
#define CIRCUIT_BREAKER_TIMEOUT 60000

// Less frequent monitoring (stable environment)
#define HEALTH_CHECK_INTERVAL_MS 20000

// Faster reconnection (good signal)
#define MAX_RETRY_DELAY 30000
```

#### Scenario 2: Unreliable/Mobile Network

**Use case**: Moving vehicle, rural area, weak signal

```cpp
// More conservative (fail fast to backup)
#define CIRCUIT_BREAKER_THRESHOLD 3
#define CIRCUIT_BREAKER_TIMEOUT 15000

// More frequent monitoring (detect issues quickly)
#define HEALTH_CHECK_INTERVAL_MS 5000

// Slower reconnection (give network time to recover)
#define MAX_RETRY_DELAY 120000
```

#### Scenario 3: Memory-Constrained Device

**Use case**: ESP32 with minimal free RAM

```cpp
// Smaller buffers
#define TELEMETRY_BUFFER_SIZE 512        // 512 bytes
#define MAX_EVENTS_IN_BUFFER 25

// Disable non-critical features
#define ENABLE_PREDICTIVE_DETECTION 0
#define ENABLE_METRICS_TRACKING 0
```

#### Scenario 4: Ultra-Reliable Production

**Use case**: Critical infrastructure, 99.99% availability requirement

```cpp
// Paranoid monitoring
#define HEALTH_CHECK_INTERVAL_MS 5000
#define CIRCUIT_BREAKER_THRESHOLD 7

// Longer timeouts for proper recovery
#define CIRCUIT_BREAKER_TIMEOUT 120000

// Comprehensive logging
#define TELEMETRY_BUFFER_SIZE 2048
#define ENABLE_DETAILED_LOGGING 1
```

---

## Network Configuration

### Multiple WiFi Networks

#### Adding Networks

```cpp
// In src/config.h - Define credentials
#define WIFI_NETWORK_1_SSID "PrimaryNetwork"
#define WIFI_NETWORK_1_PASS "password1"
#define WIFI_NETWORK_1_PRIORITY 1

#define WIFI_NETWORK_2_SSID "BackupNetwork"
#define WIFI_NETWORK_2_PASS "password2"
#define WIFI_NETWORK_2_PRIORITY 2

#define WIFI_NETWORK_3_SSID "MobileHotspot"
#define WIFI_NETWORK_3_PASS "password3"
#define WIFI_NETWORK_3_PRIORITY 3
```

#### In main.cpp - Register Networks

```cpp
void setup_networks() {
    NetworkManager& net = SystemManager::getInstance().getNetworkManager();

    net.addWiFiNetwork(WIFI_NETWORK_1_SSID,
                      WIFI_NETWORK_1_PASS,
                      WIFI_NETWORK_1_PRIORITY,
                      true);  // auto-connect

    net.addWiFiNetwork(WIFI_NETWORK_2_SSID,
                      WIFI_NETWORK_2_PASS,
                      WIFI_NETWORK_2_PRIORITY,
                      true);

    net.addWiFiNetwork(WIFI_NETWORK_3_SSID,
                      WIFI_NETWORK_3_PASS,
                      WIFI_NETWORK_3_PRIORITY,
                      false);  // manual connect only
}
```

### TCP Server Configuration

```cpp
// Network target configuration
#define SERVER_IP "192.168.1.100"
#define SERVER_PORT 9000
#define TCP_TIMEOUT_MS 30000
#define KEEPALIVE_INTERVAL_MS 30000
```

### Quality Monitoring Thresholds

```cpp
// RSSI (Received Signal Strength Indicator)
#define RSSI_EXCELLENT -40           // dBm
#define RSSI_GOOD -60
#define RSSI_FAIR -70
#define RSSI_POOR -80
#define RSSI_CRITICAL -90

// Quality Score Thresholds
#define QUALITY_EXCELLENT 80         // %
#define QUALITY_GOOD 65
#define QUALITY_FAIR 50
#define QUALITY_POOR 40
#define QUALITY_CRITICAL 20
```

### Reconnection Strategy Configuration

```cpp
// Exponential backoff parameters
#define INITIAL_RETRY_DELAY 1000       // 1 second
#define MAX_RETRY_DELAY 60000          // 60 seconds
#define RETRY_BACKOFF_FACTOR 2.0       // Double each retry
#define RETRY_JITTER_PERCENT 25        // 25% random jitter

// Network success tracking
#define NETWORK_SUCCESS_HISTORY_HOURS 24    // 24-hour history
#define FAST_RETRY_NETWORK_SUCCESS_RATE 0.9 // 90% success = fast retry
```

---

## Health Monitoring Configuration

### Health Check Cycle

```cpp
// How often health is evaluated (milliseconds)
#define HEALTH_CHECK_INTERVAL_MS 10000  // 10 seconds

// Components included in health scoring
#define HEALTH_COMPONENTS_COUNT 4
#define NETWORK_HEALTH_WEIGHT 40        // %
#define MEMORY_HEALTH_WEIGHT 30
#define AUDIO_HEALTH_WEIGHT 20
#define SYSTEM_HEALTH_WEIGHT 10
```

### Component Health Thresholds

#### Network Component

```cpp
// Thresholds for network health calculation
#define NETWORK_RSSI_WEIGHT 40          // %
#define NETWORK_PACKET_LOSS_WEIGHT 30
#define NETWORK_LATENCY_WEIGHT 20
#define NETWORK_UPTIME_WEIGHT 10

// Penalty thresholds
#define NETWORK_RSSI_POOR_THRESHOLD -80 // dBm
#define NETWORK_LOSS_HIGH_THRESHOLD 0.05 // 5%
#define NETWORK_LATENCY_HIGH_THRESHOLD 100  // ms
```

#### Memory Component

```cpp
// Memory health calculation
#define MEMORY_FREE_WEIGHT 50           // %
#define MEMORY_FRAGMENTATION_WEIGHT 30
#define MEMORY_ALLOCATION_FAILURE_WEIGHT 20

// Penalty thresholds
#define MEMORY_FREE_CRITICAL 30720      // 30KB
#define MEMORY_FREE_WARNING 81920       // 80KB
#define MEMORY_FRAG_HIGH_THRESHOLD 0.15 // 15%
```

#### Audio Component

```cpp
// Audio health calculation
#define AUDIO_BUFFER_UNDERRUN_WEIGHT 50 // %
#define AUDIO_ERROR_COUNT_WEIGHT 30
#define AUDIO_I2S_HEALTH_WEIGHT 20

// Penalty thresholds
#define AUDIO_UNDERRUN_PENALTY_PERCENT 5 // 5% per underrun
```

#### System Component

```cpp
// System health calculation
#define SYSTEM_UPTIME_WEIGHT 40         // %
#define SYSTEM_TEMPERATURE_WEIGHT 30
#define SYSTEM_CPU_LOAD_WEIGHT 30

// Penalty thresholds
#define SYSTEM_TEMP_WARNING 70          // Celsius
#define SYSTEM_TEMP_CRITICAL 85         // Celsius
#define SYSTEM_CPU_LOAD_HIGH 80         // %
```

### Predictive Failure Detection

```cpp
// Trend analysis parameters
#define TREND_WINDOW_SECONDS 60         // 60-second history
#define ANOMALY_SIGMA_THRESHOLD 2.0     // 2 sigma for anomaly
#define PREDICTION_CONFIDENCE_MIN 0.7   // 70% minimum confidence

// Advance warning time
#define FAILURE_PREDICTION_LEAD_TIME 30000  // 30 seconds
```

---

## Failure Recovery Configuration

### Circuit Breaker Configuration

```cpp
// State machine parameters
#define CIRCUIT_BREAKER_THRESHOLD 5     // Failures before opening
#define CIRCUIT_BREAKER_TIMEOUT 30000   // ms before half-open
#define CIRCUIT_BREAKER_HALF_OPEN_LIMIT 1  // Requests in half-open

// Per-component thresholds
#define WIFI_CIRCUIT_BREAKER_THRESHOLD 5
#define TCP_CIRCUIT_BREAKER_THRESHOLD 5
#define I2S_CIRCUIT_BREAKER_THRESHOLD 5
```

### Degradation Mode Configuration

```cpp
// Mode transition thresholds
#define DEGRADATION_NORMAL_THRESHOLD 80    // Health %
#define DEGRADATION_REDUCED_THRESHOLD 60
#define DEGRADATION_SAFE_THRESHOLD 40
#define DEGRADATION_RECOVERY_THRESHOLD 20

// Hysteresis to prevent oscillation
#define DEGRADATION_HYSTERESIS_UP 10       // Health increase needed
#define DEGRADATION_HYSTERESIS_DOWN 5      // Health decrease allowed

// Mode definitions
enum DegradationMode {
    NORMAL = 0,           // All features
    REDUCED_QUALITY = 1,  // Audio enhancement disabled
    SAFE_MODE = 2,        // Critical functions only
    RECOVERY = 3          // Minimal operation
};

// Features per mode
#define FEATURE_AUDIO_ENHANCEMENT_NORMAL 1
#define FEATURE_AUDIO_ENHANCEMENT_REDUCED_QUALITY 0
#define FEATURE_AUDIO_ENHANCEMENT_SAFE_MODE 0

#define FEATURE_NETWORK_OPTIMIZATION_NORMAL 1
#define FEATURE_NETWORK_OPTIMIZATION_REDUCED_QUALITY 1
#define FEATURE_NETWORK_OPTIMIZATION_SAFE_MODE 0
```

### State Persistence Configuration

```cpp
// EEPROM/Flash parameters
#define STATE_STORAGE_MAX_WRITES_PER_MINUTE 1  // Rate limiting
#define STATE_STORAGE_CRC_CHECK 1              // Validate integrity
#define STATE_STORAGE_VERSION 1                // Format version

// Serialization format (TLV)
#define STATE_TLV_TYPE_MODE 1
#define STATE_TLV_TYPE_UPTIME 2
#define STATE_TLV_TYPE_ERROR_COUNT 3
#define STATE_TLV_TYPE_METRICS 4
```

### Auto Recovery Configuration

```cpp
// Recovery strategies
#define RECOVERY_STRATEGY_WIFI_RECONNECT 0
#define RECOVERY_STRATEGY_TCP_FAILOVER 1
#define RECOVERY_STRATEGY_I2S_REINIT 2
#define RECOVERY_STRATEGY_MEMORY_CLEANUP 3

// Recovery timeouts
#define RECOVERY_TIMEOUT_WIFI 60000     // 60 seconds
#define RECOVERY_TIMEOUT_TCP 30000      // 30 seconds
#define RECOVERY_TIMEOUT_I2S 10000      // 10 seconds
#define RECOVERY_TIMEOUT_MEMORY 5000    // 5 seconds
```

---

## Performance Tuning

### Memory Optimization

```cpp
// Telemetry buffer
#define TELEMETRY_BUFFER_SIZE 1024      // bytes
#define MAX_EVENTS_IN_BUFFER 50

// Metrics history
#define METRICS_HISTORY_SIZE 60         // samples (1 per second)
#define METRICS_HISTORY_HOURS 24        // retention

// Reduce for constrained devices
#define TELEMETRY_BUFFER_SIZE_MINIMAL 512
#define MAX_EVENTS_IN_BUFFER_MINIMAL 25
#define METRICS_HISTORY_SIZE_MINIMAL 30
```

### CPU Optimization

```cpp
// Health check frequency (less frequent = less CPU)
#define HEALTH_CHECK_INTERVAL_MS 10000  // 10 seconds

// Increase for lower CPU usage
#define HEALTH_CHECK_INTERVAL_MS_LOW_CPU 20000  // 20 seconds

// Trend analysis sampling
#define TREND_SAMPLE_INTERVAL_MS 1000   // 1 second samples
```

### I/O Optimization

```cpp
// Serial output rate limiting
#define SERIAL_THROTTLE_INTERVAL_MS 100 // Max 10 messages/sec

// EEPROM write rate limiting
#define EEPROM_WRITE_MIN_INTERVAL 60000 // 1 per minute

// Telemetry export batching
#define TELEMETRY_EXPORT_BATCH_SIZE 50  // Events per export
```

---

## Hardware-Specific Configuration

### ESP32-DevKit Configuration

```cpp
// Pin definitions
#define I2S_CLK_PIN 14
#define I2S_WS_PIN 15
#define I2S_SD_PIN 32

// I2S Configuration
#define I2S_SAMPLE_RATE 16000
#define I2S_BITS_PER_SAMPLE 16
#define I2S_CHANNEL_MONO 1

// Power constraints
#define MAX_WIFI_TX_POWER 20   // dBm
```

### Seeed XIAO ESP32-S3 Configuration

```cpp
// Pin definitions
#define I2S_CLK_PIN 2
#define I2S_WS_PIN 3
#define I2S_SD_PIN 9

// I2S Configuration
#define I2S_SAMPLE_RATE 16000
#define I2S_BITS_PER_SAMPLE 16

// Power constraints
#define MAX_WIFI_TX_POWER 15   // dBm (lower power device)
#define BATTERY_OPTIMIZATION 1  // Enable power saving
```

### Custom Hardware Configuration

```cpp
// Define your own pin configuration
#define I2S_CLK_PIN <your_pin>
#define I2S_WS_PIN <your_pin>
#define I2S_SD_PIN <your_pin>

// Memory allocation strategy
#define USE_MEMORY_POOL 1
#define MEMORY_POOL_SIZE 32768  // 32KB pool

// Serial configuration
#define SERIAL_BAUD_RATE 115200
#define SERIAL_TX_PIN 1
#define SERIAL_RX_PIN 3
```

---

## Advanced Configuration

### Custom Health Check

```cpp
// Extend health monitoring for custom component

class CustomHealthCheck : public HealthCheck {
public:
    uint8_t compute() override {
        // Your custom health computation
        // Return 0-100 score
        int custom_score = checkMyComponent();
        return custom_score;
    }

    const char* getName() const override {
        return "CustomComponent";
    }
};

// Register in system setup
void setup() {
    auto& health = SystemManager::getInstance().getHealthMonitor();
    health.registerHealthCheck(
        HealthComponent::CUSTOM,
        std::make_unique<CustomHealthCheck>()
    );
}
```

### Custom Recovery Strategy

```cpp
// Add custom failure recovery strategy

class CustomRecoveryStrategy : public RecoveryStrategy {
public:
    bool execute() override {
        // Your custom recovery implementation
        // Return true if successful
        return attemptCustomRecovery();
    }

    uint32_t getTimeout() override {
        return 15000;  // 15 second timeout
    }
};

// Register in system setup
void setup() {
    auto& recovery = SystemManager::getInstance().getAutoRecovery();
    recovery.registerStrategy(
        FailureType::CUSTOM,
        std::make_unique<CustomRecoveryStrategy>()
    );
}
```

### Event Bus Subscription

```cpp
// Subscribe to system events

class MyEventListener {
public:
    void onHealthDegraded(const HealthEvent& event) {
        // Handle health degradation
    }

    void onNetworkSwitched(const NetworkEvent& event) {
        // Handle network switch
    }
};

void setup() {
    MyEventListener listener;
    auto& bus = SystemManager::getInstance().getEventBus();

    bus.subscribe(EventType::HEALTH_DEGRADED, [&](const Event& evt) {
        listener.onHealthDegraded(dynamic_cast<const HealthEvent&>(evt));
    });
}
```

### Feature Flags

```cpp
// Enable/disable features via configuration

#define ENABLE_MULTI_WIFI 1
#define ENABLE_QUALITY_MONITORING 1
#define ENABLE_HEALTH_MONITORING 1
#define ENABLE_PREDICTIVE_DETECTION 1
#define ENABLE_CIRCUIT_BREAKER 1
#define ENABLE_DEGRADATION_MODES 1
#define ENABLE_STATE_PERSISTENCE 1
#define ENABLE_AUTO_RECOVERY 1
#define ENABLE_TELEMETRY 1
#define ENABLE_METRICS_TRACKING 1
#define ENABLE_DETAILED_LOGGING 0

// In code, conditionally include features
#if ENABLE_PREDICTIVE_DETECTION
    if (health.shouldWarnOfFailure()) {
        // Predictive failure handling
    }
#endif
```

---

## Configuration Validation

### Pre-Flight Checklist

Before deploying configuration changes:

```cpp
// Verify configuration consistency
static_assert(NETWORK_HEALTH_WEIGHT +
              MEMORY_HEALTH_WEIGHT +
              AUDIO_HEALTH_WEIGHT +
              SYSTEM_HEALTH_WEIGHT == 100,
              "Health weights must sum to 100%");

static_assert(DEGRADATION_NORMAL_THRESHOLD >
              DEGRADATION_REDUCED_THRESHOLD,
              "Thresholds must be in descending order");

static_assert(INITIAL_RETRY_DELAY < MAX_RETRY_DELAY,
              "Max retry must be greater than initial");
```

### Testing Configuration Changes

```cpp
// After changing configuration:

1. Verify compilation succeeds with no warnings
2. Run HEALTH command - should show 100% or close
3. Run METRICS command - verify no errors
4. Run TELEMETRY 5 - check for initialization events
5. Wait 5 minutes and verify stability
6. Run 1-hour test with TELEMETRY monitoring
7. Compare baseline metrics
```

---

## Factory Reset Configuration

### Reset to Defaults

```cpp
// In case of configuration issues, reset to known-good defaults
void resetToDefaults() {
    // Clear EEPROM
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();

    // Restart to apply defaults
    ESP.restart();
}
```

### Backup and Restore

```cpp
// Export configuration as JSON
void exportConfiguration(JsonDocument& doc) {
    doc["circuit_breaker_threshold"] = CIRCUIT_BREAKER_THRESHOLD;
    doc["health_check_interval"] = HEALTH_CHECK_INTERVAL_MS;
    doc["max_retry_delay"] = MAX_RETRY_DELAY;
    // ... all configuration values
}

// Restore from JSON
void restoreConfiguration(const JsonDocument& doc) {
    // Load values from JSON to runtime configuration
    // Re-initialize components
}
```

---

**For more information, see:**
- RELIABILITY_GUIDE.md - Feature details and usage
- OPERATOR_GUIDE.md - Day-to-day operations
- TECHNICAL_REFERENCE.md - System architecture
