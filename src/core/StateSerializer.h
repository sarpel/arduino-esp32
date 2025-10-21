#ifndef STATE_SERIALIZER_H
#define STATE_SERIALIZER_H

#include <Arduino.h>
#include <cstdint>
#include "../config.h"

// TLV (Type-Length-Value) serialization format for EEPROM persistence

// TLV record types
enum class TLVType : uint8_t {
    NETWORK_STATE = 1,
    CONNECTION_STATS = 2,
    HEALTH_HISTORY = 3,
    RECOVERY_COUNT = 4,
    CRASH_DATA = 5
};

// Serialized state structure
struct SerializedState {
    uint16_t crc;
    uint32_t timestamp;
    uint8_t version;
    uint8_t data[512];
    uint16_t data_length;

    SerializedState() : crc(0), timestamp(0), version(1), data_length(0) {
        memset(data, 0, sizeof(data));
    }
};

// State persistence via EEPROM/Flash
class StateSerializer {
private:
    static constexpr size_t MAX_STATE_SIZE = 1024;
    static constexpr size_t EEPROM_OFFSET = 0x1000;
    static constexpr uint32_t MIN_WRITE_INTERVAL_MS = 60000;  // Min 60s between writes

    unsigned long last_write_time;
    uint32_t write_count;

public:
    StateSerializer();

    // Serialization
    bool serializeState(const SerializedState& state);
    bool deserializeState(SerializedState& state);

    // EEPROM operations
    bool writeToEEPROM(const SerializedState& state);
    bool readFromEEPROM(SerializedState& state);

    // CRC validation
    uint16_t calculateCRC(const uint8_t* data, size_t length) const;
    bool validateCRC(const SerializedState& state) const;

    // Statistics
    uint32_t getWriteCount() const { return write_count; }
    unsigned long getLastWriteTime() const { return last_write_time; }

    // Utility
    void printStatus() const;

private:
    bool canWriteToEEPROM() const;
};

#endif // STATE_SERIALIZER_H
