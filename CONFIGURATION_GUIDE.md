# ESP32 Audio Streamer - Configuration Guide

## Quick Start Configuration

This guide explains all configuration options available in `src/config.h` and their recommended values for different scenarios.

---

## Essential Configuration (Required)

These settings **MUST** be configured before the system can start.

### WiFi Configuration

Edit `src/config.h`:

```cpp
#define WIFI_SSID       "YourWiFiNetwork"
#define WIFI_PASSWORD   "YourWiFiPassword"
```

**Important:**
- The system will not start if these are empty
- WiFi password is never logged to Serial
- Supports 2.4GHz networks only (standard ESP32 limitation)
- Password must be at least 8 characters for WPA2

**Example:**
```cpp
#define WIFI_SSID       "HomeNetwork"
#define WIFI_PASSWORD   "MySecurePassword123"
```

### Server Configuration

Edit `src/config.h`:

```cpp
#define SERVER_HOST     "192.168.1.100"
#define SERVER_PORT     9000
```

**Important:**
- HOST: IP address or domain name of your TCP server
- PORT: Must be a numeric value (not a string)
- The system will not start if these are empty
- Supports both IPv4 addresses and domain names

**Examples:**
```cpp
// Using IP address
#define SERVER_HOST     "192.168.1.50"
#define SERVER_PORT     9000

// Using domain name
#define SERVER_HOST     "audio.example.com"
#define SERVER_PORT     8080
```

---

## WiFi Connection Parameters

### Basic WiFi Settings

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| **WIFI_RETRY_DELAY** | 500 ms | 100-2000 ms | Delay between WiFi connection attempts |
| **WIFI_MAX_RETRIES** | 20 | 5-100 | Maximum WiFi retry attempts before giving up |
| **WIFI_TIMEOUT** | 30 sec | 10-60 sec | Timeout for overall WiFi connection attempt |

**Recommended Values:**

- **Stable Network**: 500ms delay, 20 retries, 30s timeout (DEFAULT - good for most cases)
- **Weak Signal**: 1000ms delay, 50 retries, 60s timeout (longer wait, more patient)
- **Fast Network**: 200ms delay, 10 retries, 15s timeout (quick fail, better for automation)

**Configuration Example:**
```cpp
#define WIFI_RETRY_DELAY 500    // Try every 500ms
#define WIFI_MAX_RETRIES 20     // Try up to 20 times
#define WIFI_TIMEOUT 30000      // Give up after 30 seconds
```

### Static IP Configuration (Optional)

If you want to use a static IP instead of DHCP:

```cpp
// Uncomment to enable static IP
#define USE_STATIC_IP

// Set your static configuration
#define STATIC_IP 192, 168, 1, 100
#define GATEWAY_IP 192, 168, 1, 1
#define SUBNET_MASK 255, 255, 255, 0
#define DNS_IP 8, 8, 8, 8
```

**When to Use:**
- ✅ Fixed network setup (same WiFi, same ESP32)
- ✅ Server needs to know ESP32's IP in advance
- ❌ Mobile/traveling setups (use DHCP instead)
- ❌ Networks with conflicting IP ranges

---

## Server Connection Parameters

### TCP Connection & Reconnection

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| **SERVER_RECONNECT_MIN** | 5 sec | 1-10 sec | Initial backoff delay |
| **SERVER_RECONNECT_MAX** | 60 sec | 30-120 sec | Maximum backoff delay |
| **TCP_WRITE_TIMEOUT** | 5 sec | 1-10 sec | Timeout for writing data to server |

**Backoff Strategy:**
The system uses exponential backoff to reconnect:
```
Attempt 1: Wait 5 sec
Attempt 2: Wait 10 sec
Attempt 3: Wait 20 sec
Attempt 4: Wait 40 sec
Attempt 5+: Wait 60 sec (max)
```

**Recommended Values:**

- **Local Server** (same network): 5s min, 30s max, 5s write timeout (fast recovery)
- **Remote Server** (internet): 10s min, 120s max, 10s write timeout (patient, robust)
- **Development**: 1s min, 10s max, 1s write timeout (quick iteration)

**Configuration Example:**
```cpp
#define SERVER_RECONNECT_MIN 5000      // Start with 5s delay
#define SERVER_RECONNECT_MAX 60000     // Cap at 60s delay
#define TCP_WRITE_TIMEOUT 5000         // Give data 5s to send
```

---

## I2S Audio Configuration

