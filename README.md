# ESP32 Audio Streamer v2.0

**Professional I2S audio streaming system for ESP32 with comprehensive reliability features**

[![Build Status](https://img.shields.io/badge/build-SUCCESS-brightgreen)](#)
[![RAM Usage](https://img.shields.io/badge/RAM-16.3%25-blue)](#)
[![Flash Usage](https://img.shields.io/badge/Flash-63.1%25-blue)](#)
[![License](https://img.shields.io/badge/license-MIT-green)](#)

---

## 🚀 Quick Start

### Hardware Requirements

- **ESP32 Board**: ESP32-DevKit or Seeed XIAO ESP32-S3
- **Microphone**: INMP441 I2S digital microphone
- **Server**: TCP server listening on port 9000 (192.168.1.50 by default)

### Hardware Connections

**ESP32-DevKit:**

```
INMP441 Pin → ESP32 Pin
  SCK      → GPIO 26
  WS       → GPIO 25
  SD       → GPIO 34
  GND      → GND
  VCC      → 3V3
```

**Seeed XIAO ESP32-S3:**

```
INMP441 Pin → XIAO Pin
  SCK      → GPIO 2
  WS       → GPIO 3
  SD       → GPIO 9
  GND      → GND
  VCC      → 3V3
```

### Software Installation

1. **Install PlatformIO**

   ```bash
   # VS Code: Install "PlatformIO IDE" extension
   # OR CLI: pip install platformio
   ```

2. **Clone & Configure**

   ```bash
   git clone <repository-url>
   cd arduino-esp32
   ```

3. **Edit Configuration** (`src/config.h`)

   ```cpp
   // WiFi credentials
   #define WIFI_SSID "YourNetwork"
   #define WIFI_PASSWORD "YourPassword"

   // Server settings
   #define SERVER_HOST "192.168.1.50"  // Your server IP
   #define SERVER_PORT 9000            // TCP port

   // Audio input gain (optional)
   #define AUDIO_GAIN_NUMERATOR 3      // Gain = 3/2 = 1.5x
   #define AUDIO_GAIN_DENOMINATOR 2
   ```

4. **Build & Upload**

   **For ESP32-DevKit:**
   ```bash
   # Build firmware
   pio run -e esp32dev

   # Upload to ESP32 (adjust COM port)
   pio run -e esp32dev --target upload --upload-port COM8

   # Monitor serial output
   pio device monitor --port COM8 --baud 115200
   ```

   **For Seeed XIAO ESP32-S3:**
   ```bash
   # Build firmware
   pio run -e seeed_xiao_esp32s3

   # Upload to XIAO (adjust COM port)
   pio run -e seeed_xiao_esp32s3 --target upload --upload-port COM8

   # Monitor serial output
   pio device monitor --port COM8 --baud 115200
   ```

### Expected Output

```
[INFO] ESP32 Audio Streamer Starting Up
[INFO] Board: ESP32-DevKit
[INFO] Input gain: 1.50x (3/2)
[INFO] WiFi connected - IP: 192.168.1.19
[INFO] OTA Update Service Started
[INFO] Hostname: ESP32-AudioStreamer
[INFO] Server connection established
[INFO] Starting audio transmission
```

---

## 🎯 Features

### Audio Streaming

- **Sample Rate**: 16 kHz (configurable 8-48 kHz)
- **Bit Depth**: 16-bit signed PCM
- **Channels**: Mono (1-channel)
- **Bitrate**: ~256 Kbps (~32 KB/sec)
- **Chunk Size**: 19200 bytes per TCP write (600ms audio buffer)
- **Input Gain**: Configurable (default 1.5x)

### Reliability & Error Recovery

- ✅ **WiFi Management**: Auto-reconnect with exponential backoff
- ✅ **TCP State Machine**: Connection lifecycle management
- ✅ **Error Classification**: Transient vs permanent error handling
- ✅ **I2S Recovery**: Automatic reinitialization on audio failures
- ✅ **Memory Protection**: Leak detection via heap trend analysis
- ✅ **Watchdog Timer**: Hardware watchdog (60s timeout)
- ✅ **Config Validation**: Startup parameter validation

### Control & Monitoring

- ✅ **Serial Commands**: 8 runtime control commands
- ✅ **Real-time Stats**: Automatic statistics reporting (5-min intervals)
- ✅ **Debug Levels**: 6 configurable verbosity levels (0-5)
- ✅ **Health Monitoring**: System health checks on demand
- ✅ **OTA Updates**: Over-the-air firmware updates

---

## 📊 System Architecture

```
┌─────────────┐      ┌──────────────┐      ┌──────────────┐
│  I2S Audio  │─────→│  Audio       │─────→│  WiFi/TCP    │
│  Input      │      │  Processing  │      │  Network     │
│  (16kHz)    │      │  (Gain)      │      │  Manager     │
└─────────────┘      └──────────────┘      └──────────────┘
       ↑                                            ↓
  INMP441 Mic                              TCP Server (Port 9000)

┌────────────────────────────────────────────────────────┐
│              State Machine (main loop)                  │
├────────────────────────────────────────────────────────┤
│ INITIALIZING → CONNECTING_WIFI → CONNECTING_SERVER    │
│                                  ↓                     │
│                            CONNECTED → (streaming)     │
│                                  ↓                     │
│                           (error?) → ERROR → recovery  │
└────────────────────────────────────────────────────────┘
```

---

## 🎮 Serial Commands

Control the ESP32 at runtime via serial terminal:

| Command | Description |
|---------|-------------|
| `HELP` | Show all available commands |
| `STATUS` | Display current system state (WiFi, TCP, memory) |
| `STATS` | Print detailed statistics (uptime, bytes, errors) |
| `HEALTH` | Perform system health check |
| `CONFIG SHOW` | Display current configuration |
| `CONNECT` | Manually trigger server connection |
| `DISCONNECT` | Disconnect from server |
| `RESTART` | Reboot the ESP32 |

**Example:**

```
# Open serial monitor
pio device monitor --port COM8 --baud 115200

# Type command
STATUS

# Output:
[INFO] ========== SYSTEM STATUS ==========
[INFO] WiFi: CONNECTED (192.168.1.19)
[INFO] WiFi Signal: -65 dBm
[INFO] TCP State: CONNECTED
[INFO] System State: CONNECTED
[INFO] Free Memory: 230412 bytes (225.0 KB)
```

---

## 🔧 Configuration

### Key Parameters (`src/config.h`)

**WiFi:**
- `WIFI_SSID`, `WIFI_PASSWORD` - Network credentials
- `WIFI_TIMEOUT` - Connection timeout (30s default)

**Server:**
- `SERVER_HOST` - TCP server IP address
- `SERVER_PORT` - TCP server port (9000 default)
- `TCP_CHUNK_SIZE` - Bytes per write (19200 = 600ms audio)

**Audio:**
- `I2S_SAMPLE_RATE` - Audio sample rate (16000 Hz default)
- `AUDIO_GAIN_NUMERATOR/DENOMINATOR` - Input gain (3/2 = 1.5x default)

**Debug:**
- `DEBUG_LEVEL` - Log verbosity (0=OFF, 3=INFO, 5=VERBOSE)

---

## 🔄 OTA Updates

### Enable OTA

OTA is automatically enabled after first USB flash. Find your ESP32:

```bash
# Find device on network
pio device list --mdns

# Or check serial output for IP address
[INFO] OTA Update Service Started
[INFO] IP Address: 192.168.1.19
```

### Upload via OTA

**For ESP32-DevKit:**
```bash
# Upload new firmware over WiFi
pio run -e esp32dev-ota --target upload
```

**For Seeed XIAO ESP32-S3:**
```bash
# Upload new firmware over WiFi
pio run -e seeed_xiao_esp32s3-ota --target upload
```

### Security (Optional)

Add password protection in `src/main.cpp`:

```cpp
ArduinoOTA.setPassword("YourSecurePassword");
```

---

## 🐛 Troubleshooting

### WiFi Connection Issues

**Problem**: `WiFi connection timeout`

**Solutions**:
- Verify SSID/password in `src/config.h`
- Ensure using 2.4 GHz network (5 GHz not supported)
- Check WiFi signal strength: Send `STATUS` command

### Server Connection Issues

**Problem**: `TCP write returned 0 (timeout or error)`

**Solutions**:
- Verify server is running:
  ```bash
  ss -tuln | grep 9000  # Linux/Mac
  netstat -an | findstr 9000  # Windows
  ```
- Check `SERVER_HOST` IP matches actual server
- Verify firewall allows port 9000
- Test connectivity: `telnet 192.168.1.50 9000`

### No Audio Streaming

**Problem**: Audio not reaching server

**Solutions**:
- Verify I2S pin connections match config.h
- Check microphone power (3.3V)
- Send `STATS` command to check I2S errors
- Inspect serial logs for I2S initialization

### Memory Issues

**Problem**: `Memory critically low` warnings

**Solutions**:
- Check for memory leaks: Send `STATS` command
- Reduce `I2S_BUFFER_SIZE` if needed
- Verify `MEMORY_WARN_THRESHOLD` appropriate for your use case

### Unexpected Reboots

**Problem**: ESP32 restarts randomly

**Solutions**:
- Check watchdog timeout: May need to increase `WATCHDOG_TIMEOUT_SEC`
- Verify power supply provides stable 3.3V
- Monitor serial output for crash stack traces
- Enable verbose logging: `DEBUG_LEVEL 5`

---

## 📈 Performance Metrics

### Resource Usage

- **RAM**: 53 KB / 320 KB (16.3%)
- **Flash**: 827 KB / 1.3 MB (63.1%)
- **CPU**: ~15-25% @ 240 MHz
- **Network**: ~32 KB/sec sustained throughput

### Reliability Metrics

- **WiFi Reconnect**: Exponential backoff (5s-60s)
- **TCP Retry**: Automatic with connection state tracking
- **Error Recovery**: <5s for transient errors
- **Uptime**: Days-weeks typical (with proper power)

---

## 📖 Documentation

- **README.md** (this file) - Quick start and user guide
- **DEVELOPMENT.md** - Comprehensive developer reference
- **AGENTS.md** - Repository guidelines and conventions
- **platformio.ini** - Build configuration

---

## 🔄 Recent Updates

**November 2025** - v2.0 Reliability Release

- ✅ Fixed indentation issues in main.cpp
- ✅ Corrected unsigned integer validation logic
- ✅ Updated hardware pin documentation
- ✅ Enhanced configuration validation
- ✅ Improved error recovery mechanisms
- ✅ Consolidated documentation

**October 2025** - Connection & OTA Updates

- ✅ Fixed 5-second startup delay before first connection
- ✅ Added OTA functionality with comprehensive error handling
- ✅ TCP socket protocol alignment complete
- ✅ Full server/client compatibility verified

---

## 🤝 Contributing

1. Follow coding standards in **AGENTS.md**
2. Test builds before committing: `pio run -e esp32dev` or `pio run -e seeed_xiao_esp32s3`
3. Run tests: `pio test -e esp32dev` or `pio test -e seeed_xiao_esp32s3`
4. Document changes in commit messages

---

## 📝 License

MIT License - See LICENSE file for details

---

## 🆘 Support

**Issues**: Create GitHub issue with:
- ESP32 board model
- Serial output logs
- Configuration (sanitize credentials)
- Steps to reproduce

**Documentation**: See **DEVELOPMENT.md** for detailed technical reference

---

**Status**: ✅ Production Ready | **Version**: 2.0 | **Last Updated**: November 2025
