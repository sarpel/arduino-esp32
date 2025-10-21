#include "EnhancedLogger.h"
#include <cstdarg>
#include <cstring>

EnhancedLogger::EnhancedLogger()
    : initialized(false), enable_statistics(true), enable_buffering(false),
      global_min_level(LOG_INFO), max_messages_per_second(100),
      messages_this_second(0), last_message_time(0) {}

EnhancedLogger::~EnhancedLogger() {
    shutdown();
}

bool EnhancedLogger::initialize() {
    if (initialized) {
        return true;
    }
    
    // Initialize serial output by default
    LogOutputConfig serial_config(LogOutputType::SERIAL, LOG_DEBUG, LOG_CRITICAL);
    addOutput(serial_config);
    
    // Set default formatters
    setDefaultFormatters();
    
    // Reset statistics
    resetStatistics();
    
    initialized = true;
    
    // Log initialization
    log(LOG_INFO, "EnhancedLogger", __FILE__, __LINE__, "EnhancedLogger initialized");
    
    return true;
}

void EnhancedLogger::shutdown() {
    if (!initialized) {
        return;
    }
    
    // Flush any buffered messages
    flushBuffer();
    
    // Log shutdown
    log(LOG_INFO, "EnhancedLogger", __FILE__, __LINE__, "EnhancedLogger shutting down");
    printStatistics();
    
    // Clear outputs and filters
    outputs.clear();
    filters.clear();
    formatters.clear();
    message_buffer.clear();
    
    initialized = false;
}

void EnhancedLogger::addOutput(const LogOutputConfig& config) {
    // Remove existing output of same type
    removeOutput(config.type);
    
    outputs.push_back(config);
    
    // Log new output
    log(LOG_INFO, "EnhancedLogger", __FILE__, __LINE__, "Added output: %s", 
        getOutputName(config.type));
}

void EnhancedLogger::removeOutput(LogOutputType type) {
    outputs.erase(
        std::remove_if(outputs.begin(), outputs.end(),
            [type](const LogOutputConfig& config) { return config.type == type; }),
        outputs.end()
    );
}

void EnhancedLogger::enableOutput(LogOutputType type, bool enable) {
    for (auto& output : outputs) {
        if (output.type == type) {
            output.enabled = enable;
            log(LOG_INFO, "EnhancedLogger", __FILE__, __LINE__, "%s output %s",
                getOutputName(type), enable ? "enabled" : "disabled");
            break;
        }
    }
}

bool EnhancedLogger::hasOutput(LogOutputType type) const {
    for (const auto& output : outputs) {
        if (output.type == type && output.enabled) {
            return true;
        }
    }
    return false;
}

void EnhancedLogger::addFilter(LogFilter filter) {
    filters.push_back(filter);
}

void EnhancedLogger::clearFilters() {
    filters.clear();
}

void EnhancedLogger::setFormatter(LogOutputType type, LogFormatter formatter) {
    formatters[type] = formatter;
}

void EnhancedLogger::setDefaultFormatters() {
    formatters[LogOutputType::SERIAL] = [this](const LogMessage& msg) { return formatSerial(msg); };
    formatters[LogOutputType::FILE] = [this](const LogMessage& msg) { return formatFile(msg); };
    formatters[LogOutputType::NETWORK] = [this](const LogMessage& msg) { return formatNetwork(msg); };
    formatters[LogOutputType::SYSLOG] = [this](const LogMessage& msg) { return formatSyslog(msg); };
}

void EnhancedLogger::log(LogLevel level, const char* component, const char* file, int line, const char* format, ...) {
    if (!initialized || level < global_min_level) {
        return;
    }
    
    // Check rate limiting
    if (isRateLimited()) {
        stats.messages_dropped++;
        return;
    }
    
    // Format the message
    char message_buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);
    
    // Create log message
    LogMessage message(level, component, message_buffer, file, line);
    
    // Add context
    // Note: Context management would be implemented here
    
    // Process the message
    logMessage(message);
}

void EnhancedLogger::logMessage(const LogMessage& message) {
    if (!shouldLogMessage(message)) {
        stats.messages_filtered++;
        return;
    }
    
    // Buffer message if buffering is enabled
    if (enable_buffering) {
        message_buffer.push_back(message);
        if (message_buffer.size() >= MAX_BUFFER_SIZE) {
            flushBuffer();
        }
        return;
    }
    
    // Process message immediately
    processMessage(message);
}

