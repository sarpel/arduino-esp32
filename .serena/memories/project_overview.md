# ESP32 Audio Streamer v2.0 - Project Overview

## Project Purpose
Professional-grade I2S audio streaming system for ESP32 with comprehensive reliability features. Streams audio from INMP441 I2S microphone to TCP server with state machine architecture and robust error handling.

## Tech Stack
- **Language**: C++ (Arduino framework)
- **Platform**: PlatformIO
- **Boards**: ESP32-DevKit, Seeed XIAO ESP32-S3
- **Framework**: Arduino ESP32
- **Audio**: I2S (INMP441 microphone)
- **Networking**: WiFi + TCP

## Core Architecture
- **State Machine**: INITIALIZING → CONNECTING_WIFI → CONNECTING_SERVER → CONNECTED
- **I2S Audio**: 16kHz, 16-bit, Mono (left channel)
- **Bitrate**: ~256 Kbps (32 KB/s)
- **Resource Usage**: 15% RAM (~49KB), 59% Flash (~768KB)

## Key Components
1. **config.h**: Configuration constants and thresholds
2. **main.cpp**: Core system loop, state management, statistics
3. **i2s_audio.cpp/h**: I2S audio acquisition
4. **network.cpp/h**: WiFi and TCP management
5. **logger.cpp/h**: Logging system
6. **StateManager.h**: State machine implementation
7. **NonBlockingTimer.h**: Non-blocking timer utility

## Build & Run Commands
```bash
pio run                    # Build project
pio run --target upload    # Upload to ESP32
pio device monitor         # Monitor serial (115200 baud)
```
