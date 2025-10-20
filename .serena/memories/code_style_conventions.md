# Code Style & Conventions

## Naming Conventions
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `WIFI_SSID`, `I2S_SAMPLE_RATE`)
- **Functions**: `camelCase` (e.g., `gracefulShutdown()`, `checkMemoryHealth()`)
- **Variables**: `snake_case` (e.g., `free_heap`, `audio_buffer`)
- **Classes/Structs**: `PascalCase` (e.g., `SystemStats`, `StateManager`)
- **Defines**: `UPPER_SNAKE_CASE`

## Code Organization
- Includes at top of file with sections (Arduino.h, config.h, etc.)
- Function declarations before globals
- Global state after declarations
- Main implementation after setup/loop
- Comments with `=====` separators for major sections

## Docstring/Comments Style
- Section headers: `// ===== Section Name =====`
- Inline comments: `// Brief explanation`
- Function explanations inline above definition or in brief
- No docstring blocks, prefer inline documentation

## Type Hints
- Arduino types: `uint8_t`, `uint32_t`, `uint64_t`, `unsigned long`
- ESP types: `SystemState`, custom enums
- No type inference, explicit types preferred

## Error Handling
- Uses LOG_INFO, LOG_WARN, LOG_CRITICAL macros
- State transitions for error recovery
- Graceful shutdown procedures
- Watchdog protection enabled
