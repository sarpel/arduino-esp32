#ifndef NETWORK_SIMULATOR_H
#define NETWORK_SIMULATOR_H

#include <Arduino.h>
#include <vector>
#include <queue>
#include <memory>

enum class SimulationCondition {
    EXCELLENT = 0,
    GOOD = 1,
    FAIR = 2,
    POOR = 3,
    CRITICAL = 4,
    OFFLINE = 5
};

struct NetworkSimulationParams {
    int rssi;
    float packet_loss_percent;
    int latency_ms;
    float jitter_percent;
    float bandwidth_kbps;
    bool connection_drops;
    uint32_t drop_interval_ms;
    
    NetworkSimulationParams() : rssi(-50), packet_loss_percent(0.0f), latency_ms(0),
                               jitter_percent(0.0f), bandwidth_kbps(10000.0f),
                               connection_drops(false), drop_interval_ms(0) {}
};

struct SimulatedPacket {
    std::vector<uint8_t> data;
    unsigned long arrival_time;
    bool should_drop;
    
    SimulatedPacket() : arrival_time(0), should_drop(false) {}
};

class NetworkSimulator {
private:
    SimulationCondition current_condition;
    NetworkSimulationParams params;
    
    std::queue<SimulatedPacket> packet_queue;
    std::vector<SimulatedPacket> delayed_packets;
    
    unsigned long last_drop_time;
    unsigned long packets_dropped;
    unsigned long packets_processed;
    unsigned long total_latency_ms;
    
    bool enabled;
    bool initialized;
    
    float generateRandomJitter();
    bool shouldDropPacket();
    int calculateDelayWithJitter();
    
public:
    NetworkSimulator();
    
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    void setSimulationCondition(SimulationCondition condition);
    void setCustomParameters(const NetworkSimulationParams& params);
    
    void simulatePacketSend(const uint8_t* data, size_t length);
    bool simulatePacketReceive(uint8_t* buffer, size_t buffer_size, size_t& bytes_received);
    
    void update();
    void processDelayedPackets();
    
    void enable(bool state) { enabled = state; }
    bool isEnabled() const { return enabled; }
    
    SimulationCondition getCurrentCondition() const { return current_condition; }
    const NetworkSimulationParams& getParameters() const { return params; }
    
    uint32_t getPacketsDropped() const { return packets_dropped; }
    uint32_t getPacketsProcessed() const { return packets_processed; }
    float getAverageLatency() const;
    
    void reset();
    void printSimulationStatus() const;
};

#endif
