# ESP32 Audio Streamer - Development Guide

**Complete technical reference for developers**

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Project Structure](#project-structure)
3. [System Architecture](#system-architecture)
4. [Configuration Reference](#configuration-reference)
5. [Development Workflow](#development-workflow)
6. [Testing & Debugging](#testing--debugging)
7. [OTA Updates](#ota-updates)
8. [Troubleshooting](#troubleshooting)

---

## Quick Start

### Prerequisites

- **PlatformIO** (CLI or IDE extension)
- **Python 3.7+** (for scripts)
- **Git** (for version control)
- **ESP32 Development Board** (ESP32-DevKit or Seeed XIAO ESP32-S3)
- **INMP441 I2S Microphone**

### 5-Minute Setup

```bash
# 1. Clone repository
git clone <repository-url>
cd arduino-esp32

# 2. Configure WiFi and server in src/config.h
# Edit: WIFI_SSID, WIFI_PASSWORD, SERVER_HOST, SERVER_PORT

# 3. Build firmware
pio run -e esp32dev

# 4. Upload to board
pio run -e esp32dev --target upload --upload-port COM8

# 5. Monitor serial output
pio device monitor --port COM8 --baud 115200
```

---

## Project Structure

```
arduino-esp32/
├── src/                          # Source code
│   ├── main.cpp                  # Main application loop with state machine
│   ├── config.h                  # Configuration parameters
│   ├── config_validator.h        # Runtime configuration validation
│   ├── network.cpp/h             # WiFi and TCP connection management
│   ├── i2s_audio.cpp/h           # I2S audio input handling
│   ├── logger.cpp/h              # Rate-limited logging system
│   ├── StateManager.cpp/h        # System state machine
│   ├── serial_command.cpp/h      # Serial command handler
│   ├── NonBlockingTimer.h        # Non-blocking timer utility
│   └── adaptive_buffer.cpp/h     # RSSI-based buffer optimization
├── tests/                        # Unit tests
├── scripts/                      # Build and utility scripts
│   └── report_build_size.py     # Flash/RAM usage reporting
├── platformio.ini                # PlatformIO configuration
├── README.md                     # User documentation
└── DEVELOPMENT.md                # This file

Key Modules:
- config.h: All configuration parameters (WiFi, I2S, thresholds)
- main.cpp: State machine and main loop
- network.cpp: TCP connection state machine and WiFi management
- i2s_audio.cpp: Audio capture with error recovery
- StateManager.h: System state transitions
- serial_command.cpp: Runtime control via serial commands
```

---

## System Architecture

### High-Level Design

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Main Application                    │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐   │
│  │       Main Loop (main.cpp - State Machine)           │   │
│  └──────────────────────────────────────────────────────┘   │
│    ↑                    ↑                      ↑             │
│    │                    │                      │             │
│  ┌─┴────────┐  ┌────────┴──────┐  ┌───────────┴────┐       │
│  │  I2S     │  │  WiFi/TCP     │  │  Serial Cmd    │       │
│  │  Audio   │  │  Network      │  │  Handler       │       │
│  └──────────┘  └───────────────┘  └────────────────┘       │
│       ↓               ↓                                      │
│    INMP441         TCP Server               Serial Port     │
│  Microphone       (Port 9000)                               │
└─────────────────────────────────────────────────────────────┘
```

### State Machine

```
INITIALIZING → CONNECTING_WIFI → CONNECTING_SERVER → CONNECTED
                      ↑                  ↑                ↓
                      └──────────────────┴────← ERROR ←──┘
```

**States:**
- **INITIALIZING**: System startup, hardware initialization
- **CONNECTING_WIFI**: WiFi connection with retry logic
- **CONNECTING_SERVER**: TCP server connection with exponential backoff
- **CONNECTED**: Active audio streaming
- **ERROR**: Error recovery, attempts reconnection
- **MAINTENANCE**: Reserved for OTA updates

---

## Configuration Reference

### WiFi Configuration (src/config.h)

```cpp
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#define WIFI_RETRY_DELAY 500        // ms between retries
#define WIFI_MAX_RETRIES 20         // Max retry attempts
#define WIFI_TIMEOUT 30000          // Total timeout (ms)
```

### Server Configuration

```cpp
#define SERVER_HOST "192.168.1.50"  // TCP server IP
#define SERVER_PORT 9000            // TCP server port
#define SERVER_RECONNECT_MIN 5000   // Min backoff (ms)
#define SERVER_RECONNECT_MAX 60000  // Max backoff (ms)
#define TCP_WRITE_TIMEOUT 5000      // Write timeout (ms)
#define TCP_CHUNK_SIZE 19200        // Bytes per write (must match server)
```

### I2S Audio Configuration

```cpp
#define I2S_SAMPLE_RATE 16000       // Hz (8kHz-48kHz supported)
#define I2S_BUFFER_SIZE 4096        // Bytes (power of 2 recommended)
#define I2S_DMA_BUF_COUNT 8         // DMA buffer count
#define I2S_DMA_BUF_LEN 256         // DMA buffer length
#define AUDIO_GAIN_NUMERATOR 3      // Input gain multiplier
#define AUDIO_GAIN_DENOMINATOR 2    // Input gain divisor (gain = 3/2 = 1.5x)
```

**Pin Assignments:**

**ESP32-DevKit:**
```cpp
#define I2S_SCK_PIN 26  // Serial Clock
#define I2S_WS_PIN 25   // Word Select
#define I2S_SD_PIN 34   // Serial Data
```

**Seeed XIAO ESP32-S3:**
```cpp
#define I2S_SCK_PIN 2
#define I2S_WS_PIN 3
#define I2S_SD_PIN 9
```

### Memory & Performance Thresholds

```cpp
#define MEMORY_WARN_THRESHOLD 40000     // bytes - warning level
#define MEMORY_CRITICAL_THRESHOLD 20000 // bytes - critical level
#define MEMORY_LEAK_DROP_THRESHOLD 2048 // bytes - leak detection threshold
#define RSSI_WEAK_THRESHOLD -80         // dBm - weak WiFi signal
#define MAX_CONSECUTIVE_FAILURES 10     // Max I2S/TCP failures before action
```

### Timing Configuration

```cpp
#define MEMORY_CHECK_INTERVAL 60000 // 1 minute
#define RSSI_CHECK_INTERVAL 10000   // 10 seconds
#define STATS_PRINT_INTERVAL 300000 // 5 minutes
```

### Watchdog Configuration

```cpp
#define WATCHDOG_TIMEOUT_SEC 60     // seconds - must exceed WiFi timeout
```

### Debug Configuration

```cpp
#define DEBUG_LEVEL 3  // 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE
```

---

## Development Workflow

### Building

```bash
# Clean build
pio run --target clean
pio run -e esp32dev

# Build with verbose output
pio run -e esp32dev -v

# Check flash/RAM usage
python scripts/report_build_size.py .pio/build/esp32dev/firmware.elf
```

### Uploading

```bash
# USB upload
pio run -e esp32dev --target upload --upload-port COM8

# OTA upload (after initial USB flash)
pio run -e esp32dev-ota --target upload
```

### Monitoring

```bash
# Basic monitoring
pio device monitor --port COM8 --baud 115200

# With ESP32 exception decoder
pio device monitor --port COM8 --baud 115200 --filter esp32_exception_decoder
```

### Serial Commands

**Runtime control via serial:**

```
HELP              - Show all available commands
STATUS            - Print current system state
STATS             - Print system statistics (uptime, bytes sent, memory, errors)
HEALTH            - Perform system health check
CONFIG SHOW       - Display configuration
CONNECT           - Attempt to connect to server
DISCONNECT        - Disconnect from server
RESTART           - Restart the ESP32
```

### Code Style

- **Indentation**: 4 spaces
- **Brace style**: K&R (opening brace on same line)
- **Naming**:
  - Classes: `PascalCase`
  - Functions: `camelCase`
  - Constants: `UPPER_SNAKE_CASE`
  - Variables: `snake_case`
- **Headers**: Always use include guards (`#ifndef`, `#define`, `#endif`)
- **Logging**: Use `LOG_*` macros (`LOG_INFO`, `LOG_ERROR`, etc.)

---

## Testing & Debugging

### Unit Testing

```bash
# Run all tests
pio test -e esp32dev

# Run specific test
pio test -e esp32dev -f test_network

# Verbose test output
pio test -e esp32dev -v
```

### Debug Logging

Adjust `DEBUG_LEVEL` in `src/config.h`:

```cpp
#define DEBUG_LEVEL 4  // Enable debug logs
```

Levels:
- `0` = OFF (production)
- `1` = ERROR only
- `2` = ERROR + WARN
- `3` = ERROR + WARN + INFO (default)
- `4` = ERROR + WARN + INFO + DEBUG
- `5` = VERBOSE (all logs)

### Common Issues & Solutions

**1. TCP Connection Fails**
- **Symptom**: `TCP write returned 0`
- **Cause**: Server not running or not listening
- **Fix**: Verify server is running on correct IP/port
  ```bash
  # On server machine:
  ss -tuln | grep 9000
  ```

**2. WiFi Won't Connect**
- **Symptom**: `WiFi connection timeout`
- **Cause**: Wrong credentials or 5GHz network
- **Fix**: Verify SSID/password, use 2.4GHz network only

**3. I2S Errors**
- **Symptom**: `I2S read failed after retries`
- **Cause**: Wrong pin configuration or faulty microphone
- **Fix**: Verify pin connections match config.h

**4. Memory Leak Warning**
- **Symptom**: `Memory trend: DECREASING (potential leak)`
- **Cause**: Memory not being freed properly
- **Fix**: Check for memory allocations without corresponding free()

**5. Watchdog Reset**
- **Symptom**: ESP32 restarts unexpectedly
- **Cause**: Operation taking longer than watchdog timeout
- **Fix**: Increase `WATCHDOG_TIMEOUT_SEC` or optimize blocking code

---

## OTA Updates

### Initial Setup

1. **Flash via USB first** (required for initial OTA setup)
2. **Verify WiFi connection** (OTA requires network)
3. **Note the IP address** from serial output

### OTA Upload

```bash
# Configure OTA environment in platformio.ini:
# upload_port = ESP32-AudioStreamer.local
# or
# upload_port = 192.168.1.19

# Upload via OTA
pio run -e esp32dev-ota --target upload
```

### OTA Configuration

In `src/main.cpp`, `setupOTA()` function:

```cpp
ArduinoOTA.setHostname("ESP32-AudioStreamer");
ArduinoOTA.setPort(3232);
// ArduinoOTA.setPassword("your_password");  // Optional security
```

### OTA Security

**Recommended for production:**
```cpp
ArduinoOTA.setPassword("SecurePassword123");
```

Update `platformio.ini`:
```ini
upload_flags =
    --auth=SecurePassword123
```

### OTA Troubleshooting

**"No OTA port found"**
- Ensure ESP32 is on same network
- Check firewall allows port 3232
- Verify hostname resolution: `ping ESP32-AudioStreamer.local`

**"Upload failed"**
- Check ESP32 has enough free flash space
- Verify WiFi signal strength is adequate
- Ensure no other OTA operation is in progress

---

## Troubleshooting

### Build Issues

**1. Missing Dependencies**
```bash
pio pkg install
pio lib install
```

**2. Compilation Errors**
```bash
# Clean and rebuild
pio run --target clean
pio run -e esp32dev
```

### Runtime Issues

**1. Connection Diagnostics**

Send `STATUS` command via serial to check:
- WiFi connection state
- TCP connection state
- Server IP/port
- Free memory

**2. Statistics Analysis**

Send `STATS` command to view:
- Uptime
- Total bytes sent
- WiFi/Server reconnect counts
- I2S/TCP error counts
- Memory usage trends

**3. Health Check**

Send `HEALTH` command to verify:
- WiFi signal strength
- TCP connection health
- Memory status
- I2S subsystem status

### Performance Monitoring

```bash
# Memory usage
python scripts/report_build_size.py .pio/build/esp32dev/firmware.elf

# Output:
# RAM:   [==        ]  16.3% (used 53352 bytes from 327680 bytes)
# Flash: [======    ]  63.1% (used 827061 bytes from 1310720 bytes)
```

### Log Analysis

**Key log patterns to watch:**

```
# Successful operation
[INFO] Server connection established
[INFO] Starting audio transmission

# Warning signs
[WARN] Memory low: 35000 bytes
[WARN] Weak WiFi signal: -82 dBm

# Critical errors
[ERROR] TCP write timeout
[CRITICAL] Memory critically low - initiating graceful restart
```

---

## Performance Metrics

### Expected Performance

- **Sample Rate**: 16 kHz
- **Bit Depth**: 16-bit
- **Channels**: Mono
- **Bitrate**: ~256 Kbps (32 KB/sec)
- **Chunk Size**: 19200 bytes (600ms of audio)
- **Latency**: <100ms (network dependent)

### Resource Usage

- **RAM**: ~53 KB (16.3% of 320 KB)
- **Flash**: ~827 KB (63.1% of 1.3 MB)
- **CPU**: ~15-25% (at 240 MHz)
- **WiFi**: 2.4 GHz only, WPA/WPA2

### Reliability Features

- ✅ WiFi auto-reconnect with exponential backoff
- ✅ TCP connection state machine with retry logic
- ✅ Transient vs permanent error classification
- ✅ Automatic I2S reinitialization on failure
- ✅ Memory leak detection via heap trending
- ✅ Hardware watchdog timer (60 seconds)
- ✅ Configuration validation at startup

---

## Additional Resources

### Documentation Files

- **README.md**: User-facing quick start and overview
- **DEVELOPMENT.md**: This comprehensive developer guide
- **AGENTS.md**: Repository guidelines and conventions
- **platformio.ini**: Build configuration and environments

### Useful Commands

```bash
# Device info
pio device list

# Library search
pio lib search ArduinoOTA

# Platform updates
pio platform update

# Clean everything
pio run --target cleanall
```

### External References

- [PlatformIO Documentation](https://docs.platformio.org)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [ArduinoOTA Reference](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html)

---

**Version**: 2.0
**Last Updated**: November 2025
**Status**: ✅ Production Ready
