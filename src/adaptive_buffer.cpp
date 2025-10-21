#include "adaptive_buffer.h"
#include "logger.h"

// Static member initialization
size_t AdaptiveBuffer::base_buffer_size = 4096;
size_t AdaptiveBuffer::current_buffer_size = 4096;
int32_t AdaptiveBuffer::last_rssi = -100;
uint32_t AdaptiveBuffer::adjustment_count = 0;
unsigned long AdaptiveBuffer::last_adjustment_time = 0;

void AdaptiveBuffer::initialize(size_t base_size) {
    base_buffer_size = base_size;
    current_buffer_size = base_size;
    LOG_INFO("Adaptive Buffer initialized with base size: %u bytes", base_size);
}

size_t AdaptiveBuffer::calculateBufferSize(int32_t rssi) {
    //  RSSI to buffer size mapping:
    // Strong signal (-50 to -60):  100% = base_size
    // Good signal (-60 to -70):     80% = base_size * 0.8
    // Acceptable (-70 to -80):      60% = base_size * 0.6
    // Weak (-80 to -90):            40% = base_size * 0.4
    // Very weak (<-90):             20% = base_size * 0.2

    size_t new_size;

    if (rssi >= -60) {
        // Strong signal - full buffer
        new_size = base_buffer_size;
    } else if (rssi >= -70) {
        // Good signal - 80% buffer
        new_size = (base_buffer_size * 80) / 100;
    } else if (rssi >= -80) {
        // Acceptable signal - 60% buffer
        new_size = (base_buffer_size * 60) / 100;
    } else if (rssi >= -90) {
        // Weak signal - 40% buffer
        new_size = (base_buffer_size * 40) / 100;
    } else {
        // Very weak signal - 20% buffer (minimum useful size)
        new_size = (base_buffer_size * 20) / 100;
    }

    // Ensure minimum size (256 bytes)
    if (new_size < 256) {
        new_size = 256;
    }

    return new_size;
}

void AdaptiveBuffer::updateBufferSize(int32_t rssi) {
    last_rssi = rssi;

    // Only adjust if minimum interval passed (5 seconds)
    unsigned long now = millis();
    if (now - last_adjustment_time < 5000) {
        return;
    }

    size_t new_size = calculateBufferSize(rssi);

    // Only log if size changed significantly (>10%)
    if (new_size != current_buffer_size) {
        int change_pct = ((int)new_size - (int)current_buffer_size) * 100 / (int)current_buffer_size;

        if (abs(change_pct) >= 10) {
            LOG_DEBUG("Buffer size adjusted: %u → %u bytes (%d%%) for RSSI %d dBm",
                      current_buffer_size, new_size, change_pct, rssi);

            current_buffer_size = new_size;
            adjustment_count++;
            last_adjustment_time = now;
        }
    }
}

size_t AdaptiveBuffer::getBufferSize() {
    return current_buffer_size;
}

uint8_t AdaptiveBuffer::getEfficiencyScore() {
    // Score based on how close buffer size is to optimal for current signal
    // 100 = perfect match, lower = less optimal

    if (last_rssi >= -60) {
        return 100;  // Strong signal - using full buffer
    } else if (last_rssi >= -70) {
        return (current_buffer_size * 100) / (base_buffer_size * 80 / 100);
    } else if (last_rssi >= -80) {
        return (current_buffer_size * 100) / (base_buffer_size * 60 / 100);
    } else if (last_rssi >= -90) {
        return (current_buffer_size * 100) / (base_buffer_size * 40 / 100);
    } else {
        return (current_buffer_size * 100) / (base_buffer_size * 20 / 100);
    }
}

int32_t AdaptiveBuffer::getLastRSSI() {
    return last_rssi;
}

uint32_t AdaptiveBuffer::getAdjustmentCount() {
    return adjustment_count;
}
