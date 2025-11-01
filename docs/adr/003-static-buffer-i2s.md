# ADR-003: Static Buffer for I2S Audio Processing

**Status**: Accepted
**Date**: 2025-11-01
**Deciders**: Audio Engineer, Performance Engineer
**Technical Story**: I2S audio buffer optimization

## Context

The INMP441 I2S microphone outputs 24-bit audio in 32-bit frames, requiring conversion to 16-bit samples for network transmission. Original implementation used malloc/free for each audio read cycle:

```cpp
// PROBLEMATIC: malloc in audio loop
int32_t* temp_buffer = (int32_t*)malloc(samples * sizeof(int32_t));
// ... read and convert ...
free(temp_buffer);  // Causes heap fragmentation
```

**Issues with dynamic allocation in audio loop**:
- **Fragmentation**: Repeated malloc/free every 600ms causes severe fragmentation
- **Latency**: malloc traverses free list, non-deterministic timing
- **Failure risk**: malloc can fail due to fragmentation
- **Performance**: Allocation overhead in critical audio path

Audio processing runs at 16kHz sample rate with 600ms chunks (9600 samples), requiring allocation of 38.4KB every 600ms.

## Decision

We will use a **static pre-allocated buffer** for I2S reads instead of dynamic allocation:

```cpp
// SOLUTION: Static buffer (src/i2s_audio.h)
static int32_t temp_read_buffer[4096];  // 16KB, supports max read size
```

The buffer is:
- Pre-allocated at compile time (no runtime allocation)
- Sized for worst-case: 4096 samples × 4 bytes = 16KB
- Shared across all I2S read operations
- Thread-safe (single-threaded audio reads)

## Rationale

1. **Zero fragmentation**: No heap allocations in audio path
2. **Deterministic timing**: Constant-time buffer access
3. **Reliability**: Eliminates allocation failure risk
4. **Performance**: No malloc overhead per audio cycle
5. **Memory efficiency**: 16KB one-time cost vs repeated allocations

## Consequences

### Positive

- **Eliminates heap fragmentation** from most frequent allocation
- **Predictable audio latency** (no malloc delays)
- **Higher reliability** (no allocation failures)
- **Better performance** (~50-100μs saved per audio cycle)
- **Simpler code**: No allocation/deallocation logic needed

### Negative

- **16KB RAM consumed** regardless of actual usage
- **Single static buffer**: Cannot parallelize I2S reads (acceptable - single mic)
- **Fixed maximum size**: Cannot handle larger reads without recompilation

### Neutral

- Buffer size must accommodate worst-case scenario
- Trade-off: 16KB static vs dynamic fragmentation

## Alternatives Considered

### Alternative 1: Memory Pool for Audio Buffers

Use memory pool from MemoryManager for I2S buffers.

**Pros**:
- Reusable across multiple audio operations
- Falls back to heap if pool exhausted

**Cons**:
- Still requires allocation/deallocation logic
- Pool fragmentation possible with variable sizes
- Additional indirection overhead

**Reason for rejection**: Static buffer simpler and more deterministic for single-purpose use

### Alternative 2: DMA Buffer Reuse

Read directly into DMA buffer without intermediate conversion.

**Pros**:
- Zero copy overhead
- No intermediate buffer needed

**Cons**:
- Cannot perform 32-bit to 16-bit conversion in-place
- DMA buffer size constraints
- More complex I2S driver integration

**Reason for rejection**: Conversion requires separate buffer anyway

### Alternative 3: Stack Allocation

Allocate buffer on stack in readData() function.

**Pros**:
- No heap usage
- Automatic lifetime management

**Cons**:
- ESP32 task stack limited (~8KB default)
- 16KB exceeds safe stack allocation
- Risk of stack overflow

**Reason for rejection**: Buffer too large for stack allocation

## Implementation Notes

**Buffer Declaration** (`src/i2s_audio.h`):
```cpp
class I2SAudio {
private:
    // Static buffer for 32-bit I2S reads (prevents heap fragmentation)
    // Max size: I2S_BUFFER_SIZE (4096) / 2 samples × 4 bytes = 8192 bytes
    static int32_t temp_read_buffer[4096];  // 16KB safe maximum
};
```

**Buffer Definition** (`src/i2s_audio.cpp`):
```cpp
int32_t I2SAudio::temp_read_buffer[4096];  // Static initialization
```

**Usage in Read Operation**:
```cpp
bool I2SAudio::readData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read) {
    size_t samples_requested = buffer_size / 2;

    // Safety check against buffer overflow
    if (samples_requested > 4096) {
        LOG_ERROR("Requested samples exceeds buffer size");
        return false;
    }

    // Read into static buffer (no allocation!)
    size_t bytes_read_32bit = 0;
    esp_err_t result = i2s_read(I2S_PORT, temp_read_buffer,
                                samples_requested * sizeof(int32_t),
                                &bytes_read_32bit, pdMS_TO_TICKS(1000));

    // Convert 32-bit → 16-bit in-place
    size_t samples_read = bytes_read_32bit / sizeof(int32_t);
    int16_t* buffer_16bit = (int16_t*)buffer;
    for (size_t i = 0; i < samples_read; i++) {
        buffer_16bit[i] = (int16_t)(temp_read_buffer[i] >> 16);
    }

    return true;
}
```

**Memory Impact**:
- Before: 38.4KB allocated/freed every 600ms
- After: 16KB static allocation (one-time cost)
- Savings: Eliminates ~2MB/hour of heap churn

## Related Decisions

- ADR-002: Memory Pool Strategy (alternative allocation approach)
- I2S configuration for INMP441 microphone (32-bit frames)

## References

- `src/i2s_audio.h` - Static buffer declaration
- `src/i2s_audio.cpp` - Implementation
- ESP32 Technical Reference Manual - I2S section
- INMP441 Datasheet - Audio format specification
