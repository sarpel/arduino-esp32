# GitHub Copilot Instructions for ESP32 Audio Streamer v2.0

## Project Overview

This is an ESP32 Audio Streamer v2.0 - a professional-grade I2S audio streaming system designed for reliability and robustness. The project streams audio from an INMP441 I2S microphone to a TCP server over WiFi.

## Code Style & Conventions

### Naming Conventions
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `WIFI_SSID`, `I2S_SAMPLE_RATE`)
- **Functions**: `camelCase` (e.g., `gracefulShutdown()`, `checkMemoryHealth()`)
- **Variables**: `snake_case` (e.g., `free_heap`, `audio_buffer`)
- **Classes/Structs**: `PascalCase` (e.g., `SystemStats`, `StateManager`)
- **Defines**: `UPPER_SNAKE_CASE`

### Code Organization
- Includes at top with logical sections
- Function declarations before globals
- Use section separators: `// ===== Section Name =====`
- Static buffers preferred over heap allocation
- All timeouts and delays should be constants from `config.h`

### Arduino-Specific
- Use Arduino types: `uint8_t`, `uint32_t`, `unsigned long`
- Prefer `millis()` over `delay()` for timing
- Non-blocking operations whenever possible
- Feed watchdog timer in main loop

## Architecture Principles

### State Machine
- Explicit states: `INITIALIZING`, `CONNECTING_WIFI`, `CONNECTING_SERVER`, `CONNECTED`, `ERROR`
- Clear state transitions with logging
- State validation to prevent corruption

### Error Handling
- Three-tier error classification:
  - `TRANSIENT`: Retry likely to succeed
  - `PERMANENT`: Reinitialization needed
  - `FATAL`: System restart required
- Use logging macros: `LOG_INFO()`, `LOG_WARN()`, `LOG_ERROR()`, `LOG_CRITICAL()`
- Always log state changes and errors

### Memory Management
- Static allocation preferred
- Monitor heap with trend detection
- Warn at 40KB free, critical at 20KB free
- Track peak and minimum heap usage

## Key Design Patterns

### 1. Configuration Validation
All configuration must be validated at startup. Never start with invalid config:
```cpp
if (!ConfigValidator::validateAll()) {
    // Halt and log errors
    while(1) { delay(5000); }
}
```

### 2. Non-Blocking Operations
Use timers instead of delays:
```cpp
NonBlockingTimer timer(INTERVAL, true);
if (timer.check()) {
    // Do periodic task
}
```

### 3. Watchdog Protection
Feed watchdog in every loop iteration:
```cpp
void loop() {
    esp_task_wdt_reset();  // Always first
    // ... rest of loop
}
```

### 4. Circuit Breaker (Planned)
For repeated failures, use circuit breaker pattern to prevent resource exhaustion.

### 5. State Validation
Periodically validate system state matches reality:
```cpp
bool wifi_actual = WiFi.status() == WL_CONNECTED;
bool wifi_state = NetworkManager::isWiFiConnected();
if (wifi_actual != wifi_state) {
    // Fix state mismatch
}
```

## Common Patterns

### Adding New Features
1. Add configuration constants to `src/config.h`
2. Add validation to `src/config_validator.h`
3. Implement with error handling
4. Add logging at key points
5. Update documentation
6. Add tests if applicable

### Error Handling Template
```cpp
bool myFunction() {
    // Try operation
    esp_err_t result = someESP32Function();
    
    if (result != ESP_OK) {
        // Classify error
        ErrorType type = classifyError(result);
        
        // Log appropriately
        if (type == TRANSIENT) {
            LOG_WARN("Transient error: %d - retry", result);
        } else if (type == PERMANENT) {
            LOG_ERROR("Permanent error: %d - reinit needed", result);
        } else {
            LOG_CRITICAL("Fatal error: %d", result);
        }
        
        return false;
    }
    
    return true;
}
```

### Adding Serial Commands
See `src/serial_command.cpp` for examples. Pattern:
```cpp
void handleMyCommand(const char* args) {
    LOG_INFO("========== MY COMMAND ==========");
    // Parse args
    // Execute command
    // Display results
    LOG_INFO("================================");
}
```

## Critical Rules

### DO:
✅ Validate all configuration at startup
✅ Use constants from `config.h` (no magic numbers)
✅ Feed watchdog timer in main loop
✅ Log state changes and errors
✅ Use non-blocking operations
✅ Track memory usage and trends
✅ Check for state corruption
✅ Handle all error cases
✅ Test on both ESP32-DevKit and XIAO ESP32-S3

### DON'T:
❌ Use hardcoded delays or timeouts
❌ Block the main loop for >1 second
❌ Allocate large buffers on heap
❌ Start with invalid configuration
❌ Ignore error return values
❌ Log WiFi passwords
❌ Assume WiFi/TCP is always connected

## Testing Requirements

### Before Committing
- Code compiles without warnings
- Build succeeds for both boards (`pio run`)
- No new magic numbers introduced
- All errors logged appropriately
- Configuration validated

### Before Merging
- Full test suite passes
- 48-hour stress test complete
- No bootloops detected
- Memory leak check passes
- All documentation updated

## Documentation Standards

### Code Comments
- Use `//` for inline comments
- Use `/* */` for block comments sparingly
- Section headers: `// ===== Section Name =====`
- Explain WHY, not WHAT (code shows what)

### Markdown Files
- Keep line length reasonable (~100 chars)
- Use tables for structured data
- Include examples for complex topics
- Link to related documentation

## Reliability Focus

This project prioritizes reliability above all else. When suggesting code:

1. **Crash Prevention**: Will this ever crash? Add checks.
2. **Bootloop Prevention**: Can this cause restart loops? Add protection.
3. **Resource Leaks**: Are resources properly freed? Verify.
4. **State Corruption**: Can state become invalid? Add validation.
5. **Error Recovery**: What happens if this fails? Handle gracefully.

## ESP32-Specific Considerations

### Memory
- Total RAM: ~327 KB
- Target usage: <15% (~49 KB)
- Watch for fragmentation
- Use PSRAM if available (XIAO ESP32-S3)

### WiFi
- 2.4GHz only
- Signal monitoring enabled
- Automatic reconnection
- Exponential backoff on failures

### I2S
- 16kHz sample rate
- 16-bit mono
- DMA buffers used
- Error classification implemented

## Priority Features

When enhancing the project, prioritize:
1. **Bootloop prevention** - Highest priority
2. **Crash recovery** - Critical
3. **Circuit breaker** - High
4. **State validation** - High
5. **Resource monitoring** - Medium
6. New features - Lower priority

## References

- `README.md` - Project overview
- `CONFIGURATION_GUIDE.md` - All config options
- `TROUBLESHOOTING.md` - Common issues
- `ERROR_HANDLING.md` - Error reference
- `RELIABILITY_IMPROVEMENT_PLAN.md` - Future enhancements
- `PR1_REVIEW_ACTION_PLAN.md` - PR review guidelines

## Questions?

When uncertain about:
- **Architecture**: Follow existing patterns in `src/main.cpp`
- **Error Handling**: See `ERROR_HANDLING.md`
- **Configuration**: Check `src/config.h` and `config_validator.h`
- **Testing**: Refer to `test_framework.md`

---

**Remember**: Reliability > Features. Always.
