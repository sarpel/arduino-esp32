#ifndef TELEMETRY_COLLECTOR_H
#define TELEMETRY_COLLECTOR_H

#include <Arduino.h>
#include <vector>
#include "../config.h"

// Event severity levels
enum class EventSeverity {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

// Single telemetry event
struct TelemetryEvent {
    EventSeverity severity;
    unsigned long timestamp;
    String component;
    String message;
    uint32_t error_code;

    TelemetryEvent(EventSeverity sev, const String& comp, const String& msg, uint32_t code = 0)
        : severity(sev), timestamp(millis()), component(comp), message(msg), error_code(code) {}
};

// Circular buffer for telemetry events (~1KB, ~50 events)
class TelemetryCollector {
private:
    static constexpr size_t MAX_EVENTS = 50;
    static constexpr size_t MAX_MESSAGE_LENGTH = 64;

    std::vector<TelemetryEvent> events;
    size_t write_index;
    size_t event_count;
    unsigned long start_time;

public:
    TelemetryCollector();

    // Record events
    void recordEvent(EventSeverity severity, const String& component, const String& message, uint32_t error_code = 0);
    void recordCriticalEvent(const String& component, const String& message);
    void recordError(const String& component, const String& message);
    void recordWarning(const String& component, const String& message);
    void recordInfo(const String& component, const String& message);

    // Query events
    const std::vector<TelemetryEvent>& getEvents() const { return events; }
    size_t getEventCount() const { return event_count; }
    size_t getTotalCapacity() const { return MAX_EVENTS; }

    // Filter and search
    std::vector<TelemetryEvent> getEventsByComponent(const String& component) const;
    std::vector<TelemetryEvent> getEventsBySeverity(EventSeverity severity) const;
    std::vector<TelemetryEvent> getRecentEvents(size_t count) const;

    // Management
    void clear();
    void printAllEvents() const;
    void printRecentEvents(size_t count = 10) const;

    // Statistics
    size_t getCriticalEventCount() const;
    size_t getErrorEventCount() const;

private:
    String severityToString(EventSeverity severity) const;
};

#endif // TELEMETRY_COLLECTOR_H
