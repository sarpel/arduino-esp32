# ESP32 Audio Streamer - Developer Onboarding Guide

Welcome to the ESP32 Audio Streamer project! This guide will help you get started with development.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Development Environment](#development-environment)
3. [Project Structure](#project-structure)
4. [Architecture Overview](#architecture-overview)
5. [Development Workflow](#development-workflow)
6. [Testing](#testing)
7. [Code Standards](#code-standards)
8. [Common Tasks](#common-tasks)
9. [Debugging](#debugging)
10. [Resources](#resources)

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

# 2. Install dependencies (PlatformIO handles this automatically)
pio run --target clean

# 3. Build firmware
pio run

# 4. Upload to board
pio run --target upload --upload-port COM8  # Adjust port

# 5. Monitor serial output
pio device monitor --port COM8 --baud 115200
```

---

## Development Environment

### Recommended Tools

| Tool | Purpose | Required |
|------|---------|----------|
| **VSCode** | Primary IDE | ✅ Yes |
| **PlatformIO Extension** | Build/upload/debug | ✅ Yes |
| **Doxygen** | API documentation | ⚠️ Recommended |
| **Git** | Version control | ✅ Yes |

### VSCode Extensions

Install these for optimal development experience:

```json
{
  "recommendations": [
    "platformio.platformio-ide",
    "ms-vscode.cpptools",
    "cschlosser.doxdocgen",
    "twxs.cmake"
  ]
}
```

### Environment Setup

```bash
# Install PlatformIO Core
pip install platformio

# Verify installation
pio --version

# Install platform
pio platform install espressif32
```

---

## Project Structure

```
arduino-esp32/
├── src/                      # Source code
│   ├── main.cpp             # Entry point
│   ├── config.h             # Configuration
│   ├── core/                # Core system components
│   │   ├── SystemManager.*  # Main orchestrator
│   │   ├── EventBus.*       # Event system
│   │   └── StateMachine.*   # State management
│   ├── audio/               # Audio processing
│   │   ├── AudioProcessor.* # Main audio handler
│   │   ├── NoiseGate.*     # Noise reduction
│   │   └── ...
│   ├── network/             # Network management
│   │   ├── NetworkManager.* # WiFi & TCP
│   │   ├── ConnectionPool.* # Connection pooling
│   │   └── ...
│   ├── monitoring/          # Health & diagnostics
│   ├── security/            # Security features
│   └── utils/               # Utilities
│       ├── MemoryManager.*  # Memory pools
│       ├── EnhancedLogger.* # Logging system
│       └── ...
├── tests/                   # Test suites
│   ├── unit/               # Unit tests
│   ├── integration/        # Integration tests
│   ├── stress/             # Stress tests
│   └── performance/        # Performance tests
├── docs/                    # Documentation
│   ├── adr/                # Architecture decisions
│   ├── api/                # Generated API docs
│   └── ino/                # Implementation docs
├── scripts/                 # Build & utility scripts
├── platformio.ini          # Build configuration
└── Doxyfile                # Documentation config
```

### Key Files

| File | Purpose |
|------|---------|
| `src/config.h` | System configuration parameters |
| `src/main.cpp` | Application entry point |
| `platformio.ini` | Build settings for all boards |
| `Doxyfile` | API documentation generation |

---

## Architecture Overview

### Design Patterns

The project uses several key patterns:

1. **Singleton Pattern**: SystemManager provides global access point
2. **Event-Driven Architecture**: EventBus decouples components
3. **State Machine Pattern**: Manages system state transitions
4. **Memory Pool Pattern**: Prevents heap fragmentation
5. **Circuit Breaker Pattern**: Enhances reliability

### Component Interaction

```
┌─────────────────────────────────────────┐
│         SystemManager (Singleton)       │
│  ┌───────────────────────────────────┐  │
│  │     EventBus (Pub-Sub)            │  │
│  └────────────┬──────────────────────┘  │
│               │                         │
│  ┌────────────┼────────────────┐       │
│  ▼            ▼                ▼        │
│┌──────┐  ┌──────┐  ┌────────────┐     │
││State │  │Audio │  │Network Mgr │     │
││Mach  │  │Proc  │  │            │     │
│└──────┘  └──────┘  └────────────┘     │
│               │                         │
│  ┌────────────┴────────────┐           │
│  │   MemoryManager         │           │
│  │   (Pool Allocation)     │           │
│  └─────────────────────────┘           │
└─────────────────────────────────────────┘
```

### Key Concepts

**EventBus**: Central pub-sub system
```cpp
// Subscribe to events
eventBus->subscribe(SystemEvent::MEMORY_CRITICAL,
                   callback, EventPriority::CRITICAL_PRIORITY);

// Publish events
eventBus->publish(SystemEvent::AUDIO_QUALITY_DEGRADED, &data);
```

**Memory Pools**: Prevent fragmentation
```cpp
// Type-safe allocation
void* audio_buf = memManager->allocateAudioBuffer(4096);
void* net_buf = memManager->allocateNetworkBuffer(2048);

// Automatic pool detection on deallocation
memManager->deallocate(audio_buf);
```

**State Machine**: Manage system states
```cpp
enum class SystemState {
    INITIALIZING, CONNECTING_WIFI, CONNECTING_SERVER,
    STREAMING, ERROR, RECOVERING, SHUTDOWN
};
```

---

## Development Workflow

### 1. Feature Development

```bash
# Create feature branch
git checkout -b feature/my-feature

# Make changes
# ... edit code ...

# Build and test
pio run
pio test

# Commit with meaningful message
git add .
git commit -m "Add feature: description"

# Push and create PR
git push origin feature/my-feature
```

### 2. Code Review Checklist

Before submitting PR:

- [ ] Code compiles without warnings
- [ ] Tests pass (`pio test`)
- [ ] Documentation updated
- [ ] Memory usage checked (use build size reporter)
- [ ] Follows code style guidelines
- [ ] No hardcoded values (use config.h)
- [ ] Logging added for important events

### 3. Testing Workflow

```bash
# Run all tests
pio test

# Run specific test environment
pio test -e unit

# Run specific test file
pio test -f test_audio_processor

# Monitor test output
pio test -v  # Verbose mode
```

---

## Testing

### Test Organization

```
tests/
├── unit/                    # Component isolation tests
├── integration/             # Multi-component tests
├── stress/                  # Load & endurance tests
└── performance/             # Benchmarking tests
```

### Writing Tests

**Unit Test Example** (`tests/unit/test_my_component.cpp`):

```cpp
#include <unity.h>
#include "MyComponent.h"

void test_component_initialization() {
    MyComponent component;
    TEST_ASSERT_TRUE(component.initialize());
}

void test_component_functionality() {
    MyComponent component;
    component.initialize();

    int result = component.doSomething(42);
    TEST_ASSERT_EQUAL(42, result);
}

void setUp(void) {
    // Runs before each test
}

void tearDown(void) {
    // Runs after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_component_initialization);
    RUN_TEST(test_component_functionality);
    return UNITY_END();
}
```

### Test Best Practices

1. **Isolation**: Each test should be independent
2. **Naming**: Use descriptive test names (`test_what_when_expected`)
3. **Coverage**: Test happy path, edge cases, and error conditions
4. **Cleanup**: Always clean up resources in tearDown()
5. **Assertions**: Use appropriate TEST_ASSERT_* macros

---

## Code Standards

### C++ Style

```cpp
// Class naming: PascalCase
class AudioProcessor {
public:
    // Method naming: camelCase
    bool initialize();
    void processAudioData(uint8_t* buffer, size_t length);

private:
    // Member variables: snake_case with trailing underscore
    uint32_t sample_rate_;
    bool is_initialized_;
};

// Constants: UPPER_SNAKE_CASE
#define MAX_BUFFER_SIZE 4096
static constexpr uint32_t DEFAULT_SAMPLE_RATE = 16000;

// Enums: PascalCase for type, UPPER_SNAKE_CASE for values
enum class SystemState {
    INITIALIZING,
    RUNNING,
    ERROR
};
```

### Documentation

Use Doxygen-style comments:

```cpp
/**
 * @brief Process raw audio data from I2S microphone
 *
 * Reads audio samples, applies noise reduction, and prepares
 * for network transmission.
 *
 * @param buffer Pointer to output buffer (must be 16-bit aligned)
 * @param buffer_size Size of output buffer in bytes
 * @param[out] bytes_read Number of bytes actually read
 * @return true if read successful, false on error
 *
 * @note This function may block up to 1 second waiting for I2S data
 * @warning Buffer must be allocated before calling
 */
bool readAudioData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read);
```

### Logging

Use structured logging with appropriate levels:

```cpp
LOG_DEBUG("Processing %u audio samples", sample_count);
LOG_INFO("WiFi connected to %s (RSSI: %d dBm)", ssid, rssi);
LOG_WARN("Low memory: %u bytes free", free_memory);
LOG_ERROR("I2S read failed: error code %d", error);
LOG_CRITICAL("System failure: %s", reason);
```

---

## Common Tasks

### Adding a New Component

1. Create header and implementation files
2. Add to appropriate directory (core/, audio/, network/, etc.)
3. Register with SystemManager if needed
4. Add unit tests
5. Update documentation

### Modifying Configuration

```cpp
// src/config.h
#define MY_NEW_PARAMETER 42
#define MY_FEATURE_ENABLED true
```

### Adding New Events

```cpp
// src/core/SystemTypes.h
enum class SystemEvent {
    // ... existing events ...
    MY_NEW_EVENT,
};
```

### Generating API Documentation

```bash
# Generate HTML documentation
doxygen Doxyfile

# View documentation
open docs/api/html/index.html  # macOS/Linux
start docs/api/html/index.html # Windows
```

### Build Size Reporting

```bash
# After building
python scripts/report_build_size.py
```

---

## Debugging

### Serial Monitor

```bash
# Monitor with filter
pio device monitor --port COM8 --baud 115200 --filter esp32_exception_decoder
```

### Debug Levels

```cpp
// src/config.h
#define DEBUG_LEVEL 3  // 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE
```

### Serial Commands

While monitoring serial output:

```
HELP       - Show available commands
STATUS     - System status
STATS      - Detailed statistics
MEMORY     - Memory information
AUDIO      - Audio statistics
NETWORK    - Network information
HEALTH     - Health status
REBOOT     - Restart system
```

### Common Issues

**Issue**: Compilation errors
```bash
# Clean and rebuild
pio run --target clean
pio run
```

**Issue**: Upload fails
```bash
# Hold BOOT button on ESP32 during upload
# Or try different upload speed in platformio.ini
```

**Issue**: Out of memory
```bash
# Check memory usage
python scripts/report_build_size.py

# Reduce pool sizes in src/config.h
# Disable unused features
```

---

## Resources

### Documentation

- **Architecture Decisions**: `docs/adr/`
- **API Documentation**: Generate with `doxygen Doxyfile`
- **Technical Reference**: `docs/ino/TECHNICAL_REFERENCE.md`
- **Reliability Guide**: `docs/ino/RELIABILITY_GUIDE.md`

### External Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32)
- [Unity Testing Framework](https://github.com/ThrowTheSwitch/Unity)

### Getting Help

1. Check existing documentation in `docs/`
2. Review Architecture Decision Records in `docs/adr/`
3. Search issues in project repository
4. Ask in project communication channels

---

## Next Steps

1. **Read Architecture Decision Records**: Understand key design choices
2. **Run Tests**: Familiarize yourself with test suite
3. **Build and Flash**: Get hands-on experience
4. **Pick a Good First Issue**: Start with beginner-friendly tasks
5. **Join Discussion**: Engage with the development community

---

**Happy Coding! 🚀**

*Last Updated*: 2025-11-01
