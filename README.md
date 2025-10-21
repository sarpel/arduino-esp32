# ESP32 Audio Streamer v2.0

**Professional-grade I2S audio streaming system for ESP32 with comprehensive reliability features.**

[![Build Status](https://img.shields.io/badge/build-SUCCESS-brightgreen)](README.md)
[![RAM Usage](https://img.shields.io/badge/RAM-15.0%25-blue)](README.md)
[![Flash Usage](https://img.shields.io/badge/Flash-59.3%25-blue)](README.md)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## Overview

The ESP32 Audio Streamer is a robust, production-ready system for streaming high-quality audio from an INMP441 I2S microphone to a TCP server over WiFi. Designed with reliability, error recovery, and adaptive optimization in mind.

**Key Characteristics:**
- 16kHz, 16-bit mono audio acquisition
- ~256 Kbps streaming rate (32 KB/sec)
- Automatic error detection and recovery
- Adaptive resource management based on signal strength
- Real-time system monitoring and control via serial commands
- Comprehensive diagnostics and troubleshooting

---

## Features

### 🎯 Core Functionality
✅ **I2S Audio Acquisition** - Digital audio input from INMP441 microphone
✅ **WiFi Connectivity** - Automatic reconnection with exponential backoff
✅ **TCP Streaming** - Reliable data transmission to remote server
✅ **State Machine** - Explicit system state management with clear transitions

### 🛡️ Reliability Features
✅ **Configuration Validation** - Startup verification of all critical parameters
✅ **Error Classification** - Intelligent error categorization (transient/permanent/fatal)
✅ **Memory Leak Detection** - Automatic heap trend monitoring
✅ **TCP State Machine** - Explicit connection state tracking with validation
✅ **Health Checks** - Real-time system health scoring

### 🚀 Performance Optimization
✅ **Adaptive Buffering** - Dynamic buffer sizing based on WiFi signal strength
✅ **Exponential Backoff** - Intelligent retry strategy for connection failures
✅ **Watchdog Timer** - Hardware watchdog with timeout validation
✅ **Non-blocking Operations** - Responsive main loop with configurable task yielding

### 🔧 System Control & Monitoring
✅ **Serial Command Interface** - 8 runtime commands for system control
✅ **Real-time Statistics** - Comprehensive system metrics every 5 minutes
✅ **Health Monitoring** - Visual status indicators and diagnostic output
✅ **Debug Modes** - 6 configurable debug levels (production to verbose)

---

## Quick Start

### Requirements
- **Hardware**: ESP32-DevKit or Seeed XIAO ESP32-S3
- **Microphone**: INMP441 I2S digital microphone
- **Tools**: PlatformIO IDE or CLI
- **Server**: TCP server listening on configured host:port

### Installation

1. **Clone or download the project**
   ```bash
   cd arduino-esp32
   ```

2. **Configure WiFi and Server** - Edit `src/config.h`:
   ```cpp
   #define WIFI_SSID "YourWiFiNetwork"
   #define WIFI_PASSWORD "YourPassword"
   #define SERVER_HOST "192.168.1.100"
   #define SERVER_PORT 9000
   ```

3. **Build the project**
   ```bash
   pio run
   ```

4. **Upload to ESP32**
   ```bash
   pio run --target upload
   ```

5. **Monitor output**
   ```bash
   pio device monitor --baud 115200
   ```

---

## Hardware Setup

### Pinout - ESP32-DevKit

| Signal | Pin | Description |
|--------|-----|-------------|
| I2S_WS | GPIO15 | Word Select / LRCLK |
| I2S_SD | GPIO32 | Serial Data (microphone input) |
| I2S_SCK | GPIO14 | Serial Clock / BCLK |
| GND | GND | Ground |
| 3V3 | 3V3 | Power supply |

### Pinout - Seeed XIAO ESP32-S3

| Signal | Pin | Description |
|--------|-----|-------------|
| I2S_WS | GPIO3 | Word Select / LRCLK |
| I2S_SD | GPIO9 | Serial Data (microphone input) |
| I2S_SCK | GPIO2 | Serial Clock / BCLK |
| GND | GND | Ground |
| 3V3 | 3V3 | Power supply |

---

## Serial Commands

Access the system via serial terminal (115200 baud):

| Command | Function |
|---------|----------|
| `STATUS` | Show WiFi, TCP, memory, and system state |
| `STATS` | Display detailed system statistics |
| `HEALTH` | Perform system health check with indicators |
| `CONFIG SHOW` | Display all configuration parameters |
| `CONNECT` | Manually attempt to connect to server |
| `DISCONNECT` | Manually disconnect from server |
| `RESTART` | Restart the system |
| `HELP` | Show all available commands |

Example output:
```
> STATUS
WiFi: CONNECTED (192.168.1.100)
WiFi Signal: -65 dBm
TCP State: CONNECTED
Server: audio.server.com:9000
System State: CONNECTED
Free Memory: 65536 bytes
WiFi Reconnects: 2
Server Reconnects: 1
TCP Errors: 0
```

---

## Configuration

### Essential Parameters (`src/config.h`)

```cpp
// WiFi Configuration
#define WIFI_SSID ""                    // Your WiFi network
#define WIFI_PASSWORD ""                // Your WiFi password

// Server Configuration
#define SERVER_HOST ""                  // Server IP or hostname
#define SERVER_PORT 0                   // Server port (1-65535)

// Audio Configuration
#define I2S_SAMPLE_RATE 16000           // Audio sample rate (Hz)
#define I2S_BUFFER_SIZE 4096            // Buffer size (bytes)

// Memory Thresholds
#define MEMORY_WARN_THRESHOLD 40000     // Low memory warning (bytes)
#define MEMORY_CRITICAL_THRESHOLD 20000 // Critical level (bytes)

// Debug Configuration
#define DEBUG_LEVEL 3                   // 0=OFF, 3=INFO, 5=VERBOSE
```

See `CONFIGURATION_GUIDE.md` for detailed parameter descriptions.

---

## System Architecture

### State Machine

```
INITIALIZING
    ↓
CONNECTING_WIFI
    ↓ (success)
CONNECTING_SERVER
    ↓ (success)
CONNECTED ← main streaming state
    ↓ (WiFi lost or connection error)
ERROR
    ↓ (recovery attempt)
CONNECTING_WIFI (retry)
```

### Key Components

| Component | Purpose |
|-----------|---------|
| `I2SAudio` | I2S audio acquisition with error classification |
| `NetworkManager` | WiFi and TCP with state machine |
| `ConfigValidator` | Startup configuration validation |
| `SerialCommandHandler` | Real-time serial commands |
| `AdaptiveBuffer` | Dynamic buffer sizing by signal strength |
| `RuntimeDebugContext` | Runtime-configurable debug output |

---

## Performance Metrics

### Build Profile
```
RAM:   15.0% (49,224 / 327,680 bytes)
Flash: 59.3% (777,461 / 1,310,720 bytes)
Build time: ~6 seconds
Warnings: 0 | Errors: 0
```

### Audio Format
- **Sample Rate**: 16 kHz
- **Bit Depth**: 16-bit
- **Channels**: Mono (left channel)
- **Format**: Raw PCM, little-endian
- **Bitrate**: ~256 Kbps (32 KB/sec)

---

## Documentation

| Document | Purpose |
|----------|---------|
| `CONFIGURATION_GUIDE.md` | All 40+ parameters with recommended values |
| `TROUBLESHOOTING.md` | Solutions for 30+ common issues |
| `ERROR_HANDLING.md` | Error classification and recovery flows |
| `IMPLEMENTATION_SUMMARY.md` | Phase 1 implementation details |
| `PHASE2_IMPLEMENTATION_COMPLETE.md` | Phase 2: I2S error handling |
| `PHASE3_IMPLEMENTATION_COMPLETE.md` | Phase 3: TCP state machine, commands, debug, buffer, tests |
| `test_framework.md` | Unit testing architecture |

---

## Testing

### Build Verification
```bash
pio run                    # Build for ESP32-Dev
pio run -e seeed_xiao_esp32s3  # Build for XIAO S3
```

### Unit Tests
```bash
pio test                   # Run all tests
pio test -f test_adaptive_buffer  # Run specific test
```

---

## Troubleshooting

### Common Issues

**"WiFi connection timeout"**
- Check WIFI_SSID and WIFI_PASSWORD
- Verify WiFi signal strength
- See `TROUBLESHOOTING.md` for detailed steps

**"Server connection failed"**
- Verify SERVER_HOST and SERVER_PORT
- Check firewall settings
- Ensure server is running
- See `TROUBLESHOOTING.md`

**"I2S read errors"**
- Verify microphone wiring
- Check I2S pins for conflicts
- Verify power supply stability
- See `TROUBLESHOOTING.md`

**"Memory low warnings"**
- Monitor via `STATS` command
- Check for leaks via `HEALTH` command
- See `TROUBLESHOOTING.md`

---

## Project Structure

```
arduino-esp32/
├── src/
│   ├── config.h                    # Configuration constants
│   ├── main.cpp                    # Main application
│   ├── i2s_audio.h/cpp            # I2S + error classification
│   ├── network.h/cpp              # WiFi + TCP + state machine
│   ├── serial_command.h/cpp       # Serial commands
│   ├── debug_mode.h/cpp           # Debug configuration
│   ├── adaptive_buffer.h/cpp      # Buffer management
│   └── ... (other components)
│
├── test/
│   └── test_adaptive_buffer.cpp   # Unit tests
│
├── platformio.ini                  # Build configuration
├── README.md                        # This file
├── CONFIGURATION_GUIDE.md           # Configuration help
├── TROUBLESHOOTING.md              # Problem solving
└── ERROR_HANDLING.md               # Error reference
```

---

## Production Deployment

### Pre-deployment Checklist
- [ ] WiFi credentials configured
- [ ] Server host and port correct
- [ ] I2S pins verified for your hardware
- [ ] Debug level set to 0 or 1
- [ ] All tests passing
- [ ] Build successful with zero warnings
- [ ] Serial commands responding
- [ ] Statistics printing every 5 minutes

### Monitoring in Production
```bash
# Check every 5 minutes
STATS   # View statistics
HEALTH  # System health check
STATUS  # Current state
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 2.0 | Oct 20, 2025 | Phase 3: State machine, commands, debug, buffer, tests |
| 2.0 | Oct 20, 2025 | Phase 2: Enhanced I2S error handling |
| 2.0 | Oct 20, 2025 | Phase 1: Config validation, documentation, memory detection |

---

## Project Status

**Status**: ✅ **PRODUCTION-READY**

- ✅ All 14 planned improvements implemented
- ✅ Comprehensive error handling
- ✅ Extensive documentation
- ✅ Unit test framework
- ✅ Zero build warnings/errors
- ✅ Tested on multiple boards

---

## Support

1. Check `TROUBLESHOOTING.md` for common issues
2. Review `CONFIGURATION_GUIDE.md` for parameter help
3. See `ERROR_HANDLING.md` for error explanations
4. Use `HELP` serial command for available commands

---

**Last Updated**: October 20, 2025
**Build Status**: SUCCESS ✓
**License**: MIT
