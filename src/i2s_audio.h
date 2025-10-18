#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <Arduino.h>
#include "driver/i2s.h"
#include "config.h"

// I2S Audio management
class I2SAudio {
public:
    static bool initialize();
    static void cleanup();
    static bool readData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read);
    static bool readDataWithRetry(uint8_t* buffer, size_t buffer_size, size_t* bytes_read, int max_retries = I2S_MAX_READ_RETRIES);
    static bool reinitialize();

private:
    static bool is_initialized;
    static int consecutive_errors;
};

#endif // I2S_AUDIO_H
