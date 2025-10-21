#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <Arduino.h>
#include <vector>
#include <memory>
#include <cstdint>

enum class ProtocolVersion {
    V1 = 1,
    V2 = 2,
    V3 = 3
};

struct PacketHeader {
    uint16_t sequence_number;
    uint32_t timestamp;
    uint16_t payload_size;
    uint8_t flags;
    uint8_t version;
    uint16_t checksum;
    
    static constexpr uint16_t HEADER_SIZE = 16;
};

enum class PacketFlag {
    ACK_REQUIRED = 0x01,
    COMPRESSED = 0x02,
    ENCRYPTED = 0x04,
    FRAGMENTED = 0x08,
    PRIORITY = 0x10,
    RETRANSMISSION = 0x20
};

struct PacketStatistics {
    uint32_t total_sent;
    uint32_t total_received;
    uint32_t acks_sent;
    uint32_t acks_received;
    uint32_t retransmissions;
    uint32_t dropped_packets;
    uint32_t checksum_failures;
    uint32_t sequence_errors;
    float average_rtt_ms;
    float packet_loss_rate;
    
    PacketStatistics() : total_sent(0), total_received(0), acks_sent(0),
                        acks_received(0), retransmissions(0), dropped_packets(0),
                        checksum_failures(0), sequence_errors(0),
                        average_rtt_ms(0.0f), packet_loss_rate(0.0f) {}
};

class ProtocolHandler {
private:
    ProtocolVersion version;
    uint16_t current_sequence;
    
    std::vector<uint16_t> pending_acks;
    std::vector<std::pair<uint16_t, unsigned long>> unacked_packets;
    
    static constexpr unsigned long ACK_TIMEOUT = 5000;
    static constexpr uint16_t MAX_RETRANSMIT = 3;
    
    PacketStatistics statistics;
    uint32_t heartbeat_interval;
    unsigned long last_heartbeat_time;
    
    uint16_t calculateChecksum(const uint8_t* data, size_t length);
    bool verifyChecksum(const PacketHeader& header, const uint8_t* payload);
    void buildPacketHeader(PacketHeader& header, uint16_t payload_size, uint8_t flags);
    
public:
    ProtocolHandler();
    
    bool initialize(ProtocolVersion v = ProtocolVersion::V3);
    
    size_t encodePacket(const uint8_t* payload, size_t payload_size, uint8_t* output, size_t max_output, uint8_t flags = 0);
    bool decodePacket(const uint8_t* packet, size_t packet_size, uint8_t* payload, size_t& payload_size, PacketHeader& header);
    
    bool handleAcknowledgment(uint16_t sequence_number);
    void checkRetransmitTimeouts();
    std::vector<uint8_t> buildAckPacket(const std::vector<uint16_t>& sequences);
    
    void recordPacketSent(size_t size);
    void recordPacketReceived(size_t size);
    void recordPacketDropped();
    void recordRetransmission();
    
    const PacketStatistics& getStatistics() const { return statistics; }
    void resetStatistics();
    
    void setHeartbeatInterval(uint32_t interval_ms) { heartbeat_interval = interval_ms; }
    bool shouldSendHeartbeat();
    std::vector<uint8_t> buildHeartbeatPacket();
    
    ProtocolVersion getVersion() const { return version; }
    uint16_t getCurrentSequence() const { return current_sequence; }
    
    void printStatistics() const;
};

#endif
