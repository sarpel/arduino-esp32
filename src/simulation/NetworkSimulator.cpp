#include "NetworkSimulator.h"
#include "../utils/EnhancedLogger.h"
#include <cmath>
#include <algorithm>

NetworkSimulator::NetworkSimulator()
    : current_condition(SimulationCondition::EXCELLENT),
      last_drop_time(0), packets_dropped(0), packets_processed(0),
      total_latency_ms(0), enabled(false), initialized(false) {
}

bool NetworkSimulator::initialize() {
    initialized = true;
    return true;
}

void NetworkSimulator::shutdown() {
    initialized = false;
    while (!packet_queue.empty()) {
        packet_queue.pop();
    }
    delayed_packets.clear();
}

void NetworkSimulator::setSimulationCondition(SimulationCondition condition) {
    current_condition = condition;
    
    switch (condition) {
        case SimulationCondition::EXCELLENT:
            params.rssi = -30;
            params.packet_loss_percent = 0.1f;
            params.latency_ms = 10;
            params.jitter_percent = 2.0f;
            params.bandwidth_kbps = 10000.0f;
            params.connection_drops = false;
            break;
            
        case SimulationCondition::GOOD:
            params.rssi = -50;
            params.packet_loss_percent = 0.5f;
            params.latency_ms = 30;
            params.jitter_percent = 5.0f;
            params.bandwidth_kbps = 5000.0f;
            params.connection_drops = false;
            break;
            
        case SimulationCondition::FAIR:
            params.rssi = -67;
            params.packet_loss_percent = 2.0f;
            params.latency_ms = 80;
            params.jitter_percent = 10.0f;
            params.bandwidth_kbps = 2000.0f;
            params.connection_drops = false;
            break;
            
        case SimulationCondition::POOR:
            params.rssi = -75;
            params.packet_loss_percent = 5.0f;
            params.latency_ms = 150;
            params.jitter_percent = 20.0f;
            params.bandwidth_kbps = 500.0f;
            params.connection_drops = true;
            params.drop_interval_ms = 30000;
            break;
            
        case SimulationCondition::CRITICAL:
            params.rssi = -85;
            params.packet_loss_percent = 15.0f;
            params.latency_ms = 300;
            params.jitter_percent = 40.0f;
            params.bandwidth_kbps = 100.0f;
            params.connection_drops = true;
            params.drop_interval_ms = 5000;
            break;
            
        case SimulationCondition::OFFLINE:
            params.packet_loss_percent = 100.0f;
            params.connection_drops = true;
            break;
    }
}

void NetworkSimulator::setCustomParameters(const NetworkSimulationParams& new_params) {
    params = new_params;
    current_condition = SimulationCondition::FAIR;
}

float NetworkSimulator::generateRandomJitter() {
    float jitter_range = params.latency_ms * (params.jitter_percent / 100.0f);
    float random_val = (random(0, 200) / 100.0f) - 1.0f;
    return random_val * jitter_range;
}

bool NetworkSimulator::shouldDropPacket() {
    if (current_condition == SimulationCondition::OFFLINE) {
        return true;
    }
    
    if (params.connection_drops) {
        unsigned long current_time = millis();
        if (current_time - last_drop_time >= params.drop_interval_ms) {
            last_drop_time = current_time;
            return true;
        }
    }
    
    float random_drop = (random(0, 10000) / 10000.0f) * 100.0f;
    return random_drop < params.packet_loss_percent;
}

int NetworkSimulator::calculateDelayWithJitter() {
    int base_delay = params.latency_ms;
    float jitter = generateRandomJitter();
    int final_delay = base_delay + static_cast<int>(jitter);
    return std::max(final_delay, 0);
}

void NetworkSimulator::simulatePacketSend(const uint8_t* data, size_t length) {
    if (!enabled || !initialized || !data) {
        return;
    }
    
    if (shouldDropPacket()) {
        packets_dropped++;
        return;
    }
    
    SimulatedPacket packet;
    packet.data.assign(data, data + length);
    packet.arrival_time = millis() + calculateDelayWithJitter();
    packet.should_drop = false;
    
    delayed_packets.push_back(packet);
}

bool NetworkSimulator::simulatePacketReceive(uint8_t* buffer, size_t buffer_size, size_t& bytes_received) {
    if (!enabled || !initialized) {
        bytes_received = 0;
        return false;
    }
    
    if (delayed_packets.empty()) {
        bytes_received = 0;
        return false;
    }
    
    processDelayedPackets();
    
    if (packet_queue.empty()) {
        bytes_received = 0;
        return false;
    }
    
    SimulatedPacket packet = packet_queue.front();
    packet_queue.pop();
    
    size_t copy_size = std::min(buffer_size, packet.data.size());
    if (buffer && copy_size > 0) {
        memcpy(buffer, packet.data.data(), copy_size);
    }
    
    bytes_received = copy_size;
    packets_processed++;
    
    unsigned long packet_latency = millis() - (packet.arrival_time - params.latency_ms);
    total_latency_ms += packet_latency;
    
    return true;
}

void NetworkSimulator::update() {
    if (!enabled || !initialized) {
        return;
    }
    
    processDelayedPackets();
}

void NetworkSimulator::processDelayedPackets() {
    unsigned long current_time = millis();
    
    auto it = delayed_packets.begin();
    while (it != delayed_packets.end()) {
        if (current_time >= it->arrival_time) {
            packet_queue.push(*it);
            it = delayed_packets.erase(it);
        } else {
            ++it;
        }
    }
}

float NetworkSimulator::getAverageLatency() const {
    if (packets_processed == 0) {
        return 0.0f;
    }
    return static_cast<float>(total_latency_ms) / packets_processed;
}

void NetworkSimulator::reset() {
    while (!packet_queue.empty()) {
        packet_queue.pop();
    }
    delayed_packets.clear();
    
    packets_dropped = 0;
    packets_processed = 0;
    total_latency_ms = 0;
    last_drop_time = 0;
}

void NetworkSimulator::printSimulationStatus() const {
    EnhancedLogger& logger = EnhancedLogger::getInstance();

    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "=== Network Simulator Status ===");
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Enabled: %s", enabled ? "Yes" : "No");
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Initialized: %s", initialized ? "Yes" : "No");
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Condition: %d", static_cast<int>(current_condition));
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "");
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "=== Simulation Parameters ===");
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "RSSI: %d dBm", params.rssi);
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Packet Loss: %.2f%%", params.packet_loss_percent);
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Latency: %d ms", params.latency_ms);
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Jitter: %.2f%%", params.jitter_percent);
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Bandwidth: %.2f kbps", params.bandwidth_kbps);
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "");
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "=== Statistics ===");
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Packets Dropped: %u", packets_dropped);
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Packets Processed: %u", packets_processed);
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Average Latency: %.2f ms", getAverageLatency());
    logger.log(LogLevel::LOG_INFO, "NetworkSimulator", __FILE__, __LINE__, "Pending Packets: %u", static_cast<uint32_t>(delayed_packets.size()));
}
