#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <memory>
#include "../core/SystemManager.h"
#include "../config.h"

// Network quality metrics
struct NetworkQuality {
    int rssi;
    float packet_loss;
    int latency_ms;
    float bandwidth_kbps;
    float stability_score;
    uint32_t connection_drops;
    uint32_t reconnect_count;
    
    NetworkQuality() : rssi(0), packet_loss(0.0f), latency_ms(0), 
                      bandwidth_kbps(0.0f), stability_score(1.0f), 
                      connection_drops(0), reconnect_count(0) {}
};

// WiFi network configuration
struct WiFiNetwork {
    String ssid;
    String password;
    int priority;
    bool auto_connect;
    
    WiFiNetwork(const String& s, const String& p, int pri = 0, bool auto_conn = true)
        : ssid(s), password(p), priority(pri), auto_connect(auto_conn) {}
};

// Multi-WiFi manager
class MultiWiFiManager {
private:
    std::vector<WiFiNetwork> networks;
    size_t current_network_index;
    unsigned long last_switch_time;
    static constexpr unsigned long MIN_SWITCH_INTERVAL = 30000; // 30 seconds
    
public:
    MultiWiFiManager();
    
    void addNetwork(const String& ssid, const String& password, int priority = 0);
    void removeNetwork(const String& ssid);
    void clearNetworks();
    
    bool connectToBestNetwork();
    bool switchToNextNetwork();
    bool shouldSwitchNetwork(int current_rssi);
    
    const WiFiNetwork& getCurrentNetwork() const;
    size_t getNetworkCount() const { return networks.size(); }
    bool hasNetworks() const { return !networks.empty(); }
    
    void sortNetworksByPriority();
    
private:
    int findNetworkIndex(const String& ssid) const;
    bool isValidNetwork(size_t index) const;
};

// Enhanced Network Manager
class NetworkManager {
private:
    // Multi-WiFi support
    std::unique_ptr<MultiWiFiManager> wifi_manager;
    
    // Connection state
    bool wifi_connected;
    bool server_connected;
    WiFiClient client;
    
    // Quality monitoring
    NetworkQuality current_quality;
    NetworkQuality historical_quality;
    unsigned long last_quality_check;
    
    // Statistics
    uint32_t wifi_reconnect_count;
    uint32_t server_reconnect_count;
    uint32_t tcp_error_count;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    
    // Configuration
    bool initialized;
    bool safe_mode;
    
    // Internal methods
    bool connectToWiFiInternal();
    bool connectToServerInternal();
    void disconnectFromWiFiInternal();
    void disconnectFromServerInternal();
    void updateNetworkQuality();
    void calculateStabilityScore();
    bool shouldAutoSwitchNetwork();
    
public:
    NetworkManager();
    ~NetworkManager();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // WiFi management
    void handleWiFiConnection();
    bool isWiFiConnected() const { return wifi_connected; }
    String getWiFiSSID() const;
    IPAddress getWiFiIP() const;
    
    // Multi-WiFi support
    void addWiFiNetwork(const String& ssid, const String& password, int priority = 0);
    void removeWiFiNetwork(const String& ssid);
    void clearWiFiNetworks();
    bool switchToBestWiFiNetwork();
    
    // Server connection
    bool connectToServer();
    void disconnectFromServer();
    bool isServerConnected() const { return server_connected; }
    bool writeData(const uint8_t* data, size_t length);
    bool readData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read);
    
    // Quality monitoring
    void monitorWiFiQuality();
    const NetworkQuality& getNetworkQuality() const { return current_quality; }
    float getNetworkStability() const { return current_quality.stability_score; }
    int getWiFiRSSI() const { return current_quality.rssi; }
    
    // Statistics
    uint32_t getWiFiReconnectCount() const { return wifi_reconnect_count; }
    uint32_t getServerReconnectCount() const { return server_reconnect_count; }
    uint32_t getTCPErrorCount() const { return tcp_error_count; }
    uint32_t getBytesSent() const { return bytes_sent; }
    uint32_t getBytesReceived() const { return bytes_received; }
    
    // Safe mode
    void setSafeMode(bool enable) { safe_mode = enable; }
    bool isSafeMode() const { return safe_mode; }
    
    // Utility
    void printNetworkInfo() const;
    void printStatistics() const;
    bool validateConnection() const;
    
    // Advanced features
    bool startWiFiScan();
    std::vector<String> getAvailableNetworks() const;
    bool testConnectionQuality();
    float estimateBandwidth();
};

#endif // NETWORK_MANAGER_H