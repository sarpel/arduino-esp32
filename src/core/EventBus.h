#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include "SystemTypes.h"

// Event priority levels (defined in SystemTypes.h to avoid Arduino conflicts)

// Event metadata
struct EventMetadata {
    SystemEvent type;
    EventPriority priority;
    unsigned long timestamp;
    const void* data;
    size_t data_size;
    const char* source_component;
    
    EventMetadata(SystemEvent t, EventPriority p, const void* d = nullptr, 
                  size_t size = 0, const char* source = "unknown")
        : type(t), priority(p), timestamp(millis()), data(d), 
          data_size(size), source_component(source) {}
};

// Event handler function type
typedef std::function<void(const void*)> EventHandler;

// Handler registration info
struct HandlerRegistration {
    EventHandler handler;
    EventPriority max_priority;
    const char* component_name;
    
    HandlerRegistration(EventHandler h, EventPriority max_p, const char* name)
        : handler(h), max_priority(max_p), component_name(name) {}
};

class EventBus {
private:
    // Event handlers organized by event type
    std::map<SystemEvent, std::vector<HandlerRegistration>> handlers;
    
    // Event queue for asynchronous processing
    std::queue<EventMetadata> event_queue;
    
    // Statistics
    struct EventStats {
        uint32_t total_events_published;
        uint32_t total_events_processed;
        uint32_t events_dropped;
        uint32_t handler_errors;
        std::map<SystemEvent, uint32_t> event_type_counts;
        std::map<EventPriority, uint32_t> priority_counts;
        
        EventStats() : total_events_published(0), total_events_processed(0),
                      events_dropped(0), handler_errors(0) {}
    } stats;
    
    // Configuration
    static constexpr size_t MAX_QUEUE_SIZE = 100;
    static constexpr uint32_t MAX_PROCESSING_TIME_MS = 50;
    static constexpr uint32_t EVENT_TIMEOUT_MS = 5000;
    
    // Processing control
    bool initialized;
    bool processing_enabled;
    unsigned long last_processing_time;
    
    // Internal methods
    bool shouldProcessEvent(const EventMetadata& event);
    void processEvent(const EventMetadata& event);
    void dropEvent(const EventMetadata& event);
    void recordEventStats(const EventMetadata& event);
    void handleHandlerError(const EventMetadata& event, const char* error);
    
public:
    EventBus() : initialized(false), processing_enabled(true), last_processing_time(0) {}
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // Event subscription
    bool subscribe(SystemEvent event, EventHandler handler, 
                    EventPriority max_priority = EventPriority::NORMAL_PRIORITY,
                   const char* component_name = "unknown");
    
    bool unsubscribe(SystemEvent event, EventHandler handler);
    void unsubscribeAll(const char* component_name);
    
    // Event publishing
    bool publish(SystemEvent event, const void* data = nullptr, 
                          size_t data_size = 0, EventPriority priority = EventPriority::NORMAL_PRIORITY,
                 const char* source_component = "unknown");
    
    bool publishImmediate(SystemEvent event, const void* data = nullptr,
                         size_t data_size = 0, EventPriority priority = EventPriority::NORMAL_PRIORITY,
                         const char* source_component = "unknown");
    
    // Event processing
    void processEvents();
    void processEvents(uint32_t max_time_ms);
    void clearQueue();
    
    // Queue management
    size_t getQueueSize() const { return event_queue.size(); }
    bool isQueueEmpty() const { return event_queue.empty(); }
    bool isQueueFull() const { return event_queue.size() >= MAX_QUEUE_SIZE; }
    
    // Processing control
    void enableProcessing() { processing_enabled = true; }
    void disableProcessing() { processing_enabled = false; }
    bool isProcessingEnabled() const { return processing_enabled; }
    
    // Statistics
    uint32_t getTotalEventsPublished() const { return stats.total_events_published; }
    uint32_t getTotalEventsProcessed() const { return stats.total_events_processed; }
    uint32_t getEventsDropped() const { return stats.events_dropped; }
    uint32_t getHandlerErrors() const { return stats.handler_errors; }
    uint32_t getEventCount(SystemEvent event) const;
    uint32_t getPriorityCount(EventPriority priority) const;
    
    // Utility
    const char* getEventName(SystemEvent event) const;
    const char* getPriorityName(EventPriority priority) const;
    void printStatistics() const;
    void resetStatistics();
};

// Global event bus access
#define EVENT_BUS() (SystemManager::getInstance().getEventBus())

// Convenience macros for event publishing
#define PUBLISH_EVENT(event, data, priority) \
    EVENT_BUS()->publish(event, data, 0, priority, __FUNCTION__)

#define PUBLISH_EVENT_IMMEDIATE(event, data, priority) \
    EVENT_BUS()->publishImmediate(event, data, 0, priority, __FUNCTION__)

// Convenience macros for event subscription
#define SUBSCRIBE_TO_EVENT(event, handler, priority) \
    EVENT_BUS()->subscribe(event, handler, priority, __FUNCTION__)

#endif // EVENT_BUS_H