#include "NetworkQualityMonitor.h"
#include <WiFi.h>
#include "../utils/EnhancedLogger.h"
#include "../core/SystemManager.h"
#include <cmath>

NetworkQualityMonitor::NetworkQualityMonitor()
    : last_check_time(0), measurement_count(0), rssi_ema(0.0f),
      packet_loss_ema(0.0f), previous_rssi(0), previous_check_time(0) {
    history.reserve(HISTORY_SIZE);
}

void NetworkQualityMonitor::update() {
    unsigned long current_time = millis();
    if (current_time - last_check_time < CHECK_INTERVAL) {
        return;
    }

    last_check_time = current_time;
    measurement_count++;

    if (WiFi.status() == WL_CONNECTED) {
        int current_rssi = WiFi.RSSI();
        updateRSSI(current_rssi);

        // Simple packet loss estimation based on connection stability
        // In a full implementation, this would be measured via ping/echo
        float loss = 0.0f;
        if (current_rssi < -80) {
            loss = 5.0f;  // Estimated 5% loss in weak signal
        } else if (current_rssi < -70) {
            loss = 2.0f;  // Estimated 2% loss
        }
        recordPacketLoss(loss);
    }
}

void NetworkQualityMonitor::updateRSSI(int rssi_value) {
    // Update exponential moving average (EMA)
    if (rssi_ema == 0.0f) {
        rssi_ema = rssi_value;
    } else {
        rssi_ema = 0.7f * rssi_ema + 0.3f * rssi_value;
    }

    previous_rssi = rssi_value;
    previous_check_time = millis();

    // Store in history
    QualityMetrics metric(rssi_value, packet_loss_ema, 0);
    if (history.size() >= HISTORY_SIZE) {
        history.erase(history.begin());
    }
    history.push_back(metric);
}

void NetworkQualityMonitor::recordPacketLoss(float loss_percent) {
    if (packet_loss_ema == 0.0f) {
        packet_loss_ema = loss_percent;
    } else {
        packet_loss_ema = 0.8f * packet_loss_ema + 0.2f * loss_percent;
    }
}

int NetworkQualityMonitor::getCurrentRSSI() const {
    return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

float NetworkQualityMonitor::getPacketLoss() const {
    return packet_loss_ema;
}

float NetworkQualityMonitor::getAverageRSSI() const {
    if (history.empty()) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (const auto& metric : history) {
        sum += metric.rssi;
    }
    return sum / history.size();
}

int NetworkQualityMonitor::getRSSITrend() const {
    if (history.size() < 2) {
        return 0;
    }

    // Simple linear regression slope
    int n = history.size();
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_xy = 0.0f;
    float sum_x2 = 0.0f;

    for (int i = 0; i < n; i++) {
        sum_x += i;
        sum_y += history[i].rssi;
        sum_xy += i * history[i].rssi;
        sum_x2 += i * i;
    }

    float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    return static_cast<int>(slope);
}

int NetworkQualityMonitor::getQualityScore() const {
    // Quality score 0-100 based on RSSI and packet loss
    int rssi = getCurrentRSSI();
    if (rssi == 0) {
        return 0;  // Not connected
    }

    // RSSI scoring (-30 to -90 dBm)
    int rssi_score = 100;
    if (rssi > -50) {
        rssi_score = 100;
    } else if (rssi > -60) {
        rssi_score = 90;
    } else if (rssi > -70) {
        rssi_score = 70;
    } else if (rssi > -80) {
        rssi_score = 40;
    } else {
        rssi_score = 10;
    }

    // Adjust for packet loss
    int loss_penalty = static_cast<int>(packet_loss_ema * 2);  // Each % loss = 2 points
    return std::max(0, rssi_score - loss_penalty);
}

bool NetworkQualityMonitor::isQualityDegraded() const {
    return getQualityScore() < 50;
}

bool NetworkQualityMonitor::shouldReconnect() const {
    int rssi = getCurrentRSSI();
    float loss = getPacketLoss();

    // Reconnect if RSSI is very weak or packet loss is high
    return (rssi < RSSI_WEAK_THRESHOLD) || (loss > 10.0f);
}

void NetworkQualityMonitor::clearHistory() {
    history.clear();
}

void NetworkQualityMonitor::printQualityMetrics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;

    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "=== Network Quality Metrics ===");
    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "Current RSSI: %d dBm", getCurrentRSSI());
    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "Average RSSI: %.1f dBm", getAverageRSSI());
    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "Packet Loss: %.2f%%", getPacketLoss());
    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "Quality Score: %d/100", getQualityScore());
    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "RSSI Trend: %d dBm/sample", getRSSITrend());
    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "History Size: %u samples", static_cast<unsigned>(getHistorySize()));
    logger->log(LogLevel::LOG_INFO, "NetworkQualityMonitor", __FILE__, __LINE__,
                "================================");
}
