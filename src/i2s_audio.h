#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <Arduino.h>
#include "driver/i2s.h"
#include "config.h"

// I2S Audio Error Classification
enum class I2SErrorType
{
    NONE,      // No error
    TRANSIENT, // Temporary error, retry likely to succeed
    PERMANENT, // Permanent error, reinitialization needed
    FATAL      // Cannot recover, reboot required
};

// I2S Audio management
class I2SAudio
{
public:
    static bool initialize();
    static void cleanup();
    static bool readData(uint8_t *buffer, size_t buffer_size, size_t *bytes_read);
    static bool readDataWithRetry(uint8_t *buffer, size_t buffer_size, size_t *bytes_read, int max_retries = I2S_MAX_READ_RETRIES);
    static bool reinitialize();

    // Health check and error classification
    static bool healthCheck();
    static I2SErrorType classifyError(esp_err_t error);
    static uint32_t getErrorCount();
    static uint32_t getTransientErrorCount();
    static uint32_t getPermanentErrorCount();

private:
    static bool is_initialized;
    static int consecutive_errors;
    static uint32_t total_errors;
    static uint32_t transient_errors;
    static uint32_t permanent_errors;

    // Static buffer for 32-bit I2S reads (prevents heap fragmentation)
    // Max size: I2S_BUFFER_SIZE (4096) / 2 samples × 4 bytes = 8192 bytes
    static int32_t temp_read_buffer[4096]; // 4096 samples × 4 bytes = 16KB (safe maximum)
};

#endif // I2S_AUDIO_H
