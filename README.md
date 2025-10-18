# ESP32 Audio Streamer v2.0

Professional-grade I2S audio streaming system for ESP32 with comprehensive reliability features.

## Features

✅ **Robust Audio Streaming**: INMP441 I2S microphone → TCP server
✅ **State Machine Architecture**: Clear system states with automatic transitions
✅ **Intelligent Reconnection**: Exponential backoff (5s → 60s)
✅ **Watchdog Protection**: Prevents system resets during operations
✅ **Memory Monitoring**: Proactive crash prevention
✅ **WiFi Quality Monitoring**: Preemptive reconnection on weak signal
✅ **TCP Keepalive**: Fast dead connection detection
✅ **Comprehensive Logging**: Detailed system visibility
✅ **Statistics Tracking**: Operational metrics
✅ **Graceful Shutdown**: Clean resource cleanup

## Hardware Requirements

- **ESP32 Dev Module** (or compatible)
- **INMP441 I2S Microphone**
- **WiFi Network** (2.4GHz)
- **TCP Server** (listening on configured port)

### Wiring (INMP441 → ESP32)

| INMP441 Pin | ESP32 GPIO | Description |
|-------------|------------|-------------|
| SCK (BCLK)  | GPIO 14    | Bit Clock |
| WS (LRC)    | GPIO 15    | Word Select |
| SD (DOUT)   | GPIO 32    | Serial Data |
| VDD         | 3.3V       | Power |
| GND         | GND        | Ground |
| L/R         | GND        | Left channel |

## Quick Start

### 1. Configure Settings

Edit `src/config.h`:

```cpp
// WiFi credentials
#define WIFI_SSID       "YourWiFi"
#define WIFI_PASSWORD   "YourPassword"

// Server settings
#define SERVER_HOST     "192.168.1.50"
#define SERVER_PORT     9000
```

### 2. Build and Upload

```bash
# Build project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

## Documentation

- **[IMPROVEMENT.md](./IMPROVEMENT.md)** - Detailed improvement plan and specifications
- **[IMPLEMENTATION_COMPLETE.md](./IMPLEMENTATION_COMPLETE.md)** - Full implementation details, troubleshooting, and configuration guide

## System States

```
INITIALIZING → CONNECTING_WIFI → CONNECTING_SERVER → CONNECTED (streaming)
```

## Audio Format

- **Sample Rate**: 16 kHz
- **Bit Depth**: 16-bit
- **Channels**: Mono (left channel)
- **Format**: Raw PCM, little-endian
- **Bitrate**: ~256 Kbps (32 KB/s)

## Performance

- **RAM Usage**: 15% (~49 KB)
- **Flash Usage**: 59% (~768 KB)
- **Uptime Target**: 99%+
- **Reconnection**: < 30 seconds

## Version

**v2.0** - Reliability-Enhanced Release (2025-10-18)

---

For complete documentation, see [IMPLEMENTATION_COMPLETE.md](./IMPLEMENTATION_COMPLETE.md)
