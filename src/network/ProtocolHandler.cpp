#include "ProtocolHandler.h"
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"
#include <cstring>
#include <algorithm>

ProtocolHandler::ProtocolHandler() 
    : version(ProtocolVersion::V3), current_sequence(0), 
      heartbeat_interval(30000), last_heartbeat_time(0) {
}

bool ProtocolHandler::initialize(ProtocolVersion v) {
    version = v;
    current_sequence = 0;
    statistics = PacketStatistics();
    return true;
}

uint16_t ProtocolHandler::calculateChecksum(const uint8_t* data, size_t length) {
    uint16_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
        checksum = ((checksum << 1) | (checksum >> 15));
    }
    return checksum;
}

bool ProtocolHandler::verifyChecksum(const PacketHeader& header, const uint8_t* payload) {
    uint16_t calculated = calculateChecksum(payload, header.payload_size);
    return calculated == header.checksum;
}

void ProtocolHandler::buildPacketHeader(PacketHeader& header, uint16_t payload_size, uint8_t flags) {
    header.sequence_number = current_sequence++;
    header.timestamp = millis();
    header.payload_size = payload_size;
    header.flags = flags;
    header.version = static_cast<uint8_t>(version);
    header.checksum = 0;
}

size_t ProtocolHandler::encodePacket(const uint8_t* payload, size_t payload_size, 
                                      uint8_t* output, size_t max_output, uint8_t flags) {
    if (payload_size + PacketHeader::HEADER_SIZE > max_output) {
        return 0;
    }
    
    PacketHeader header;
    buildPacketHeader(header, payload_size, flags);
    header.checksum = calculateChecksum(payload, payload_size);
    
    memcpy(output, &header, PacketHeader::HEADER_SIZE);
    if (payload && payload_size > 0) {
        memcpy(output + PacketHeader::HEADER_SIZE, payload, payload_size);
    }
    
    recordPacketSent(PacketHeader::HEADER_SIZE + payload_size);
    
    if (flags & static_cast<uint8_t>(PacketFlag::ACK_REQUIRED)) {
        unacked_packets.push_back({header.sequence_number, millis()});
    }
    
    return PacketHeader::HEADER_SIZE + payload_size;
}

bool ProtocolHandler::decodePacket(const uint8_t* packet, size_t packet_size, 
                                   uint8_t* payload, size_t& payload_size, PacketHeader& header) {
    if (packet_size < PacketHeader::HEADER_SIZE) {
        return false;
    }
    
    memcpy(&header, packet, PacketHeader::HEADER_SIZE);
    
    if (header.payload_size + PacketHeader::HEADER_SIZE > packet_size) {
        return false;
    }
    
    if (!verifyChecksum(header, packet + PacketHeader::HEADER_SIZE)) {
        statistics.checksum_failures++;
        return false;
    }
    
    if (header.payload_size > 0 && payload) {
        memcpy(payload, packet + PacketHeader::HEADER_SIZE, header.payload_size);
    }
    
    payload_size = header.payload_size;
    recordPacketReceived(packet_size);
    
    if (header.flags & static_cast<uint8_t>(PacketFlag::ACK_REQUIRED)) {
        pending_acks.push_back(header.sequence_number);
    }
    
    return true;
}

bool ProtocolHandler::handleAcknowledgment(uint16_t sequence_number) {
    auto it = std::find_if(unacked_packets.begin(), unacked_packets.end(),
                          [sequence_number](const std::pair<uint16_t, unsigned long>& p) {
                              return p.first == sequence_number;
                          });
    
    if (it != unacked_packets.end()) {
        unsigned long rtt = millis() - it->second;
        statistics.average_rtt_ms = (statistics.average_rtt_ms * 0.9f) + (rtt * 0.1f);
        unacked_packets.erase(it);
        statistics.acks_received++;
        return true;
    }
    
    return false;
}

void ProtocolHandler::checkRetransmitTimeouts() {
    unsigned long current_time = millis();
    
    for (auto it = unacked_packets.begin(); it != unacked_packets.end();) {
        if (current_time - it->second > ACK_TIMEOUT) {
            statistics.retransmissions++;
            ++it;
        } else {
            ++it;
        }
    }
}

std::vector<uint8_t> ProtocolHandler::buildAckPacket(const std::vector<uint16_t>& sequences) {
    std::vector<uint8_t> ack_packet(PacketHeader::HEADER_SIZE + sequences.size() * 2);
    
    PacketHeader header;
    buildPacketHeader(header, sequences.size() * 2, 0);
    header.checksum = 0;
    
    memcpy(ack_packet.data(), &header, PacketHeader::HEADER_SIZE);
    
    for (size_t i = 0; i < sequences.size(); i++) {
        uint16_t seq = sequences[i];
        memcpy(ack_packet.data() + PacketHeader::HEADER_SIZE + (i * 2), &seq, 2);
    }
    
    statistics.acks_sent++;
    return ack_packet;
}

void ProtocolHandler::recordPacketSent(size_t size) {
    statistics.total_sent++;
}

void ProtocolHandler::recordPacketReceived(size_t size) {
    statistics.total_received++;
}

void ProtocolHandler::recordPacketDropped() {
    statistics.dropped_packets++;
    statistics.packet_loss_rate = static_cast<float>(statistics.dropped_packets) / 
                                  (statistics.total_received + statistics.dropped_packets);
}

void ProtocolHandler::recordRetransmission() {
    statistics.retransmissions++;
}

void ProtocolHandler::resetStatistics() {
    statistics = PacketStatistics();
}

bool ProtocolHandler::shouldSendHeartbeat() {
    unsigned long current_time = millis();
    if (current_time - last_heartbeat_time >= heartbeat_interval) {
        last_heartbeat_time = current_time;
        return true;
    }
    return false;
}

std::vector<uint8_t> ProtocolHandler::buildHeartbeatPacket() {
    std::vector<uint8_t> heartbeat(PacketHeader::HEADER_SIZE);
    
    PacketHeader header;
    buildPacketHeader(header, 0, 0);
    header.checksum = 0;
    
    memcpy(heartbeat.data(), &header, PacketHeader::HEADER_SIZE);
    return heartbeat;
}

void ProtocolHandler::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "=== Protocol Handler Statistics ===");
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Version: %d", static_cast<int>(version));
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Total Sent: %u", statistics.total_sent);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Total Received: %u", statistics.total_received);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "ACKs Sent: %u", statistics.acks_sent);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "ACKs Received: %u", statistics.acks_received);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Retransmissions: %u", statistics.retransmissions);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Dropped Packets: %u", statistics.dropped_packets);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Checksum Failures: %u", statistics.checksum_failures);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Average RTT: %.2f ms", statistics.average_rtt_ms);
    logger->log(LogLevel::LOG_INFO, "ProtocolHandler", __FILE__, __LINE__, "Packet Loss Rate: %.2f%%", statistics.packet_loss_rate * 100.0f);
}
