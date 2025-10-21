#include "TelemetryCollector.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"

TelemetryCollector::TelemetryCollector() : write_index(0), event_count(0), start_time(millis()) {
    events.reserve(MAX_EVENTS);
}

void TelemetryCollector::recordEvent(EventSeverity severity, const String& component, const String& message, uint32_t error_code) {
    TelemetryEvent event(severity, component, message, error_code);

    if (events.size() >= MAX_EVENTS) {
        // Overwrite oldest event (circular buffer)
        if (write_index >= MAX_EVENTS) {
            write_index = 0;
        }
        if (write_index < events.size()) {
            events[write_index] = event;
        } else {
            events.push_back(event);
        }
        write_index = (write_index + 1) % MAX_EVENTS;
    } else {
        events.push_back(event);
    }

    event_count++;

    // Also log to logger
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        LogLevel level = LogLevel::LOG_INFO;
        switch (severity) {
            case EventSeverity::DEBUG:
                level = LogLevel::LOG_DEBUG;
                break;
            case EventSeverity::WARNING:
                level = LogLevel::LOG_WARN;
                break;
            case EventSeverity::ERROR:
            case EventSeverity::CRITICAL:
                level = LogLevel::LOG_ERROR;
                break;
            default:
                break;
        }

        logger->log(level, component.c_str(), __FILE__, __LINE__, "%s (code: %u)", message.c_str(), error_code);
    }
}

void TelemetryCollector::recordCriticalEvent(const String& component, const String& message) {
    recordEvent(EventSeverity::CRITICAL, component, message);
}

void TelemetryCollector::recordError(const String& component, const String& message) {
    recordEvent(EventSeverity::ERROR, component, message);
}

void TelemetryCollector::recordWarning(const String& component, const String& message) {
    recordEvent(EventSeverity::WARNING, component, message);
}

void TelemetryCollector::recordInfo(const String& component, const String& message) {
    recordEvent(EventSeverity::INFO, component, message);
}

std::vector<TelemetryEvent> TelemetryCollector::getEventsByComponent(const String& component) const {
    std::vector<TelemetryEvent> result;
    for (const auto& event : events) {
        if (event.component == component) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<TelemetryEvent> TelemetryCollector::getEventsBySeverity(EventSeverity severity) const {
    std::vector<TelemetryEvent> result;
    for (const auto& event : events) {
        if (event.severity == severity) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<TelemetryEvent> TelemetryCollector::getRecentEvents(size_t count) const {
    std::vector<TelemetryEvent> result;
    size_t start = events.size() > count ? events.size() - count : 0;
    for (size_t i = start; i < events.size(); i++) {
        result.push_back(events[i]);
    }
    return result;
}

void TelemetryCollector::clear() {
    events.clear();
    write_index = 0;
    event_count = 0;
}

void TelemetryCollector::printAllEvents() const {
    printRecentEvents(events.size());
}

void TelemetryCollector::printRecentEvents(size_t count) const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "TelemetryCollector", __FILE__, __LINE__,
                "=== Telemetry Events (showing %u of %u) ===", static_cast<unsigned>(count), static_cast<unsigned>(events.size()));

    std::vector<TelemetryEvent> recent = getRecentEvents(count);
    for (size_t i = 0; i < recent.size(); i++) {
        const auto& evt = recent[i];
        logger->log(LogLevel::LOG_INFO, "TelemetryCollector", __FILE__, __LINE__,
                    "[%s] %s: %s (code: %u, t+%lums)",
                    severityToString(evt.severity).c_str(), evt.component.c_str(),
                    evt.message.c_str(), evt.error_code, evt.timestamp - start_time);
    }

    logger->log(LogLevel::LOG_INFO, "TelemetryCollector", __FILE__, __LINE__,
                "======================================");
}

size_t TelemetryCollector::getCriticalEventCount() const {
    return getEventsBySeverity(EventSeverity::CRITICAL).size();
}

size_t TelemetryCollector::getErrorEventCount() const {
    return getEventsBySeverity(EventSeverity::ERROR).size();
}

String TelemetryCollector::severityToString(EventSeverity severity) const {
    switch (severity) {
        case EventSeverity::DEBUG:
            return "DEBUG";
        case EventSeverity::INFO:
            return "INFO";
        case EventSeverity::WARNING:
            return "WARN";
        case EventSeverity::ERROR:
            return "ERROR";
        case EventSeverity::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}