### Microphone Hardware Pins

**Auto-Detected by Board:**

```cpp
// ESP32-DevKit (default)
#define I2S_WS_PIN 15   // Word Select / LRCLK
#define I2S_SD_PIN 32   // Serial Data / DOUT
#define I2S_SCK_PIN 14  // Serial Clock / BCLK

// Seeed XIAO ESP32-S3
#define I2S_WS_PIN 3
#define I2S_SD_PIN 9
#define I2S_SCK_PIN 2
```

**Wiring for INMP441 Microphone:**

| INMP441 Pin | ESP32 Pin | Description |
|------------|-----------|-------------|
| VDD | 3.3V | Power |
| GND | GND | Ground |
| SCK (BCLK) | GPIO 14 | Bit Clock (ESP32-Dev) / GPIO 2 (XIAO) |
| WS (LRCLK) | GPIO 15 | Word Select (ESP32-Dev) / GPIO 3 (XIAO) |
| SD (DOUT) | GPIO 32 | Serial Data (ESP32-Dev) / GPIO 9 (XIAO) |
| L/R | GND | Left Channel (GND = use left only) |

### Audio Parameters

| Parameter | Default | Value | Description |
|-----------|---------|-------|-------------|
| **I2S_SAMPLE_RATE** | 16000 | 16 kHz | Audio sample rate (no modification recommended) |
| **I2S_BUFFER_SIZE** | 4096 | 4 KB | Size of audio data buffer |
| **I2S_DMA_BUF_COUNT** | 8 | count | Number of DMA buffers |
| **I2S_DMA_BUF_LEN** | 256 | samples | Length of each DMA buffer |
| **I2S_MAX_READ_RETRIES** | 3 | retries | Retry count for I2S read errors |

**Recommended:**
- Leave these at defaults unless you experience performance issues
- Larger buffers = more memory used but smoother streaming
- More DMA buffers = better protection against interrupts

**Advanced Users Only:**
```cpp
// For very low latency (reduce buffer)
#define I2S_BUFFER_SIZE 2048
#define I2S_DMA_BUF_COUNT 4

// For maximum stability (increase buffer)
#define I2S_BUFFER_SIZE 8192
#define I2S_DMA_BUF_COUNT 16
```

---

## Memory & System Thresholds

### Memory Management

| Parameter | Default | Description |
|-----------|---------|-------------|
| **MEMORY_WARN_THRESHOLD** | 40 KB | Alert if free heap drops below this |
| **MEMORY_CRITICAL_THRESHOLD** | 20 KB | Critical alert - prepare for restart |

**Recommended:**
- Warn: 40 KB (plenty of time to investigate)
- Critical: 20 KB (final warning before crash)
- Emergency: ~10 KB (auto-restart triggers)

```cpp
#define MEMORY_WARN_THRESHOLD 40000         // 40 KB warning
#define MEMORY_CRITICAL_THRESHOLD 20000     // 20 KB critical
```

### WiFi Signal Quality

| Parameter | Default | Description |
|-----------|---------|-------------|
| **RSSI_WEAK_THRESHOLD** | -80 dBm | Trigger WiFi reconnect if signal weaker |

**Signal Strength Reference:**
- **-30 dBm**: Excellent, very close to router
- **-50 dBm**: Very good, strong signal
- **-70 dBm**: Good, reasonable distance
- **-80 dBm**: Weak, far from router or obstacles
- **-90 dBm**: Very weak, barely connected

```cpp
#define RSSI_WEAK_THRESHOLD -80  // Reconnect if signal < -80 dBm
```

### Failure Tolerance

| Parameter | Default | Description |
|-----------|---------|-------------|
| **MAX_CONSECUTIVE_FAILURES** | 10 | Max failures before state reset |

---

## Timing & Monitoring

### System Check Intervals

| Parameter | Default | Recommended Range |
|-----------|---------|-------------------|
| **MEMORY_CHECK_INTERVAL** | 60 sec | 30-300 sec (1-5 min) |
| **RSSI_CHECK_INTERVAL** | 10 sec | 5-60 sec |
| **STATS_PRINT_INTERVAL** | 300 sec | 60-900 sec (1-15 min) |

**Meanings:**
- **Memory check**: How often to monitor heap (affects battery)
- **RSSI check**: How often to monitor WiFi signal strength
- **Stats print**: How often to output statistics to Serial

