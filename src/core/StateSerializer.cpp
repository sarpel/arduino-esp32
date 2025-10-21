#include "StateSerializer.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"

StateSerializer::StateSerializer() : last_write_time(0), write_count(0) {}

bool StateSerializer::serializeState(const SerializedState& state) {
    if (!canWriteToEEPROM()) {
        return false;
    }

    return writeToEEPROM(state);
}

bool StateSerializer::deserializeState(SerializedState& state) {
    return readFromEEPROM(state);
}

bool StateSerializer::writeToEEPROM(const SerializedState& state) {
    // In a real implementation, this would write to EEPROM
    // For now, we just log the operation
    write_count++;
    last_write_time = millis();

    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_DEBUG, "StateSerializer", __FILE__, __LINE__,
                    "Wrote state to EEPROM: %u bytes (write #%u)", state.data_length, write_count);
    }

    return true;
}

bool StateSerializer::readFromEEPROM(SerializedState& state) {
    // In a real implementation, this would read from EEPROM
    // For now, we just log the operation
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_DEBUG, "StateSerializer", __FILE__, __LINE__,
                    "Read state from EEPROM");
    }

    return true;
}

uint16_t StateSerializer::calculateCRC(const uint8_t* data, size_t length) const {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

bool StateSerializer::validateCRC(const SerializedState& state) const {
    uint16_t calculated = calculateCRC(state.data, state.data_length);
    return calculated == state.crc;
}

void StateSerializer::printStatus() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "StateSerializer", __FILE__, __LINE__,
                "=== State Serializer Status ===");
    logger->log(LogLevel::LOG_INFO, "StateSerializer", __FILE__, __LINE__,
                "Total Writes: %u", write_count);
    logger->log(LogLevel::LOG_INFO, "StateSerializer", __FILE__, __LINE__,
                "Last Write: %lu ms ago", millis() - last_write_time);
    logger->log(LogLevel::LOG_INFO, "StateSerializer", __FILE__, __LINE__,
                "Max State Size: %u bytes", static_cast<unsigned>(MAX_STATE_SIZE));
    logger->log(LogLevel::LOG_INFO, "StateSerializer", __FILE__, __LINE__,
                "================================");
}

bool StateSerializer::canWriteToEEPROM() const {
    // Prevent too-frequent writes to preserve flash
    unsigned long time_since_last_write = millis() - last_write_time;
    return time_since_last_write >= MIN_WRITE_INTERVAL_MS;
}
