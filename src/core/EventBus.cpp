#include "EventBus.h"
#include "../utils/EnhancedLogger.h"
#include "SystemManager.h"

bool EventBus::initialize() {
    if (initialized) {
        return true;
    }
    
    // Clear any existing handlers
    handlers.clear();
    
    // Clear event queue
    clearQueue();
    
    // Reset statistics
    resetStatistics();
    
    // Enable processing
    processing_enabled = true;
    last_processing_time = millis();
    
    initialized = true;
    
    // Log initialization
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "EventBus initialized - max queue size: %u", MAX_QUEUE_SIZE);
    }
    
    return true;
}

void EventBus::shutdown() {
    if (!initialized) {
        return;
    }
    
    // Log shutdown statistics
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "EventBus shutting down - processed: %u, dropped: %u, errors: %u",
                   stats.total_events_processed, stats.events_dropped, stats.handler_errors);
    }
    
    // Clear all handlers
    handlers.clear();
    
    // Clear event queue
    clearQueue();
    
    // Disable processing
    processing_enabled = false;
    
    initialized = false;
}

bool EventBus::subscribe(SystemEvent event, EventHandler handler, 
                        EventPriority max_priority, const char* component_name) {
    if (!initialized) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_ERROR, "EventBus", __FILE__, __LINE__, "Cannot subscribe - EventBus not initialized");
        }
        return false;
    }
    
    if (!handler) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_ERROR, "EventBus", __FILE__, __LINE__, "Cannot subscribe - invalid handler");
        }
        return false;
    }
    
    // Add handler to the event's handler list
    handlers[event].emplace_back(handler, max_priority, component_name);
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_DEBUG, "EventBus", __FILE__, __LINE__, "Subscribed %s to event %s (priority: %s)",
                   component_name, getEventName(event), getPriorityName(max_priority));
    }
    
    return true;
}

bool EventBus::unsubscribe(SystemEvent event, EventHandler handler) {
    if (!initialized) {
        return false;
    }
    
    auto it = handlers.find(event);
    if (it == handlers.end()) {
        return false;
    }
    
    auto& event_handlers = it->second;
    auto handler_it = std::find_if(event_handlers.begin(), event_handlers.end(),
        [&handler](const HandlerRegistration& reg) {
            return true;
        });
    
    if (handler_it != event_handlers.end()) {
        event_handlers.erase(handler_it);
        return true;
    }
    
    return false;
}

void EventBus::unsubscribeAll(const char* component_name) {
    if (!initialized) {
        return;
    }
    
    for (auto& event_pair : handlers) {
        auto& event_handlers = event_pair.second;
        event_handlers.erase(
            std::remove_if(event_handlers.begin(), event_handlers.end(),
                [component_name](const HandlerRegistration& reg) {
                    return strcmp(reg.component_name, component_name) == 0;
                }),
            event_handlers.end()
        );
    }
}

bool EventBus::publish(SystemEvent event, const void* data, 
                      size_t data_size, EventPriority priority, const char* source_component) {
    if (!initialized) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_ERROR, "EventBus", __FILE__, __LINE__, "Cannot publish - EventBus not initialized");
        }
        return false;
    }
    
    // Check if queue is full
    if (isQueueFull()) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_WARN, "EventBus", __FILE__, __LINE__, "Event queue full - dropping event %s from %s",
                       getEventName(event), source_component);
        }
        stats.events_dropped++;
        return false;
    }
    
    // Create event metadata
    EventMetadata metadata(event, priority, data, data_size, source_component);
    
    // Add to queue
    event_queue.push(metadata);
    
    // Update statistics
    recordEventStats(metadata);
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger && priority <= EventPriority::HIGH_PRIORITY) {
        logger->log(LogLevel::LOG_DEBUG, "EventBus", __FILE__, __LINE__, "Queued event %s from %s (priority: %s, queue: %u)",
                   getEventName(event), source_component, getPriorityName(priority), 
                   event_queue.size());
    }
    
    return true;
}

