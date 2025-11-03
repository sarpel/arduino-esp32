#ifndef NETWORK_H
#define NETWORK_H

#include "NonBlockingTimer.h"
#include "adaptive_buffer.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>

// TCP Connection states
enum class TCPConnectionState {
  DISCONNECTED, // Not connected, ready for new attempt
  CONNECTING,   // Connection attempt in progress
  CONNECTED,    // Active connection (data can flow)
  ERROR,        // Connection error detected
  CLOSING       // Graceful disconnect in progress
};

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

  // Adaptive buffer management
  static void updateAdaptiveBuffer();
  static size_t getAdaptiveBufferSize();

  // Server connection management
  static bool connectToServer();
  static void disconnectFromServer();
  static bool isServerConnected();
  static WiFiClient &getClient();

  // TCP connection state management
  static TCPConnectionState getTCPState();
  static bool isTCPConnecting();
  static bool isTCPConnected();
  static bool isTCPError();
  static unsigned long getTimeSinceLastWrite();
  static unsigned long getConnectionUptime();

  // TCP write with timeout detection
  static bool writeData(const uint8_t *data, size_t length);

  // Statistics
  static uint32_t getWiFiReconnectCount();
  static uint32_t getServerReconnectCount();
  static uint32_t getTCPErrorCount();
  static uint32_t getTCPStateChangeCount();

private:
  // Connection state tracking
  static TCPConnectionState tcp_state;
  static unsigned long tcp_state_change_time;
  static unsigned long tcp_connection_established_time;
  static uint32_t tcp_state_changes;

  // Error recovery
  static void handleTCPError(const char *error_source);
  static void updateTCPState(TCPConnectionState new_state);
  static bool validateConnection();

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