bool EnhancedLogger::shouldLogMessage(const LogMessage& message) const {
    // Check global minimum level
    if (message.level < global_min_level) {
        return false;
    }
    
    // Apply filters
    for (const auto& filter : filters) {
        if (!filter(message)) {
            return false;
        }
    }
    
    return true;
}

void EnhancedLogger::processMessage(const LogMessage& message) {
    stats.messages_logged++;
    stats.level_counts[message.level]++;
    
    // Process for each enabled output
    for (const auto& output : outputs) {
        if (output.enabled && message.level >= output.min_level && message.level <= output.max_level) {
            writeToOutput(message, output);
            stats.output_counts[output.type]++;
        }
    }
}

void EnhancedLogger::writeToOutput(const LogMessage& message, const LogOutputConfig& output) {
    switch (output.type) {
        case LogOutputType::SERIAL:
            writeToSerial(message);
            break;
        case LogOutputType::FILE:
            writeToFile(message, output);
            break;
        case LogOutputType::NETWORK:
            writeToNetwork(message, output);
            break;
        case LogOutputType::SYSLOG:
            writeToSyslog(message, output);
            break;
        case LogOutputType::CUSTOM:
            // Custom output handling would go here
            break;
    }
}

void EnhancedLogger::writeToSerial(const LogMessage& message) {
    String formatted = formatSerial(message);
    Serial.println(formatted);
}

void EnhancedLogger::writeToFile(const LogMessage& message, const LogOutputConfig& output) {
    // File output implementation would go here
    // For now, just format and potentially buffer
    String formatted = formatFile(message);
    // Would write to file system
}

void EnhancedLogger::writeToNetwork(const LogMessage& message, const LogOutputConfig& output) {
    // Network output implementation would go here
    // For now, just format
    String formatted = formatNetwork(message);
    // Would send over network
}

void EnhancedLogger::writeToSyslog(const LogMessage& message, const LogOutputConfig& output) {
    // Syslog output implementation would go here
    // For now, just format
    String formatted = formatSyslog(message);
    // Would send to syslog server
}

String EnhancedLogger::formatSerial(const LogMessage& message) const {
    char timestamp[16];
    snprintf(timestamp, sizeof(timestamp), "[%06lu]", message.timestamp % 1000000);
    
    return String(timestamp) + "[" + getLevelName(message.level) + "][" + 
           message.component + "] " + message.message;
}

String EnhancedLogger::formatFile(const LogMessage& message) const {
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lu", message.timestamp);
    
    return String(timestamp) + "," + getLevelName(message.level) + "," + 
           message.component + "," + message.message;
}

String EnhancedLogger::formatNetwork(const LogMessage& message) const {
    // JSON format for network transmission
    String json = "{";
    json += "\"timestamp\":" + String(message.timestamp) + ",";
    json += "\"level\":\"" + String(getLevelName(message.level)) + "\",";
    json += "\"component\":\"" + String(message.component) + "\",";
    json += "\"message\":\"" + String(message.message) + "\",";
    if (message.file) {
        json += "\"file\":\"" + String(message.file) + "\",";
        json += "\"line\":" + String(message.line);
    }
    json += "}";
    
    return json;
}

String EnhancedLogger::formatSyslog(const LogMessage& message) const {
    // RFC 5424 syslog format
    int priority = static_cast<int>(message.level) * 8 + 16;  // Local use
    
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lu", message.timestamp);
    
    return String("<") + priority + String(">1 ") + timestamp + String(" ESP32AudioStreamer ") + 
           message.component + String(" - - - ") + message.message;
}

void EnhancedLogger::logBuffer(const uint8_t* buffer, size_t size, LogLevel level, const char* component) {
    if (!initialized || level < global_min_level) {
        return;
    }
    
    // Format buffer as hex string
    String hex_string;
    for (size_t i = 0; i < size && i < 32; i++) {  // Limit to first 32 bytes
        if (i > 0) hex_string += " ";
        if (buffer[i] < 16) hex_string += "0";
        hex_string += String(buffer[i], HEX);
    }
    if (size > 32) {
        hex_string += "...";
    }
    
    log(level, component, __FILE__, __LINE__, "Buffer[%u]: %s", size, hex_string.c_str());
}

