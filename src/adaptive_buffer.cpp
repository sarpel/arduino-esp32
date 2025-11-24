#include "adaptive_buffer.h"
#include "logger.h"

// Static member initialization
size_t AdaptiveBuffer::base_buffer_size = 4096;
size_t AdaptiveBuffer::current_buffer_size = 4096;
int32_t AdaptiveBuffer::last_rssi = -100;
uint32_t AdaptiveBuffer::adjustment_count = 0;
unsigned long AdaptiveBuffer::last_adjustment_time = 0;

void AdaptiveBuffer::initialize(size_t base_size)
{
    base_buffer_size = base_size;
    current_buffer_size = base_size;
    LOG_INFO("Adaptive Buffer initialized with base size: %u bytes", base_size);
}

size_t AdaptiveBuffer::calculateBufferSize(int32_t rssi)
{
    // RSSI-based buffer sizing for network reliability
    // Design principle: Weak signal = more packet loss/retransmission = need LARGER buffers
    // Strong signal = reliable transmission = can use smaller buffers to save RAM
    //
    // RSSI to buffer size mapping (inversely proportional to signal strength):
    // Strong signal (-50 to -60):     50% = base_size * 0.5   (reliable, minimal buffering)
    // Good signal (-60 to -70):       75% = base_size * 0.75
    // Acceptable (-70 to -80):       100% = base_size         (normal operation)
    // Weak (-80 to -90):             120% = base_size * 1.2  (needs extra buffering)
    // Very weak (<-90):              150% = base_size * 1.5  (maximum buffering for reliability)
    //
    // Note: We cap at 150% to avoid excessive memory usage on long-term weak signals

    size_t new_size;

    if (rssi >= -60)
    {
        // Strong signal - can use smaller buffer to save RAM
        new_size = (base_buffer_size * 50) / 100;
    }
    else if (rssi >= -70)
    {
        // Good signal - 75% buffer
        new_size = (base_buffer_size * 75) / 100;
    }
    else if (rssi >= -80)
    {
        // Acceptable signal - use full buffer
        new_size = base_buffer_size;
    }
    else if (rssi >= -90)
    {
        // Weak signal - INCREASE buffer to absorb jitter
        new_size = (base_buffer_size * 120) / 100;
    }
    else
    {
        // Very weak signal - MAXIMIZE buffer for reliability
        new_size = (base_buffer_size * 150) / 100;
    }

    // Ensure minimum size (256 bytes)
    if (new_size < 256)
    {
        new_size = 256;
    }

    return new_size;
}

void AdaptiveBuffer::updateBufferSize(int32_t rssi)
{
    last_rssi = rssi;

    // Only adjust if minimum interval passed (5 seconds)
    unsigned long now = millis();
    
    // BUG FIX: Handle millis() overflow (occurs every ~49.7 days)
    // When millis() wraps around, the subtraction will be incorrect
    // Use proper overflow-safe comparison
    if (last_adjustment_time != 0 && (now - last_adjustment_time < 5000) && (now >= last_adjustment_time))
    {
        return;
    }

    size_t new_size = calculateBufferSize(rssi);

    // Only log if size changed significantly (>10%)
    if (new_size != current_buffer_size)
    {
        // BUG FIX: Prevent division by zero when calculating percentage change
        if (current_buffer_size == 0) {
            LOG_ERROR("Adaptive buffer: current_buffer_size is 0, resetting to new size");
            current_buffer_size = new_size;
            adjustment_count++;
            last_adjustment_time = now;
            return;
        }
        
        // BUG FIX: Use safe integer arithmetic to prevent overflow
        // Cast to int64_t to handle potential overflow in intermediate calculations
        int64_t size_diff = (int64_t)new_size - (int64_t)current_buffer_size;
        int change_pct = (int)((size_diff * 100) / (int64_t)current_buffer_size);

        if (abs(change_pct) >= 10)
        {
            LOG_DEBUG("Buffer size adjusted: %u → %u bytes (%d%%) for RSSI %d dBm",
                      current_buffer_size, new_size, change_pct, rssi);

            current_buffer_size = new_size;
            adjustment_count++;
            last_adjustment_time = now;
        }
    }
}

size_t AdaptiveBuffer::getBufferSize()
{
    return current_buffer_size;
}

uint8_t AdaptiveBuffer::getEfficiencyScore()
{
    // Score based on how close buffer size is to optimal for current signal
    // 100 = perfect match, capped at 100 to prevent overflow
    //
    // This calculates what percentage of the optimal buffer we're currently using.
    // Higher is better - means we're well-matched to current network conditions.

    size_t optimal_size = calculateBufferSize(last_rssi);

    // BUG FIX: Prevent division by zero crash
    // If optimal_size is 0 (which shouldn't happen in normal operation but could occur
    // with extreme RSSI values or initialization issues), return 0 score safely
    if (optimal_size == 0)
    {
        LOG_ERROR("Adaptive buffer: optimal_size is 0, cannot calculate efficiency score");
        return 0; // Safety check - avoid division by zero
    }

    // BUG FIX: Prevent overflow in multiplication before division
    // Use 32-bit arithmetic safely by checking bounds first
    if (current_buffer_size > (UINT16_MAX / 100)) {
        // For very large buffers, calculate differently to prevent overflow
        uint16_t ratio = (current_buffer_size / optimal_size);
        uint16_t raw_score = ratio * 100;
        return (raw_score > 100) ? 100 : (uint8_t)raw_score;
    }

    uint16_t raw_score = (uint16_t)((current_buffer_size * 100) / optimal_size);

    // Cap at 100 to prevent overflow in uint8_t and to reflect perfection
    return (raw_score > 100) ? 100 : (uint8_t)raw_score;
}

int32_t AdaptiveBuffer::getLastRSSI()
{
    return last_rssi;
}

uint32_t AdaptiveBuffer::getAdjustmentCount()
{
    return adjustment_count;
}
