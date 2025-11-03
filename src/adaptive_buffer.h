#ifndef ADAPTIVE_BUFFER_H
#define ADAPTIVE_BUFFER_H

#include <Arduino.h>

// Adaptive buffer sizing based on WiFi signal strength
class AdaptiveBuffer {
public:
  // Initialize with base buffer size
  static void initialize(size_t base_size = 4096);

  // Update buffer size based on WiFi RSSI
  static void updateBufferSize(int32_t rssi);

  // Get current buffer size
  static size_t getBufferSize();

  // Get buffer efficiency score (0-100)
  static uint8_t getEfficiencyScore();

  // Get statistics
  static int32_t getLastRSSI();
  static uint32_t getAdjustmentCount();

private:
  static size_t base_buffer_size;
  static size_t current_buffer_size;
  static int32_t last_rssi;
  static uint32_t adjustment_count;
  static unsigned long last_adjustment_time;

  // Calculate appropriate buffer size for signal strength
  static size_t calculateBufferSize(int32_t rssi);
};

#endif // ADAPTIVE_BUFFER_H
