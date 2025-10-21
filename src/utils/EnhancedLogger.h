#ifndef ENHANCED_LOGGER_H
#define ENHANCED_LOGGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include <map>
#include <cstdarg>
#include "../core/SystemTypes.h"

// Log output types (defined in SystemTypes.h to avoid Arduino conflicts)

// Log output configuration
struct LogOutputConfig {
    LogOutputType type;
    LogLevel min_level;
    LogLevel max_level;
    bool enabled;
    std::map<String, String> parameters;
    
    LogOutputConfig(LogOutputType t, LogLevel min = LogLevel::LOG_DEBUG, LogLevel max = LogLevel::LOG_CRITICAL)
        : type(t), min_level(min), max_level(max), enabled(true) {}
};

// Log message structure
struct LogMessage {
    LogLevel level;
    const char* component;
    const char* message;
    unsigned long timestamp;
    const char* file;
    int line;
    std::map<String, String> context;
    
    LogMessage(LogLevel lvl, const char* comp, const char* msg, const char* f = nullptr, int ln = 0)
        : level(lvl), component(comp), message(msg), timestamp(millis()),
          file(f), line(ln) {}
};

// Log filter function
typedef std::function<bool(const LogMessage&)> LogFilter;

// Log formatter function
typedef std::function<String(const LogMessage&)> LogFormatter;

class EnhancedLogger {
private:
    // Output configurations
    std::vector<LogOutputConfig> outputs;
    
    // Filters
    std::vector<LogFilter> filters;
    
    // Formatters
    std::map<LogOutputType, LogFormatter> formatters;
    
    // Statistics
    struct LogStats {
        uint32_t messages_logged;
        uint32_t messages_filtered;
        uint32_t messages_dropped;
        std::map<LogLevel, uint32_t> level_counts;
        std::map<LogOutputType, uint32_t> output_counts;
        
        LogStats() : messages_logged(0), messages_filtered(0), messages_dropped(0) {}
    } stats;
    
    // Configuration
    bool initialized;
    bool enable_statistics;
    bool enable_buffering;
    LogLevel global_min_level;
    
    // Rate limiting
    uint32_t max_messages_per_second;
    uint32_t messages_this_second;
    unsigned long last_message_time;
    
    // Buffering
    std::vector<LogMessage> message_buffer;
    static constexpr size_t MAX_BUFFER_SIZE = 100;
    
    // Internal methods
    bool shouldLogMessage(const LogMessage& message) const;
    void processMessage(const LogMessage& message);
    void writeToOutput(const LogMessage& message, const LogOutputConfig& output);
    void writeToSerial(const LogMessage& message);
    void writeToFile(const LogMessage& message, const LogOutputConfig& output);
    void writeToNetwork(const LogMessage& message, const LogOutputConfig& output);
    void writeToSyslog(const LogMessage& message, const LogOutputConfig& output);
    
    // Formatters
    String formatSerial(const LogMessage& message) const;
    String formatFile(const LogMessage& message) const;
    String formatNetwork(const LogMessage& message) const;
    String formatSyslog(const LogMessage& message) const;
    
    // Utility
    const char* getLevelName(LogLevel level) const;
    const char* getOutputName(LogOutputType type) const;
    
public:
    EnhancedLogger();
    ~EnhancedLogger();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // Configuration
    void setGlobalMinLevel(LogLevel level) { global_min_level = level; }
    LogLevel getGlobalMinLevel() const { return global_min_level; }
    void enableStatistics(bool enable) { enable_statistics = enable; }
    void enableBuffering(bool enable) { enable_buffering = enable; }
    void setRateLimit(uint32_t messages_per_second) { max_messages_per_second = messages_per_second; }
    
    // Output management
    void addOutput(const LogOutputConfig& config);
    void removeOutput(LogOutputType type);
    void enableOutput(LogOutputType type, bool enable);
    bool hasOutput(LogOutputType type) const;
    
    // Filter management
    void addFilter(LogFilter filter);
    void clearFilters();
    
    // Formatter management
    void setFormatter(LogOutputType type, LogFormatter formatter);
    void setDefaultFormatters();
    
    // Logging methods
    void log(LogLevel level, const char* component, const char* file, int line, const char* format, ...);
    void logMessage(const LogMessage& message);
    void logBuffer(const uint8_t* buffer, size_t size, LogLevel level, const char* component);
    
    // Context management
    void setContext(const String& key, const String& value);
    void clearContext();
    void removeContext(const String& key);
    
    // Buffer management
    void flushBuffer();
    void clearBuffer();
    size_t getBufferSize() const { return message_buffer.size(); }
    
    // Statistics
    const LogStats& getStatistics() const { return stats; }
    void resetStatistics();
    void printStatistics() const;
    
    // Utility
    void printOutputs() const;
    void printFilters() const;
    bool isRateLimited() const;
    
    // Convenience methods for different log levels
    void debug(const char* component, const char* format, ...);
    void info(const char* component, const char* format, ...);
    void warn(const char* component, const char* format, ...);
    void error(const char* component, const char* format, ...);
    void critical(const char* component, const char* format, ...);
};

// Global logger access
#define ENHANCED_LOGGER() (SystemManager::getInstance().getLogger())

// Convenience macros
#define LOG_DEBUG_COMP(component, fmt, ...) \
    ENHANCED_LOGGER()->log(LOG_DEBUG, component, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO_COMP(component, fmt, ...) \
    ENHANCED_LOGGER()->log(LOG_INFO, component, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN_COMP(component, fmt, ...) \
    ENHANCED_LOGGER()->log(LOG_WARN, component, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR_COMP(component, fmt, ...) \
    ENHANCED_LOGGER()->log(LOG_ERROR, component, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_CRITICAL_COMP(component, fmt, ...) \
    ENHANCED_LOGGER()->log(LOG_CRITICAL, component, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // ENHANCED_LOGGER_H