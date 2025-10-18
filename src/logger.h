#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
};

class Logger {
private:
    static LogLevel min_level;
    static const char* level_names[];

public:
    static void init(LogLevel level = LOG_INFO);
    static void setLevel(LogLevel level) { min_level = level; }
    static void log(LogLevel level, const char* file, int line, const char* fmt, ...);
};

// Convenience macros
#define LOG_DEBUG(fmt, ...)    Logger::log(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)     Logger::log(LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)     Logger::log(LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)    Logger::log(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) Logger::log(LOG_CRITICAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