bool EventBus::publishImmediate(SystemEvent event, const void* data,
                               size_t data_size, EventPriority priority, const char* source_component) {
    if (!initialized) {
        return false;
    }
    
    // Create event metadata
    EventMetadata metadata(event, priority, data, data_size, source_component);
    
    // Process immediately
    processEvent(metadata);
    
    // Update statistics
    recordEventStats(metadata);
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger && priority <= EventPriority::HIGH_PRIORITY) {
        logger->log(LogLevel::LOG_DEBUG, "EventBus", __FILE__, __LINE__, "Processed immediate event %s from %s (priority: %s)",
                   getEventName(event), source_component, getPriorityName(priority));
    }
    
    return true;
}

void EventBus::processEvents() {
    processEvents(MAX_PROCESSING_TIME_MS);
}

void EventBus::processEvents(uint32_t max_time_ms) {
    if (!initialized || !processing_enabled) {
        return;
    }
    
    unsigned long start_time = millis();
    uint32_t processed_count = 0;
    
    while (!event_queue.empty() && (millis() - start_time) < max_time_ms) {
        EventMetadata event = event_queue.front();
        event_queue.pop();
        
        // Check if event has timed out
        if (shouldProcessEvent(event)) {
            processEvent(event);
            processed_count++;
        } else {
            dropEvent(event);
        }
    }
    
    if (processed_count > 0) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_DEBUG, "EventBus", __FILE__, __LINE__, "Processed %u events in %lu ms (queue: %u)",
                       processed_count, millis() - start_time, event_queue.size());
        }
    }
    
    last_processing_time = millis();
}

bool EventBus::shouldProcessEvent(const EventMetadata& event) {
    // Check for timeout
    if (millis() - event.timestamp > EVENT_TIMEOUT_MS) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_WARN, "EventBus", __FILE__, __LINE__, "Event %s timed out after %lu ms",
                       getEventName(event.type), millis() - event.timestamp);
        }
        return false;
    }
    
    return true;
}

void EventBus::processEvent(const EventMetadata& event) {
    auto it = handlers.find(event.type);
    if (it == handlers.end()) {
        // No handlers for this event
        return;
    }
    
    auto& event_handlers = it->second;
    uint32_t handlers_called = 0;
    uint32_t handlers_failed = 0;
    
    // Call all registered handlers for this event
    for (const auto& registration : event_handlers) {
        // Check if handler should be called based on priority
        if (static_cast<int>(registration.max_priority) <= static_cast<int>(event.priority)) {
            try {
                registration.handler(event.data);
                handlers_called++;
            } catch (...) {
                handlers_failed++;
                handleHandlerError(event, "Handler exception");
            }
        }
    }
    
    stats.total_events_processed++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger && event.priority <= EventPriority::HIGH_PRIORITY) {
        logger->log(LogLevel::LOG_DEBUG, "EventBus", __FILE__, __LINE__, "Event %s processed: %u handlers called, %u failed",
                   getEventName(event.type), handlers_called, handlers_failed);
    }
}

void EventBus::dropEvent(const EventMetadata& event) {
    stats.events_dropped++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_WARN, "EventBus", __FILE__, __LINE__, "Dropped event %s from %s (timeout: %lu ms)",
                   getEventName(event.type), event.source_component, 
                   millis() - event.timestamp);
    }
}

void EventBus::recordEventStats(const EventMetadata& event) {
    stats.total_events_published++;
    stats.event_type_counts[event.type]++;
    stats.priority_counts[event.priority]++;
}

void EventBus::handleHandlerError(const EventMetadata& event, const char* error) {
    stats.handler_errors++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_ERROR, "EventBus", __FILE__, __LINE__, "Handler error for event %s: %s",
                   getEventName(event.type), error);
    }
}

void EventBus::clearQueue() {
    while (!event_queue.empty()) {
        event_queue.pop();
    }
}

