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

    // Initialize Serial port (works for both USB CDC and UART modes)
    Serial.begin(115200);

#if ARDUINO_USB_CDC_ON_BOOT
    // ESP32-S3 USB CDC: Wait for USB connection
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 2000) {
        delay(50);
        yield(); // Feed watchdog
    }
#else
    // UART mode: Brief delay for stability
    delay(1000);
#endif

    _logger_tokens = LOGGER_BURST_MAX;
    _logger_last_refill_ms = millis();
    _logger_suppressed = 0;

    // Immediate diagnostic output - work regardless of USB connection
    Serial.println("\n=== LOGGER INITIALIZED ===");
    Serial.printf("Board: %s\n", BOARD_NAME);
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("USB CDC: %s\n",
                  #if ARDUINO_USB_CDC_ON_BOOT
                  "ENABLED"
                  #else
                  "DISABLED"
                  #endif
                 );
    Serial.printf("Logger Level: %d\n", (int)level);
    Serial.printf("Init Time: %lu ms\n", millis());

    #if ARDUINO_USB_CDC_ON_BOOT
    if (!Serial) {
        Serial.println("WARNING: USB CDC not connected - output may not be visible");
        Serial.println("Check USB cable and serial monitor connection");
        Serial.println("Board will continue operation regardless of USB CDC status");
    } else {
        Serial.println("USB CDC connection established successfully");
    }
    #endif

    Serial.println("========================\n");
    Serial.flush();
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

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Extract filename from path
    const char *filename = strrchr(file, '/');
    if (!filename)
        filename = strrchr(file, '\\');
    filename = filename ? filename + 1 : file;

    _logger_tokens -= 1.0f;
    Serial.printf("[%6lu] [%-8s] [Heap:%6u] %s (%s:%d)\n",
                  millis() / 1000,
                  level_names[level],
                  ESP.getFreeHeap(),
                  buffer,
                  filename,
                  line);
}
