#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include "config.h"
#include "driver/i2s.h"
#include <Arduino.h>

// I2S Audio Error Classification
enum class I2SErrorType {
  NONE,      // No error
  TRANSIENT, // Temporary error, retry likely to succeed
  PERMANENT, // Permanent error, reinitialization needed
  FATAL      // Cannot recover, reboot required
};

// I2S Audio management
class I2SAudio {
public:
  static bool initialize();
  static void cleanup();
  static bool readData(uint8_t *buffer, size_t buffer_size, size_t *bytes_read);
  static bool readDataWithRetry(uint8_t *buffer, size_t buffer_size,
                                size_t *bytes_read,
                                int max_retries = I2S_MAX_READ_RETRIES);
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
};

#endif // I2S_AUDIO_H
