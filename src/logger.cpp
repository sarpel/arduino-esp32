#include "logger.h"
#include "config.h"
#include <stdarg.h>

LogLevel Logger::min_level = LOG_INFO;

const char *Logger::level_names[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "CRITICAL"};

static float _logger_tokens = 0.0f;
static uint32_t _logger_last_refill_ms = 0;
static uint32_t _logger_suppressed = 0;

static inline void logger_refill_tokens()
{
    uint32_t now = millis();
    if (_logger_last_refill_ms == 0)
    {
        _logger_last_refill_ms = now;
        _logger_tokens = LOGGER_BURST_MAX;
        return;
    }
    uint32_t elapsed = now - _logger_last_refill_ms;
    if (elapsed == 0)
        return;
    float rate_per_ms = (float)LOGGER_MAX_LINES_PER_SEC / 1000.0f;
    _logger_tokens += elapsed * rate_per_ms;
    if (_logger_tokens > (float)LOGGER_BURST_MAX)
        _logger_tokens = (float)LOGGER_BURST_MAX;
    _logger_last_refill_ms = now;
}

void Logger::init(LogLevel level)
{
    min_level = level;
    Serial.begin(115200);
    delay(1000);
    _logger_tokens = LOGGER_BURST_MAX;
    _logger_last_refill_ms = millis();
    _logger_suppressed = 0;
}

void Logger::log(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    if (level < min_level)
        return;

    logger_refill_tokens();
    if (_logger_tokens < 1.0f)
    {
        // Rate limited: drop message and count it
        _logger_suppressed++;
        return;
    }

    // If there were suppressed messages and we have enough budget, report once
    if (_logger_suppressed > 0 && _logger_tokens >= 2.0f)
    {
        _logger_tokens -= 1.0f;
        Serial.printf("[%6lu] [%-8s] [Heap:%6u] %s (%s:%d)\n",
                      millis() / 1000,
                      "INFO",
                      ESP.getFreeHeap(),
                      "[logger] Suppressed messages due to rate limiting",
                      "logger",
                      0);
        _logger_tokens -= 1.0f;
        Serial.printf("[%6lu] [%-8s] [Heap:%6u] Suppressed count: %u (%s:%d)\n",
                      millis() / 1000,
                      "INFO",
                      ESP.getFreeHeap(),
                      _logger_suppressed,
                      "logger",
                      0);
        _logger_suppressed = 0;
    }

    // BUG FIX: Use fixed-size buffer with overflow protection
    // vsnprintf guarantees null-termination even on truncation
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Check if message was truncated (written >= buffer size)
    if (written >= (int)sizeof(buffer)) {
        // Message was truncated - add truncation indicator
        // Safely overwrite last few chars with "..." to indicate truncation
        buffer[sizeof(buffer) - 4] = '.';
        buffer[sizeof(buffer) - 3] = '.';
        buffer[sizeof(buffer) - 2] = '.';
        buffer[sizeof(buffer) - 1] = '\0';
    }

    // Extract filename from path
    // BUG FIX: Add null pointer check for file parameter
    const char *filename = "unknown";
    if (file != nullptr) {
        const char *slash = strrchr(file, '/');
        if (!slash)
            slash = strrchr(file, '\\');
        filename = slash ? slash + 1 : file;
    }

    _logger_tokens -= 1.0f;
    Serial.printf("[%6lu] [%-8s] [Heap:%6u] %s (%s:%d)\n",
                  millis() / 1000,
                  level_names[level],
                  ESP.getFreeHeap(),
                  buffer,
                  filename,
                  line);
}
