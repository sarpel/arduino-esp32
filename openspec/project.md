# Project Context

## Purpose
ESP32 Audio Streamer v2.0 is a professional-grade I2S audio streaming system that transforms ESP32 microcontrollers into reliable audio streaming devices. The system captures audio from an INMP441 I2S digital microphone and streams it over WiFi/TCP to a remote server.

**Key Goals:**
- Achieve production-ready stability with >99.5% uptime
- Maintain <10% RAM usage through optimized memory management
- Provide professional audio quality with advanced processing
- Enable modular architecture for easy maintenance and extension
- Support multiple ESP32 variants (ESP32-DevKit, Seeed XIAO ESP32-S3)

## Tech Stack
- **Language**: C++ (Arduino framework)
- **Platform**: PlatformIO (Espressif32)
- **Target Boards**: ESP32-DevKit, Seeed XIAO ESP32-S3
- **Microcontroller**: ESP32 (240 MHz, 320KB RAM, 4MB Flash)
- **Audio Hardware**: INMP441 I2S digital microphone
- **Networking**: ESP32 WiFi + TCP/IP stack
- **Build System**: PlatformIO
- **Testing**: Unity test framework
- **Standard**: C++11 compatible (Arduino ESP32 framework)

### Core Libraries
- `WiFi` - Network connectivity and WiFi management
- `Update` - OTA firmware update support
- `ArduinoJson` - JSON configuration parsing
- `WebServer` - HTTP server for web interface
- `WiFiClientSecure` - HTTPS/TLS support
- `HTTPClient` - HTTP client for updates
- `ArduinoOTA` - Arduino OTA framework

## Project Conventions

### Code Style
**Naming Conventions:**
- Constants: `UPPER_SNAKE_CASE` (e.g., `WIFI_SSID`, `I2S_SAMPLE_RATE`)
- Functions: `camelCase` (e.g., `gracefulShutdown()`, `checkMemoryHealth()`)
- Variables: `snake_case` (e.g., `free_heap`, `audio_buffer`)
- Classes/Structs: `PascalCase` (e.g., `SystemStats`, `NetworkManager`)
- Defines: `UPPER_SNAKE_CASE`

**Documentation:**
- Doxygen-style docstrings for all public APIs: `/** @brief ... @param ... @return ... */`
- Member documentation: `///` for class members
- Implementation notes: `//` for inline comments
- Section headers: `// ========== SECTION ==========`

**Type System:**
- Explicit types preferred (no auto/inference)
- Arduino types: `uint8_t`, `uint32_t`, `uint64_t`, `unsigned long`
- Custom enums: `SystemState`, `TCPConnectionState`, etc.

### Architecture Patterns
**State Machine Architecture:**
- Explicit state transitions: `INITIALIZING → CONNECTING_WIFI → CONNECTING_SERVER → CONNECTED → ERROR`
- State-based error recovery with automatic transitions
- Non-blocking cooperative multitasking

**Modular Component Design:**
- Domain-driven directory structure: `core/`, `audio/`, `network/`, `monitoring/`, `security/`, `utils/`
- Separation of concerns: Each component handles one responsibility
- Event-driven communication via EventBus (publish-subscribe pattern)
- Header-only interfaces with implementation in .cpp files

**Memory Management:**
- Pool-based allocation to prevent fragmentation
- Explicit cleanup in component destructors
- Watchdog-protected memory monitoring
- RAII patterns for resource management

**Error Handling:**
- LOG_INFO, LOG_WARN, LOG_CRITICAL macros for structured logging
- State machine recovery: Transient errors → retry, Permanent errors → reinitialize, Fatal errors → restart
- Exponential backoff with jitter for network reconnection
- Graceful shutdown procedures with cleanup

### Testing Strategy
**Test Organization:**
- `tests/unit/` - Unit tests for individual components
- `tests/integration/` - Component interaction tests
- `tests/stress/` - Reliability and memory leak detection
- `tests/performance/` - Latency and throughput benchmarking

**Testing Framework:**
- Unity test framework (PlatformIO standard)
- Target: >80% code coverage
- Automated CI/CD via GitHub Actions
- Pre-commit validation and static analysis

**Quality Gates:**
- Zero compilation warnings policy
- Memory leak detection (stress tests)
- Performance regression detection
- State machine transition validation

### Git Workflow
**Branching Strategy:**
- `main` - Production-ready stable code
- `improve_3_kimi` - Current development branch
- Feature branches: `feature/` prefix for new capabilities
- Systematic commit messages documenting all fixes

**Commit Conventions:**
- Descriptive messages explaining "why" not just "what"
- Reference issue/phase numbers for systematic work
- Include compilation status changes
- Document breaking changes explicitly