**Configuration Example:**
```cpp
#define MEMORY_CHECK_INTERVAL 60000   // Check memory every 1 minute
#define RSSI_CHECK_INTERVAL 10000    // Check WiFi signal every 10 sec
#define STATS_PRINT_INTERVAL 300000  // Print stats every 5 minutes
```

---

## System Initialization Timeouts

### Application-Specific Settings

| Parameter | Default | Description |
|-----------|---------|-------------|
| **SERIAL_INIT_DELAY** | 1000 ms | Delay after serial initialization |
| **GRACEFUL_SHUTDOWN_DELAY** | 100 ms | Delay between shutdown steps |
| **ERROR_RECOVERY_DELAY** | 5000 ms | Delay before error recovery attempt |
| **TASK_YIELD_DELAY** | 1 ms | Micro-delay in main loop for background tasks |

**Usually Leave at Defaults** - these are optimized for ESP32 and shouldn't need changes.

---

## Watchdog Configuration

| Parameter | Default | Notes |
|-----------|---------|-------|
| **WATCHDOG_TIMEOUT_SEC** | 10 sec | Hardware watchdog reset timeout |

**Important:**
- Must be longer than WIFI_TIMEOUT to avoid false resets during WiFi connection
- Must be longer than ERROR_RECOVERY_DELAY
- Validated automatically on startup

```
Validation checks:
✓ WATCHDOG_TIMEOUT (10s) > WIFI_TIMEOUT (30s) ? NO - WARNING
✓ WATCHDOG_TIMEOUT (10s) > ERROR_RECOVERY (5s) ? YES - OK
```

---

## TCP Keepalive (Advanced)

These settings help detect dead connections quickly:

| Parameter | Default | Description |
|-----------|---------|-------------|
| **TCP_KEEPALIVE_IDLE** | 5 sec | Time before sending keepalive probe |
| **TCP_KEEPALIVE_INTERVAL** | 5 sec | Interval between keepalive probes |
| **TCP_KEEPALIVE_COUNT** | 3 | Number of probes before giving up |

**Result**: Dead connection detected in ~5 + (5×3) = 20 seconds maximum.

```cpp
#define TCP_KEEPALIVE_IDLE 5        // Probe after 5 sec idle
#define TCP_KEEPALIVE_INTERVAL 5    // Probe every 5 sec
#define TCP_KEEPALIVE_COUNT 3       // Give up after 3 probes
```

---

## Scenario Configurations

### Scenario 1: Home/Lab Setup (Local Server)

```cpp
// WiFi Configuration
#define WIFI_SSID       "HomeNetwork"
#define WIFI_PASSWORD   "Password123"
#define WIFI_RETRY_DELAY 500
#define WIFI_MAX_RETRIES 20
#define WIFI_TIMEOUT 30000

// Server Configuration
#define SERVER_HOST     "192.168.1.100"
#define SERVER_PORT     9000
#define SERVER_RECONNECT_MIN 5000
#define SERVER_RECONNECT_MAX 30000
#define TCP_WRITE_TIMEOUT 5000

// Monitoring (frequent feedback)
#define MEMORY_CHECK_INTERVAL 30000
#define RSSI_CHECK_INTERVAL 10000
#define STATS_PRINT_INTERVAL 60000
```

### Scenario 2: Production/Remote Server

```cpp
// WiFi Configuration
#define WIFI_SSID       "CompanyNetwork"
#define WIFI_PASSWORD   "SecurePassword456"
#define WIFI_RETRY_DELAY 1000
#define WIFI_MAX_RETRIES 30
#define WIFI_TIMEOUT 60000

// Server Configuration
#define SERVER_HOST     "audio.company.com"
#define SERVER_PORT     443
#define SERVER_RECONNECT_MIN 10000
#define SERVER_RECONNECT_MAX 120000
#define TCP_WRITE_TIMEOUT 10000

// Monitoring (less frequent, save bandwidth)
#define MEMORY_CHECK_INTERVAL 120000
#define RSSI_CHECK_INTERVAL 30000
#define STATS_PRINT_INTERVAL 600000
```

### Scenario 3: Mobile/Unstable Network

