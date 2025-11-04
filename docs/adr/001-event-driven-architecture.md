# ADR-001: Event-Driven Architecture with EventBus

**Status**: Accepted
**Date**: 2025-11-01
**Deciders**: System Architect
**Technical Story**: Modular architecture refactoring (v3.0)

## Context

The ESP32 Audio Streamer needed to evolve from a monolithic design to a modular architecture that would support:
- Loose coupling between components
- Asynchronous communication
- Extensibility for new features
- Clear separation of concerns
- Testability of individual components

The system has multiple components (audio processing, network management, health monitoring) that need to communicate without tight dependencies.

## Decision

We will implement an **Event-Driven Architecture** using a custom EventBus with publish-subscribe pattern.

Key design elements:
- Central EventBus for all system events
- Priority-based event handling (CRITICAL, HIGH, NORMAL, LOW)
- Synchronous and asynchronous event delivery options
- Type-safe event payloads
- Component-based event subscriptions

## Rationale

1. **Decoupling**: Components can communicate without direct references
2. **Extensibility**: New components can subscribe to existing events
3. **Testability**: Components can be tested in isolation with mock event bus
4. **Performance**: Priority queues ensure critical events are processed first
5. **Reliability**: Event queuing prevents event loss during high load

## Consequences

### Positive

- Components are loosely coupled and independently testable
- Easy to add new features without modifying existing code
- Clear event flow makes system behavior easier to understand
- Priority handling ensures critical operations (memory warnings, errors) are processed immediately
- Event statistics provide observability into system behavior

### Negative

- Additional memory overhead for event queue (~2-4KB)
- Slight latency increase for event-based communication vs direct calls
- Learning curve for developers unfamiliar with event-driven patterns
- Debugging event flows can be more complex than direct function calls

### Neutral

- Requires careful event naming conventions
- Event documentation becomes critical
- Need to monitor event queue depth to prevent overflow

## Alternatives Considered

### Alternative 1: Direct Function Calls

Traditional approach with components calling each other directly.

**Pros**:
- Simple and straightforward
- No memory overhead
- Easier debugging

**Cons**:
- Tight coupling between components
- Hard to extend without modifying existing code
- Difficult to test in isolation

**Reason for rejection**: Does not scale for modular architecture goals

### Alternative 2: Message Queue (FreeRTOS Queues)

Use FreeRTOS native message queues for inter-component communication.

**Pros**:
- Battle-tested FreeRTOS implementation
- Thread-safe by design
- Well-documented

**Cons**:
- Less type-safe than custom solution
- No priority handling without multiple queues
- Harder to implement pub-sub pattern
- More memory overhead per queue

**Reason for rejection**: Custom EventBus provides better type safety and priority handling

### Alternative 3: Observer Pattern (Direct)

Classic observer pattern with components registering callbacks.

**Pros**:
- Well-known pattern
- Direct callback invocation
- Lower overhead

**Cons**:
- No event queuing (synchronous only)
- No priority handling
- Harder to implement asynchronous delivery
- Component lifecycle management more complex

**Reason for rejection**: Lacks async support and priority features needed for reliability

## Implementation Notes

**EventBus Core** (`src/core/EventBus.h/cpp`):
```cpp
class EventBus {
    void subscribe(SystemEvent event, EventCallback callback,
                   EventPriority priority, const char* subscriber_id);
    void publish(SystemEvent event, const void* data = nullptr,
                 bool immediate = false);
    void processEvents(uint32_t max_events = 0);
};
```

**Event Priority Levels**:
- CRITICAL_PRIORITY: Memory warnings, system errors (immediate)
- HIGH_PRIORITY: Network events, audio quality issues
- NORMAL_PRIORITY: Status updates, statistics
- LOW_PRIORITY: Non-critical notifications

**Memory Usage**:
- Event queue: 32 events × ~64 bytes = 2KB
- Subscriber registry: ~1-2KB
- Total: ~3-4KB overhead

## Related Decisions

- ADR-002: Memory Pool Strategy (manages EventBus memory)
- ADR-005: Circuit Breaker Pattern (uses EventBus for state changes)

## References

- `src/core/EventBus.h` - EventBus interface
- `src/core/EventBus.cpp` - EventBus implementation
- `src/core/SystemTypes.h` - Event type definitions
- Martin Fowler's Event-Driven Architecture patterns
