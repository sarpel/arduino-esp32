# ADR-002: Memory Pool Allocation Strategy

**Status**: Accepted
**Date**: 2025-11-01
**Deciders**: System Architect, Performance Engineer
**Technical Story**: Memory optimization for reliable 24/7 operation

## Context

ESP32 devices have limited RAM (520KB total, ~200KB available for applications). Long-running audio streaming applications face memory fragmentation challenges:

- Frequent malloc/free operations cause heap fragmentation
- Fragmentation leads to allocation failures even when total free memory exists
- Audio processing requires predictable memory allocation
- Network buffers are allocated/deallocated frequently
- System must maintain <20% RAM usage for stability

Traditional heap allocation (malloc/free) is unsuitable for real-time audio processing on embedded systems.

## Decision

We will implement a **Memory Pool Allocation Strategy** with typed pools for different use cases:

1. **Audio Buffer Pool**: Fixed-size pool for audio sample buffers
2. **Network Buffer Pool**: Fixed-size pool for TCP send/receive buffers
3. **General Buffer Pool**: Variable-size pool for miscellaneous allocations
4. **Static Buffers**: Pre-allocated static buffers for I2S operations

## Rationale

1. **Fragmentation Prevention**: Pools eliminate fragmentation for repeated allocations
2. **Deterministic Allocation**: O(1) allocation time, predictable behavior
3. **Memory Efficiency**: Optimized block sizes reduce waste
4. **Leak Detection**: Easy to track allocations per pool
5. **Performance**: Faster than heap allocation (no free list traversal)

## Consequences

### Positive

- **Zero heap fragmentation** for pooled allocations
- **Predictable memory usage**: Pool sizes configured at compile-time
- **Fast allocation**: O(1) time complexity
- **Easy debugging**: Pool statistics show usage patterns
- **Leak prevention**: Automatic tracking of all allocations
- **Emergency cleanup**: Can free unused pools under memory pressure

### Negative

- **Memory overhead**: Pre-allocated pools consume memory even when unused
- **Fixed limits**: Pool exhaustion possible if underestimated
- **Complexity**: Additional abstraction layer vs direct malloc
- **Configuration**: Requires tuning pool sizes for optimal usage

### Neutral

- Pool sizes must be profiled and adjusted for workload
- Falls back to heap allocation when pools exhausted
- Requires memory pressure monitoring

## Alternatives Considered

### Alternative 1: FreeRTOS Heap Allocator (heap_4)

Use FreeRTOS heap_4 with best-fit allocation.

**Pros**:
- Built into FreeRTOS
- Coalesces adjacent free blocks
- Well-tested

**Cons**:
- Still subject to fragmentation over time
- Non-deterministic allocation time
- No type-based allocation tracking
- No pool statistics

**Reason for rejection**: Does not solve fragmentation problem for long-running systems

### Alternative 2: TLSF (Two-Level Segregated Fit)

Use TLSF allocator for O(1) allocation with low fragmentation.

**Pros**:
- O(1) allocation and deallocation
- Low fragmentation
- Industry-proven

**Cons**:
- Larger code size (~5KB)
- More complex implementation
- Requires memory overhead for segregated lists
- Overkill for ESP32's limited RAM

**Reason for rejection**: Pool-based approach simpler and more appropriate for embedded

### Alternative 3: Static Allocation Only

Pre-allocate all memory statically, no dynamic allocation.

**Pros**:
- Zero fragmentation
- Zero allocation overhead
- Completely deterministic

**Cons**:
- Inflexible and wasteful
- Hard to tune memory usage
- Cannot adapt to varying workloads
- Complex lifecycle management

**Reason for rejection**: Too rigid for modular architecture

## Implementation Notes

**Memory Pool Configuration** (`src/utils/MemoryManager.h`):
```cpp
struct MemoryConfig {
    size_t audio_buffer_pool_size = 4;      // 4 blocks × 4KB = 16KB
    size_t network_buffer_pool_size = 2;    // 2 blocks × 20KB = 40KB
    size_t general_buffer_pool_size = 8;    // 8 blocks × 512B = 4KB
    size_t max_heap_allocation = 65536;     // 64KB max for fallback
};
```

**Pool Allocation API**:
```cpp
void* allocateAudioBuffer(size_t size, const char* source);
void* allocateNetworkBuffer(size_t size, const char* source);
void* allocateGeneralBuffer(size_t size, const char* source);
void deallocate(void* ptr);  // Auto-detects pool
```

**Static Buffers** (Prevents fragmentation entirely):
```cpp
// I2S read buffer (src/i2s_audio.h)
static int32_t temp_read_buffer[4096];  // 16KB static
```

**Memory Usage Breakdown**:
- Audio Pool: 16KB (pre-allocated)
- Network Pool: 40KB (pre-allocated)
- General Pool: 4KB (pre-allocated)
- Static Buffers: 16KB (I2S)
- **Total**: ~76KB / 200KB available = 38% (acceptable)

## Related Decisions

- ADR-001: Event-Driven Architecture (EventBus uses pools)
- ADR-003: Static Buffer for I2S (uses static allocation)

## References

- `src/utils/MemoryManager.h` - Memory pool interface
- `src/utils/MemoryManager.cpp` - Pool implementation
- `src/config.h` - Memory thresholds configuration
- Doug Lea's Memory Allocator design
- Embedded Systems Memory Management Best Practices