```cpp
// WiFi Configuration (more patient)
#define WIFI_SSID       "MobileNetwork"
#define WIFI_PASSWORD   "Password789"
#define WIFI_RETRY_DELAY 2000      // Longer delay between attempts
#define WIFI_MAX_RETRIES 50        // More attempts
#define WIFI_TIMEOUT 90000         // Longer timeout

// Server Configuration (robust backoff)
#define SERVER_HOST     "remote-server.example.com"
#define SERVER_PORT     8080
#define SERVER_RECONNECT_MIN 15000 // Start at 15s
#define SERVER_RECONNECT_MAX 180000// Cap at 3 minutes
#define TCP_WRITE_TIMEOUT 15000

// Monitoring (alert on every issue)
#define MEMORY_CHECK_INTERVAL 30000
#define RSSI_CHECK_INTERVAL 5000
#define STATS_PRINT_INTERVAL 120000
```

---

## Configuration Validation

The system automatically validates all configuration on startup:

```
ESP32 Audio Streamer Starting Up
=== Starting Configuration Validation ===
Checking WiFi configuration...
  ✓ WiFi SSID configured
  ✓ WiFi password configured
Checking server configuration...
  ✓ Server HOST configured: 192.168.1.100
  ✓ Server PORT configured: 9000
Checking I2S configuration...
  ✓ I2S sample rate: 16000 Hz
  ✓ I2S buffer size: 4096 bytes
Checking watchdog configuration...
  ✓ Watchdog timeout: 10 seconds
✓ All configuration validations passed
=== Configuration Validation Complete ===
```

**If validation fails:**
```
Configuration validation failed - cannot start system
Please check config.h and fix the issues listed above
```

---

## Power Consumption Notes

### Factors Affecting Power Usage

| Setting | Higher Value | Impact |
|---------|-------------|--------|
| Sample Rate | 16 kHz | Fixed for 16 kHz audio |
| Buffer Size | Larger | More RAM used, better throughput |
| DMA Buffers | More | More overhead, smoother streaming |
| Check Intervals | Shorter | More CPU wakeups, higher drain |
| WiFi Retry | More attempts | Longer connection phase, higher drain |

### Estimated Power Consumption

- **Idle (not streaming)**: ~50 mA (WiFi on, no I2S)
- **WiFi connecting**: ~100-200 mA (varies with attempts)
- **Streaming (connected)**: ~70-100 mA (depends on WiFi signal)
- **Reconnecting**: ~150-300 mA (WiFi + retries)

**To Minimize Power:**
1. Increase check intervals (reduces CPU wakeups)
2. Decrease WiFi retry attempts (faster fail for bad networks)
3. Place ESP32 near router (better signal = less retransmits)

---

## Board-Specific Notes

### ESP32-DevKit

- Plenty of GPIO pins available
- Standard I2S pins: GPIO 14 (SCK), GPIO 15 (WS), GPIO 32 (SD)
- ~320 KB RAM available for buffers
- Good for prototyping and development

### Seeed XIAO ESP32-S3

- Compact form factor (much smaller)
- Different I2S pins: GPIO 2 (SCK), GPIO 3 (WS), GPIO 9 (SD)
- Built-in USB-C for programming
- ~512 KB RAM (more than standard ESP32)
- Good for embedded/portable applications

**No configuration needed** - auto-detected via board type in PlatformIO.

---

## Testing Your Configuration

After updating `config.h`:

1. **Rebuild**: `pio run`
2. **Upload**: `pio run --target upload`
3. **Monitor**: `pio device monitor --baud 115200`
4. **Watch for**:
   - ✓ "All configuration validations passed"
   - ✓ WiFi connection status
   - ✓ Server connection status
   - ✓ Audio data being transmitted

---

## Common Configuration Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| "WiFi SSID is empty" | CONFIG_VALIDATION failed | Add WiFi SSID to config.h |
| "Server PORT invalid" | SERVER_PORT is string, not number | Change `"9000"` to `9000` |
| "Watchdog may reset during WiFi" | WATCHDOG_TIMEOUT < WIFI_TIMEOUT | Increase WATCHDOG_TIMEOUT to >30s |
| "WiFi connects then disconnects" | Wrong password or router issue | Verify WIFI_PASSWORD, test phone connection |
| "Can't reach server" | Wrong SERVER_HOST or port | Verify host/port, test with `ping` |
| "Memory keeps decreasing" | Potential memory leak | Check I2S read/write error counts |
| "Very frequent reconnections" | Network unstable | Increase WIFI_RETRY_DELAY or check signal |

---

## See Also

- `src/config.h` - All configuration constants
- `ERROR_HANDLING.md` - Error states and recovery
- `README.md` - Quick start guide
- `TROUBLESHOOTING.md` - Problem-solving guide
