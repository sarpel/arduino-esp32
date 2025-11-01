# ESP32 Audio Streamer v2.0 - Quick Start Guide

**Professional-grade I2S audio streaming system for ESP32 with comprehensive reliability features.**

[![Build Status](https://img.shields.io/badge/build-SUCCESS-brightgreen)](#)
[![RAM Usage](https://img.shields.io/badge/RAM-15.0%25-blue)](#)
[![Flash Usage](https://img.shields.io/badge/Flash-59.6%25-blue)](#)
[![License](https://img.shields.io/badge/license-MIT-green)](#)

---

## 📚 Documentation Structure

This project now uses **3 consolidated documentation files**:

1. **README.md** (this file) - Quick Start & Overview
2. **DEVELOPMENT.md** - Complete Technical Reference
3. **TROUBLESHOOTING.md** - Diagnostics & Solutions

---

## 🚀 Quick Start

### Requirements

- **Hardware**: ESP32-DevKit or Seeed XIAO ESP32-S3
- **Microphone**: INMP441 I2S digital microphone
- **Tools**: PlatformIO IDE or CLI
- **Server**: TCP server listening on port 9000

### Hardware Connections

**ESP32-DevKit:**

```
INMP441 Pin → ESP32 Pin
  CLK      → GPIO 14
  WS       → GPIO 15
  SD       → GPIO 32
  GND      → GND
  VCC      → 3V3
```

**Seeed XIAO ESP32-S3:**

```
INMP441 Pin → XIAO Pin
  CLK      → GPIO 2
  WS       → GPIO 3
  SD       → GPIO 9
  GND      → GND
  VCC      → 3V3
```

### Installation & Configuration

1. **Clone the project**

   ```bash
   git clone <repo>
   cd arduino-esp32
   ```

2. **Edit `src/config.h`** with your settings:

   ```cpp
   // WiFi
   #define WIFI_SSID "YourNetwork"
   #define WIFI_PASSWORD "YourPassword"

   // Server
   #define SERVER_HOST "192.168.1.50"  // Your server IP
   #define SERVER_PORT 9000            // TCP port
   ```

3. **Upload firmware**

   ```bash
   pio run --target upload --upload-port COM8
   ```

4. **Monitor serial output**
   ```bash
   pio device monitor --port COM8 --baud 115200
   ```

### Expected Output

```
[INFO] ESP32 Audio Streamer Starting Up
[INFO] WiFi connected - IP: 192.168.1.19
[INFO] Attempting to connect to server 192.168.1.50:9000 (attempt 1)...
[INFO] Server connection established
[INFO] Starting audio transmission: first chunk is 19200 bytes
```

---

## 🎯 Core Features

### Streaming

- **Sample Rate**: 16 kHz
- **Bit Depth**: 16-bit
- **Channels**: Mono (1-channel)
- **Bitrate**: ~256 Kbps (~32 KB/sec)
- **Chunk Size**: 19200 bytes per TCP write (600ms of audio)

### Reliability

- ✅ WiFi auto-reconnect with exponential backoff
- ✅ TCP connection state machine
- ✅ Transient vs permanent error classification
- ✅ Automatic I2S reinitialization on failure
- ✅ Memory leak detection via heap trending
- ✅ Hardware watchdog timer (60 seconds)

### Control & Monitoring

- ✅ 8 Serial commands for runtime control
- ✅ Real-time statistics every 5 minutes
- ✅ 6 configurable debug levels
- ✅ System health monitoring

---

## 🛠️ Common Tasks

### Check System Status

```
Send serial command: STATS
Response: Current uptime, bytes sent, error counts, memory stats
```

### Change Debug Level

```
Send serial command: DEBUG 4
(0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE)
```

### View WiFi Signal Strength

```
Send serial command: SIGNAL
Response: Current RSSI in dBm
```

### Force Server Reconnect

```
Send serial command: RECONNECT
```

### View All Commands

```
Send serial command: HELP
```

---

## 📊 System Architecture

```
┌─────────────┐      ┌──────────────┐      ┌──────────────┐
│  I2S Audio  │─────→│  Adaptive    │─────→│  WiFi/TCP    │
│  Input      │      │  Buffer      │      │  Network     │
│  (16kHz)    │      │  (adaptive)  │      │  Manager     │
└─────────────┘      └──────────────┘      └──────────────┘
       ↑                                            ↓
  INMP441                                    Server (TCP)
  Microphone                                 Port 9000

┌────────────────────────────────────────────────────────┐
│              State Machine (main loop)                  │
├────────────────────────────────────────────────────────┤
│ INITIALIZING → CONNECTING_WIFI → CONNECTING_SERVER   │
│                                  ↓                     │
│                            CONNECTED → (loops)         │
│                                  ↓                     │
│                           (error?) → ERROR state       │
└────────────────────────────────────────────────────────┘
```

---

## 📋 Serial Command Reference

```
HELP              - Show all available commands
STATS             - Print system statistics (uptime, bytes sent, memory, errors)
STATUS            - Print current system state
SIGNAL            - Print WiFi RSSI (signal strength) in dBm
DEBUG [0-5]       - Set debug level (0=OFF, 5=VERBOSE)
RECONNECT         - Force server reconnection
REBOOT            - Restart the ESP32
```

---

## 🐛 Quick Troubleshooting

**ESP32 won't connect to WiFi?**

- Verify WiFi credentials in `config.h`
- Ensure network is 2.4 GHz (not 5 GHz)

**Server connection timeout?**

- Check `SERVER_HOST` matches actual server IP
- Verify server is listening: `ss -tuln | grep 9000`
- Check firewall allows port 9000

**No audio streaming?**

- Verify I2S pins match your board
- Check microphone connections
- Send `STATS` command to see error count

**For detailed help**, see `TROUBLESHOOTING.md`.

---

## 📦 Configuration Parameters

See `src/config.h` for complete reference:

- WiFi: SSID, password, retry settings
- Server: Host, port, reconnect backoff
- I2S: Sample rate (16kHz), buffer sizes
- Safety: Memory thresholds, watchdog timeout
- Debug: Log level (0-5)

---

## 🔄 Recent Updates

**October 21, 2025** - Connection Startup Bug Fix

- Fixed 5-second startup delay before first server connection
- Added `startExpired()` method to NonBlockingTimer
- Server connections now attempt immediately after WiFi

**October 20, 2025** - Protocol Alignment Complete

- TCP socket options verified and aligned
- Data format: 16kHz, 16-bit, mono ✓
- Chunk size: 19200 bytes ✓
- Full server/client compatibility ✓

---

## 📖 For More Information

- **Complete Technical Reference** → `DEVELOPMENT.md`
- **Troubleshooting & Diagnostics** → `TROUBLESHOOTING.md`
- **Source Code** → `src/` directory

---

**Status**: ✅ Production Ready | **Last Updated**: October 21, 2025 | **Version**: 2.0