uint32_t EventBus::getEventCount(SystemEvent event) const {
    auto it = stats.event_type_counts.find(event);
    return (it != stats.event_type_counts.end()) ? it->second : 0;
}

uint32_t EventBus::getPriorityCount(EventPriority priority) const {
    auto it = stats.priority_counts.find(priority);
    return (it != stats.priority_counts.end()) ? it->second : 0;
}

const char* EventBus::getEventName(SystemEvent event) const {
    switch (event) {
        case SystemEvent::SYSTEM_STARTUP: return "SYSTEM_STARTUP";
        case SystemEvent::SYSTEM_SHUTDOWN: return "SYSTEM_SHUTDOWN";
        case SystemEvent::SYSTEM_ERROR: return "SYSTEM_ERROR";
        case SystemEvent::SYSTEM_RECOVERY: return "SYSTEM_RECOVERY";
        case SystemEvent::AUDIO_DATA_AVAILABLE: return "AUDIO_DATA_AVAILABLE";
        case SystemEvent::AUDIO_PROCESSING_ERROR: return "AUDIO_PROCESSING_ERROR";
        case SystemEvent::AUDIO_QUALITY_DEGRADED: return "AUDIO_QUALITY_DEGRADED";
        case SystemEvent::NETWORK_CONNECTED: return "NETWORK_CONNECTED";
        case SystemEvent::NETWORK_DISCONNECTED: return "NETWORK_DISCONNECTED";
        case SystemEvent::NETWORK_QUALITY_CHANGED: return "NETWORK_QUALITY_CHANGED";
        case SystemEvent::SERVER_CONNECTED: return "SERVER_CONNECTED";
        case SystemEvent::SERVER_DISCONNECTED: return "SERVER_DISCONNECTED";
        case SystemEvent::MEMORY_LOW: return "MEMORY_LOW";
        case SystemEvent::MEMORY_CRITICAL: return "MEMORY_CRITICAL";
        case SystemEvent::CPU_OVERLOAD: return "CPU_OVERLOAD";
        case SystemEvent::TEMPERATURE_HIGH: return "TEMPERATURE_HIGH";
        case SystemEvent::CONFIG_CHANGED: return "CONFIG_CHANGED";
        case SystemEvent::CONFIG_INVALID: return "CONFIG_INVALID";
        case SystemEvent::PROFILE_LOADED: return "PROFILE_LOADED";
        case SystemEvent::SECURITY_BREACH: return "SECURITY_BREACH";
        case SystemEvent::AUTHENTICATION_FAILED: return "AUTHENTICATION_FAILED";
        case SystemEvent::ENCRYPTION_ERROR: return "ENCRYPTION_ERROR";
        default: return "UNKNOWN_EVENT";
    }
}

const char* EventBus::getPriorityName(EventPriority priority) const {
    switch (priority) {
        case EventPriority::CRITICAL_PRIORITY: return "CRITICAL";
        case EventPriority::HIGH_PRIORITY: return "HIGH";
        case EventPriority::NORMAL_PRIORITY: return "NORMAL";
        case EventPriority::LOW_PRIORITY: return "LOW";
        default: return "UNKNOWN_PRIORITY";
    }
}

void EventBus::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "=== EventBus Statistics ===");
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "Total published: %u", stats.total_events_published);
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "Total processed: %u", stats.total_events_processed);
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "Dropped: %u", stats.events_dropped);
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "Handler errors: %u", stats.handler_errors);
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "Current queue size: %u", event_queue.size());
    
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "--- Event Type Counts ---");
    for (const auto& pair : stats.event_type_counts) {
        logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "%s: %u", getEventName(pair.first), pair.second);
    }
    
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "--- Priority Counts ---");
    for (const auto& pair : stats.priority_counts) {
        logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "%s: %u", getPriorityName(pair.first), pair.second);
    }
    
    logger->log(LogLevel::LOG_INFO, "EventBus", __FILE__, __LINE__, "========================");
}

void EventBus::resetStatistics() {
    stats = EventStats();
}