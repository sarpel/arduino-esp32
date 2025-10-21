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

### Doxygen-style docstrings for all public APIs

- Use `/** ... */` blocks with standard Doxygen tags
- Document all public classes, methods, enums, and significant members
- Required tags:
  - `@brief` - Short description (first sentence)
  - `@param` - Parameter description (one per parameter)
  - `@return` - Return value description
  - `@note` - Important notes, warnings, or usage constraints
- Optional tags as needed:
  - `@warning` - Critical warnings about usage
  - `@code` / `@endcode` - Usage example blocks
  - `@see` - Cross-references to related functions
- Include usage examples for complex interfaces
- Explain thread-safety, lifecycle requirements, and side effects

**Example:**

```cpp
/**
 * @brief Initialize the adaptive buffer system with a base buffer size
 * @param base_size The initial buffer size in bytes (default: 4096)
 * @return True if initialization successful, false otherwise
 * @note This must be called during system initialization before any buffer operations
 */
static bool initialize(size_t base_size = 4096);
```