void EnhancedLogger::setContext(const String& key, const String& value) {
    // Context management would be implemented here
    // For now, this is a placeholder
}

void EnhancedLogger::clearContext() {
    // Context management would be implemented here
}

void EnhancedLogger::removeContext(const String& key) {
    // Context management would be implemented here
}

void EnhancedLogger::flushBuffer() {
    if (!enable_buffering) {
        return;
    }
    
    // Process all buffered messages
    for (const auto& message : message_buffer) {
        processMessage(message);
    }
    
    message_buffer.clear();
}

void EnhancedLogger::clearBuffer() {
    message_buffer.clear();
}

void EnhancedLogger::resetStatistics() {
    stats = LogStats();
}

void EnhancedLogger::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "EnhancedLogger", "=== Logger Statistics ===");
    logger->log(LOG_INFO, "EnhancedLogger", "Messages logged: %u", stats.messages_logged);
    logger->log(LOG_INFO, "EnhancedLogger", "Messages filtered: %u", stats.messages_filtered);
    logger->log(LOG_INFO, "EnhancedLogger", "Messages dropped: %u", stats.messages_dropped);
    
    logger->log(LOG_INFO, "EnhancedLogger", "--- Level Counts ---");
    for (const auto& pair : stats.level_counts) {
        logger->log(LOG_INFO, "EnhancedLogger", "%s: %u", getLevelName(pair.first), pair.second);
    }
    
    logger->log(LOG_INFO, "EnhancedLogger", "--- Output Counts ---");
    for (const auto& pair : stats.output_counts) {
        logger->log(LOG_INFO, "EnhancedLogger", "%s: %u", getOutputName(pair.first), pair.second);
    }
    
    logger->log(LOG_INFO, "EnhancedLogger", "======================");
}

void EnhancedLogger::printOutputs() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "EnhancedLogger", "=== Logger Outputs ===");
    for (const auto& output : outputs) {
        logger->log(LOG_INFO, "EnhancedLogger", "%s: %s (min: %s, max: %s)",
                   getOutputName(output.type), output.enabled ? "enabled" : "disabled",
                   getLevelName(output.min_level), getLevelName(output.max_level));
    }
    logger->log(LOG_INFO, "EnhancedLogger", "=====================");
}

void EnhancedLogger::printFilters() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "EnhancedLogger", "=== Logger Filters ===");
    logger->log(LOG_INFO, "EnhancedLogger", "Active filters: %u", filters.size());
    logger->log(LOG_INFO, "EnhancedLogger", "=====================");
}

bool EnhancedLogger::isRateLimited() const {
    unsigned long current_time = millis();
    
    // Reset counter if new second
    if (current_time - last_message_time >= 1000) {
        messages_this_second = 0;
        last_message_time = current_time;
    }
    
    return messages_this_second >= max_messages_per_second;
}

const char* EnhancedLogger::getLevelName(LogLevel level) const {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARN: return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* EnhancedLogger::getOutputName(LogOutputType type) const {
    switch (type) {
        case LogOutputType::SERIAL: return "SERIAL";
        case LogOutputType::FILE: return "FILE";
        case LogOutputType::NETWORK: return "NETWORK";
        case LogOutputType::SYSLOG: return "SYSLOG";
        case LogOutputType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

// Convenience methods
void EnhancedLogger::debug(const char* component, const char* format, ...) {
    char message_buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);
    
    log(LOG_DEBUG, component, __FILE__, __LINE__, "%s", message_buffer);
}

void EnhancedLogger::info(const char* component, const char* format, ...) {
    char message_buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);
    
    log(LOG_INFO, component, __FILE__, __LINE__, "%s", message_buffer);
}

void EnhancedLogger::warn(const char* component, const char* format, ...) {
    char message_buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);
    
    log(LOG_WARN, component, __FILE__, __LINE__, "%s", message_buffer);
}

void EnhancedLogger::error(const char* component, const char* format, ...) {
    char message_buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);
    
    log(LOG_ERROR, component, __FILE__, __LINE__, "%s", message_buffer);
}

void EnhancedLogger::critical(const char* component, const char* format, ...) {
    char message_buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);
    
    log(LOG_CRITICAL, component, __FILE__, __LINE__, "%s", message_buffer);
}