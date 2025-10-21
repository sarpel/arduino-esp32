#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

// System-wide type definitions to avoid circular dependencies

// System states
enum class SystemState {
    INITIALIZING,
    CONNECTING_WIFI,
    CONNECTING_SERVER,
    CONNECTED,
    DISCONNECTED,
    ERROR,
    MAINTENANCE
};

// System events
enum class SystemEvent {
    // System events
    SYSTEM_STARTUP,
    SYSTEM_SHUTDOWN,
    SYSTEM_ERROR,
    SYSTEM_RECOVERY,
    
    // Audio events
    AUDIO_DATA_AVAILABLE,
    AUDIO_PROCESSING_ERROR,
    AUDIO_QUALITY_DEGRADED,
    
    // Network events
    NETWORK_CONNECTED,
    NETWORK_DISCONNECTED,
    NETWORK_QUALITY_CHANGED,
    SERVER_CONNECTED,
    SERVER_DISCONNECTED,
    
    // Health events
    MEMORY_LOW,
    MEMORY_CRITICAL,
    CPU_OVERLOAD,
    TEMPERATURE_HIGH,
    
    // Configuration events
    CONFIG_CHANGED,
    CONFIG_INVALID,
    PROFILE_LOADED,
    
    // Security events
    SECURITY_BREACH,
    AUTHENTICATION_FAILED,
    ENCRYPTION_ERROR
};

// Event priority levels (avoiding Arduino macro conflicts)
enum class EventPriority {
    CRITICAL_PRIORITY = 0,  // System-critical events (errors, emergencies)
    HIGH_PRIORITY = 1,      // Important events (state changes, connections)
    NORMAL_PRIORITY = 2,    // Regular events (data, status updates)
    LOW_PRIORITY = 3        // Background events (statistics, diagnostics)
};

// Log levels
enum class LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
};

// Log output types (avoiding Arduino macro conflicts)
enum class LogOutputType {
    SERIAL_OUTPUT = 0,
    FILE_OUTPUT = 1,
    NETWORK_OUTPUT = 2,
    SYSLOG_OUTPUT = 3,
    CUSTOM_OUTPUT = 4
};

#endif // SYSTEM_TYPES_H