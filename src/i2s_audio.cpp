#include "i2s_audio.h"
#include "logger.h"

bool I2SAudio::is_initialized = false;
int I2SAudio::consecutive_errors = 0;

bool I2SAudio::initialize() {
    LOG_INFO("Initializing I2S audio driver...");

    // I2S configuration using legacy Arduino-ESP32 API
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,  // Use non-deprecated constant
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = I2S_DMA_BUF_COUNT,
        .dma_buf_len = I2S_DMA_BUF_LEN,
        .use_apll = true,              // Better clock stability
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0                // Auto-calculate
    };

    // I2S pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    // Install I2S driver
    esp_err_t result = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (result != ESP_OK) {
        LOG_ERROR("I2S driver install failed: %d", result);
        return false;
    }

    // Set I2S pin configuration
    result = i2s_set_pin(I2S_PORT, &pin_config);
    if (result != ESP_OK) {
        LOG_ERROR("I2S pin configuration failed: %d", result);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // Clear DMA buffer to remove initialization noise
    i2s_zero_dma_buffer(I2S_PORT);

    is_initialized = true;
    consecutive_errors = 0;
    LOG_INFO("I2S audio driver initialized successfully");
    return true;
}

void I2SAudio::cleanup() {
    if (!is_initialized) return;

    LOG_INFO("Cleaning up I2S audio driver...");
    i2s_stop(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
    is_initialized = false;
    LOG_INFO("I2S audio driver cleaned up");
}

bool I2SAudio::readData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!is_initialized) {
        LOG_ERROR("I2S not initialized");
        return false;
    }

    esp_err_t result = i2s_read(I2S_PORT, buffer, buffer_size, bytes_read, pdMS_TO_TICKS(1000));

    if (result != ESP_OK) {
        LOG_ERROR("I2S read failed: %d", result);
        consecutive_errors++;
        return false;
    }

    if (*bytes_read == 0) {
        LOG_WARN("I2S read returned 0 bytes");
        consecutive_errors++;
        return false;
    }

    // Reset error counter on successful read
    consecutive_errors = 0;
    return true;
}

bool I2SAudio::readDataWithRetry(uint8_t* buffer, size_t buffer_size, size_t* bytes_read, int max_retries) {
    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (readData(buffer, buffer_size, bytes_read)) {
            if (attempt > 0) {
                LOG_INFO("I2S read succeeded on attempt %d", attempt + 1);
            }
            return true;
        }

        LOG_WARN("I2S read attempt %d/%d failed", attempt + 1, max_retries);

        // Check if we need to reinitialize due to persistent errors
        if (consecutive_errors > MAX_CONSECUTIVE_FAILURES) {
            LOG_CRITICAL("Too many consecutive I2S errors - attempting reinitialization");
            if (reinitialize()) {
                LOG_INFO("I2S reinitialized successfully, retrying read");
                // Try one more time after reinitialize
                if (readData(buffer, buffer_size, bytes_read)) {
                    return true;
                }
            }
        }

        delay(10);  // Brief pause before retry
    }

    return false;
}

bool I2SAudio::reinitialize() {
    LOG_INFO("Reinitializing I2S...");
    cleanup();
    delay(100);
    bool result = initialize();
    if (result) {
        consecutive_errors = 0;
    }
    return result;
}
