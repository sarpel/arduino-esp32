#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "NonBlockingTimer.h"

// Exponential backoff for reconnection attempts
class ExponentialBackoff {
private:
    unsigned long min_delay;
    unsigned long max_delay;
    unsigned long current_delay;
    int consecutive_failures;

public:
    ExponentialBackoff(unsigned long min_ms = SERVER_RECONNECT_MIN,
                      unsigned long max_ms = SERVER_RECONNECT_MAX);

    unsigned long getNextDelay();
    void reset();
    int getFailureCount() const { return consecutive_failures; }
};

// Network management with reliability features
class NetworkManager {
public:
    static void initialize();
    static void handleWiFiConnection();
    static bool isWiFiConnected();
    static void monitorWiFiQuality();

    // Server connection management
    static bool connectToServer();
    static void disconnectFromServer();
    static bool isServerConnected();
    static WiFiClient& getClient();

    // TCP write with timeout detection
    static bool writeData(const uint8_t* data, size_t length);

    // Statistics
    static uint32_t getWiFiReconnectCount();
    static uint32_t getServerReconnectCount();
    static uint32_t getTCPErrorCount();

private:
    static bool server_connected;
    static unsigned long last_successful_write;
    static NonBlockingTimer wifi_retry_timer;
    static NonBlockingTimer server_retry_timer;
    static NonBlockingTimer rssi_check_timer;
    static ExponentialBackoff server_backoff;
    static WiFiClient client;

    static uint32_t wifi_reconnect_count;
    static uint32_t server_reconnect_count;
    static uint32_t tcp_error_count;
    static int wifi_retry_count;
};

#endif // NETWORK_H