**Current Status:**
- Main branch: `main`
- Active branch: `improve_3_kimi`
- Recent work: Phase 2c completion - 100% compilation success (383 → 0 errors)

## Domain Context

### Audio Streaming Fundamentals
**Audio Format:**
- Sample Rate: 16 kHz (16000 Hz)
- Bit Depth: 16-bit PCM (Pulse Code Modulation)
- Channels: Mono (left channel)
- Byte Order: Little-endian (ESP32 native)
- Bitrate: 256 Kbps (16000 Hz × 2 bytes × 8 bits)

**Streaming Protocol:**
- Chunk Size: 19200 bytes per TCP write
- Duration per Chunk: 600ms (9600 samples)
- TCP Server: 192.168.1.50:9000 (configurable)
- TCP Options: `TCP_NODELAY=1`, `SO_KEEPALIVE` enabled
- Keepalive: 5s idle, 5s interval, 3 probes

### Hardware Specifications
**ESP32 Capabilities:**
- CPU: Dual-core 240 MHz Xtensa LX6
- RAM: 320 KB SRAM
- Flash: 4 MB
- WiFi: 802.11 b/g/n (2.4 GHz only)
- I2S: 2 independent I2S interfaces

**INMP441 Microphone:**
- Interface: I2S digital output
- Sensitivity: -26 dBFS
- SNR: 61 dB
- Sample Rate: Up to 48 kHz
- Bit Depth: 24-bit (downsampled to 16-bit)

### I2S Pin Mapping
**ESP32-DevKit:**
- I2S_WS (Word Select): GPIO 15
- I2S_SD (Serial Data): GPIO 32
- I2S_SCK (Serial Clock): GPIO 14

**Seeed XIAO ESP32-S3:**
- I2S_WS: GPIO 3
- I2S_SD: GPIO 9
- I2S_SCK: GPIO 2

### Performance Characteristics
- Free Heap: ~248 KB (steady state)
- Memory Usage: <10% RAM (~49 KB active)
- Flash Usage: 59% (~768 KB)
- WiFi Connect Time: 2-5 seconds
- Server Connect Time: <100ms (local network)
- I2S Buffer Latency: ~100ms
- End-to-end Latency: 5-50ms (WiFi dependent)

## Important Constraints

### Hardware Constraints
- **Memory Limited**: 320 KB RAM total → aggressive optimization required
- **WiFi 2.4 GHz Only**: No 5 GHz band support
- **Single-core Limitations**: No true parallelism (cooperative multitasking only)
- **I2S Hardware**: 2 I2S interfaces maximum
- **Watchdog Timer**: 60-second timeout enforced

### Network Constraints
- **Local Network Only**: Designed for LAN operation (192.168.x.x)
- **TCP Chunk Size**: MUST match server expectation (19200 bytes)
- **RSSI Dependency**: Performance degrades below -70 dBm
- **No IPv6**: IPv4 only support

### Arduino Framework Constraints
- **C++11 Standard**: No C++14/17 features
- **No std::make_unique**: Must use `std::unique_ptr` with manual allocation
- **Arduino String**: Limited String type vs std::string
- **Enum Conflicts**: Arduino defines conflict with standard enums (e.g., INPUT, OUTPUT)

### Build System Constraints
- **PlatformIO Required**: Not compatible with Arduino IDE
- **Board-Specific Builds**: Different pin mappings require compile-time selection
- **OTA Size Limit**: Firmware must fit in available partition (~1.3 MB)

## External Dependencies

### Server-Side Integration
**Audio Receiver Server:**
- Expected Protocol: TCP on port 9000
- Expected Format: 16 kHz, 16-bit, Mono PCM
- Chunk Size: 19200 bytes (600ms)
- Reference Implementation: `audio-receiver/receiver.py` (Python)
- Network: Must be on same LAN as ESP32

### Development Tools
- **PlatformIO Core**: 6.1+ required
- **Python**: 3.8+ for toolchain
- **Serial Monitor**: 115200 baud for debugging
- **Upload Tool**: esptool.py (bundled with PlatformIO)

### Optional External Services
- **OTA Update Server**: HTTPS endpoint for firmware downloads
- **Configuration API**: REST API for remote configuration
- **Monitoring Service**: Optional metrics collection endpoint

### WiFi Network Requirements
- **Band**: 2.4 GHz (802.11 b/g/n)
- **Security**: WPA/WPA2 supported
- **Signal**: > -70 dBm recommended for stability
- **Latency**: < 50ms RTT for optimal performance
- **Bandwidth**: Minimum 256 Kbps upload required
