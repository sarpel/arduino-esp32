#ifndef NETWORK_QUALITY_MONITOR_H
#define NETWORK_QUALITY_MONITOR_H

#include <Arduino.h>
#include <vector>
#include "../config.h"

// Network quality metrics with history tracking
struct QualityMetrics {
    int rssi;                    // Signal strength (dBm)
    float packet_loss;           // Percentage (0-100)
    int latency_ms;              // Round-trip time
    unsigned long timestamp;     // When measured

    QualityMetrics() : rssi(0), packet_loss(0.0f), latency_ms(0), timestamp(0) {}
    QualityMetrics(int r, float p, int l) : rssi(r), packet_loss(p), latency_ms(l), timestamp(millis()) {}
};

// NetworkQualityMonitor - tracks WiFi quality metrics
class NetworkQualityMonitor {
private:
    static constexpr size_t HISTORY_SIZE = 60;  // 60 seconds of history
    static constexpr uint32_t CHECK_INTERVAL = 1000;  // Check every 1 second

    std::vector<QualityMetrics> history;
    unsigned long last_check_time;
    unsigned long measurement_count;

    // Exponential moving average for RSSI
    float rssi_ema;
    float packet_loss_ema;

    int previous_rssi;
    unsigned long previous_check_time;

public:
    NetworkQualityMonitor();

    // Update quality metrics
    void update();
    void updateRSSI(int rssi_value);
    void recordPacketLoss(float loss_percent);

    // Get current metrics
    int getCurrentRSSI() const;
    float getPacketLoss() const;
    float getAverageRSSI() const;
    int getRSSITrend() const;  // Returns slope of RSSI trend

    // Quality scoring (0-100)
    int getQualityScore() const;

    // History analysis
    const std::vector<QualityMetrics>& getHistory() const { return history; }
    size_t getHistorySize() const { return history.size(); }
    void clearHistory();

    // Thresholds and alerts
    bool isQualityDegraded() const;
    bool shouldReconnect() const;

    // Utility
    void printQualityMetrics() const;
};

#endif // NETWORK_QUALITY_MONITOR_H
