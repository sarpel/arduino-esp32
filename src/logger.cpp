#include "logger.h"
#include <stdarg.h>

LogLevel Logger::min_level = LOG_INFO;

const char* Logger::level_names[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "CRITICAL"
};

void Logger::init(LogLevel level) {
    min_level = level;
    Serial.begin(115200);
    delay(1000);
}

void Logger::log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < min_level) return;

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Extract filename from path
    const char* filename = strrchr(file, '/');
    if (!filename) filename = strrchr(file, '\\');
    filename = filename ? filename + 1 : file;

    Serial.printf("[%6lu] [%-8s] [Heap:%6u] %s (%s:%d)\n",
                  millis() / 1000,
                  level_names[level],
                  ESP.getFreeHeap(),
                  buffer,
                  filename,
                  line);
}
