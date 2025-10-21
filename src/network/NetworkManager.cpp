#include "NetworkManager.h"
#include "../core/SystemManager.h"
#include "../utils/EnhancedLogger.h"
#include <memory>
#include "NetworkManager.h"

// MultiWiFiManager implementation
MultiWiFiManager::MultiWiFiManager() : current_network_index(0), last_switch_time(0) {}

void MultiWiFiManager::addNetwork(const String& ssid, const String& password, int priority) {
    networks.emplace_back(ssid, password, priority);
    sortNetworksByPriority();
}

void MultiWiFiManager::removeNetwork(const String& ssid) {
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        networks.erase(networks.begin() + index);
        if (current_network_index >= networks.size()) {
            current_network_index = 0;
        }
    }
}

void MultiWiFiManager::clearNetworks() {
    networks.clear();
    current_network_index = 0;
}

bool MultiWiFiManager::connectToBestNetwork() {
    if (networks.empty()) {
        return false;
    }
    
    // Try networks in priority order
    for (size_t i = 0; i < networks.size(); i++) {
        const auto& network = networks[i];
        if (network.auto_connect) {
            WiFi.begin(network.ssid.c_str(), network.password.c_str());
            
            // Wait for connection with timeout
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                current_network_index = i;
                return true;
            }
        }
    }
    
    return false;
}

bool MultiWiFiManager::switchToNextNetwork() {
    if (networks.size() <= 1) {
        return false;
    }
    
    unsigned long current_time = millis();
    if (current_time - last_switch_time < MIN_SWITCH_INTERVAL) {
        return false;  // Too soon to switch
    }
    
    size_t next_index = (current_network_index + 1) % networks.size();
    const auto& network = networks[next_index];
    
    WiFi.begin(network.ssid.c_str(), network.password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        current_network_index = next_index;
        last_switch_time = current_time;
        return true;
    }
    
    return false;
}

bool MultiWiFiManager::shouldSwitchNetwork(int current_rssi) {
    if (networks.size() <= 1) {
        return false;
    }
    
    // Switch if RSSI is very poor
    return current_rssi < -85;
}

const WiFiNetwork& MultiWiFiManager::getCurrentNetwork() const {
    static WiFiNetwork empty_network("", "");
    return isValidNetwork(current_network_index) ? networks[current_network_index] : empty_network;
}

void MultiWiFiManager::sortNetworksByPriority() {
    std::sort(networks.begin(), networks.end(), 
        [](const WiFiNetwork& a, const WiFiNetwork& b) {
            return a.priority > b.priority;  // Higher priority first
        });
}

int MultiWiFiManager::findNetworkIndex(const String& ssid) const {
    for (size_t i = 0; i < networks.size(); i++) {
        if (networks[i].ssid == ssid) {
            return i;
        }
    }
    return -1;
}

bool MultiWiFiManager::isValidNetwork(size_t index) const {
    return index < networks.size();
}

// NetworkManager implementation
NetworkManager::NetworkManager() 
    : wifi_connected(false), server_connected(false), initialized(false), safe_mode(false),
      wifi_reconnect_count(0), server_reconnect_count(0), tcp_error_count(0),
      bytes_sent(0), bytes_received(0), last_quality_check(0) {
    
    wifi_manager = std::make_unique<MultiWiFiManager>();
}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize() {
    if (initialized) {
        return true;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "NetworkManager", "Initializing NetworkManager");
    }
    
    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We'll handle reconnection ourselves
    
    // Add default network from config
    wifi_manager->addNetwork(WIFI_SSID, WIFI_PASSWORD, 10);  // High priority
    
    // Initialize quality metrics
    current_quality = NetworkQuality();
    historical_quality = NetworkQuality();
    last_quality_check = millis();
    
    initialized = true;
    
    if (logger) {
        logger->log(LOG_INFO, "NetworkManager", "NetworkManager initialized with %u WiFi networks",
                   wifi_manager->getNetworkCount());
    }
    
    return true;
}

void NetworkManager::shutdown() {
    if (!initialized) {
        return;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "NetworkManager", "Shutting down NetworkManager");
        printStatistics();
    }
    
    disconnectFromServerInternal();
    disconnectFromWiFiInternal();
    
    initialized = false;
}

void NetworkManager::handleWiFiConnection() {
    if (!initialized || safe_mode) {
        return;
    }
    
    // Check current WiFi status
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifi_connected) {
            wifi_connected = true;
            wifi_reconnect_count++;
            
            auto logger = SystemManager::getInstance().getLogger();
            if (logger) {
                logger->log(LOG_INFO, "NetworkManager", "WiFi connected - IP: %s, RSSI: %d dBm",
                           WiFi.localIP().toString().c_str(), WiFi.RSSI());
            }
            
            // Publish connection event
            auto eventBus = SystemManager::getInstance().getEventBus();
            if (eventBus) {
                eventBus->publish(SystemEvent::NETWORK_CONNECTED);
            }
        }
        
        // Monitor quality
        monitorWiFiQuality();
        
        // Check if we should switch networks
        if (shouldAutoSwitchNetwork()) {
            switchToBestWiFiNetwork();
        }
        
    } else {
        if (wifi_connected) {
            wifi_connected = false;
            current_quality.connection_drops++;
            
            auto logger = SystemManager::getInstance().getLogger();
            if (logger) {
                logger->log(LOG_WARN, "NetworkManager", "WiFi connection lost");
            }
            
            // Publish disconnection event
            auto eventBus = SystemManager::getInstance().getEventBus();
            if (eventBus) {
                eventBus->publish(SystemEvent::NETWORK_DISCONNECTED);
            }
        }
        
        // Attempt reconnection
        static unsigned long last_reconnect_attempt = 0;
        unsigned long current_time = millis();
        
        if (current_time - last_reconnect_attempt >= 5000) {  // Try every 5 seconds
            last_reconnect_attempt = current_time;
            
            if (wifi_manager->hasNetworks()) {
                wifi_manager->connectToBestNetwork();
            } else {
                // Fallback to config-defined network
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            }
        }
    }
}

bool NetworkManager::connectToWiFiInternal() {
    if (wifi_manager->hasNetworks()) {
        return wifi_manager->connectToBestNetwork();
    } else {
        // Fallback to config
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            attempts++;
        }
        
        return WiFi.status() == WL_CONNECTED;
    }
}

bool NetworkManager::connectToServerInternal() {
    if (!wifi_connected || safe_mode) {
        return false;
    }
    
    if (server_connected) {
        return true;  // Already connected
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "NetworkManager", "Connecting to server %s:%d", 
                   SERVER_HOST, SERVER_PORT);
    }
    
    // Attempt connection with timeout
    if (client.connect(SERVER_HOST, SERVER_PORT, 10000)) {  // 10 second timeout
        server_connected = true;
        server_reconnect_count++;
        
        // Configure TCP keepalive
        client.setKeepAlive(true);
        client.setNoDelay(true);
        
        if (logger) {
            logger->log(LOG_INFO, "NetworkManager", "Server connection established");
        }
        
        // Publish connection event
        auto eventBus = SystemManager::getInstance().getEventBus();
        if (eventBus) {
            eventBus->publish(SystemEvent::SERVER_CONNECTED);
        }
        
        return true;
    } else {
        tcp_error_count++;
        
        if (logger) {
            logger->log(LOG_ERROR, "NetworkManager", "Server connection failed");
        }
        
        return false;
    }
}

void NetworkManager::disconnectFromWiFiInternal() {
    if (wifi_connected) {
        wifi_connected = false;
        WiFi.disconnect();
        
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LOG_INFO, "NetworkManager", "WiFi disconnected");
        }
    }
}

void NetworkManager::disconnectFromServerInternal() {
    if (server_connected) {
        server_connected = false;
        client.stop();
        
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LOG_INFO, "NetworkManager", "Server disconnected");
        }
        
        // Publish disconnection event
        auto eventBus = SystemManager::getInstance().getEventBus();
        if (eventBus) {
            eventBus->publish(SystemEvent::SERVER_DISCONNECTED);
        }
    }
}

bool NetworkManager::writeData(const uint8_t* data, size_t length) {
    if (!server_connected || !client.connected()) {
        return false;
    }
    
    size_t written = client.write(data, length);
    if (written == length) {
        bytes_sent += length;
        return true;
    } else {
        tcp_error_count++;
        
        // Connection might be broken
        if (!client.connected()) {
            disconnectFromServerInternal();
        }
        
        return false;
    }
}

bool NetworkManager::readData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!server_connected || !client.connected()) {
        return false;
    }
    
    *bytes_read = 0;
    
    // Check if data is available
    if (client.available()) {
        *bytes_read = client.read(buffer, buffer_size);
        bytes_received += *bytes_read;
        return true;
    }
    
    return false;  // No data available
}

void NetworkManager::monitorWiFiQuality() {
    unsigned long current_time = millis();
    if (current_time - last_quality_check < 5000) {  // Check every 5 seconds
        return;
    }
    
    last_quality_check = current_time;
    
    if (WiFi.status() == WL_CONNECTED) {
        // Update RSSI
        current_quality.rssi = WiFi.RSSI();
        
        // Calculate stability score (0-1)
        calculateStabilityScore();
        
        // Update historical data
        if (historical_quality.rssi == 0) {
            historical_quality = current_quality;
        } else {
            // Exponential smoothing
            historical_quality.rssi = 0.9f * historical_quality.rssi + 0.1f * current_quality.rssi;
            historical_quality.stability_score = 0.9f * historical_quality.stability_score + 0.1f * current_quality.stability_score;
        }
        
        // Check for quality degradation
        if (current_quality.stability_score < 0.5f) {
            auto eventBus = SystemManager::getInstance().getEventBus();
            if (eventBus) {
                eventBus->publish(SystemEvent::NETWORK_QUALITY_CHANGED, &current_quality);
            }
        }
    }
}

void NetworkManager::calculateStabilityScore() {
    // Calculate stability based on RSSI and connection history
    float rssi_score = 1.0f;
    
    if (current_quality.rssi > -50) {
        rssi_score = 1.0f;
    } else if (current_quality.rssi > -60) {
        rssi_score = 0.9f;
    } else if (current_quality.rssi > -70) {
        rssi_score = 0.7f;
    } else if (current_quality.rssi > -80) {
        rssi_score = 0.4f;
    } else {
        rssi_score = 0.1f;
    }
    
    // Factor in connection drops
    float drop_penalty = 1.0f - (current_quality.connection_drops * 0.1f);
    if (drop_penalty < 0.1f) drop_penalty = 0.1f;
    
    current_quality.stability_score = rssi_score * drop_penalty;
}

bool NetworkManager::shouldAutoSwitchNetwork() {
    if (wifi_manager->getNetworkCount() <= 1) {
        return false;
    }
    
    return wifi_manager->shouldSwitchNetwork(current_quality.rssi);
}

bool NetworkManager::switchToBestWiFiNetwork() {
    return wifi_manager->switchToNextNetwork();
}

void NetworkManager::addWiFiNetwork(const String& ssid, const String& password, int priority) {
    if (wifi_manager) {
        wifi_manager->addNetwork(ssid, password, priority);
    }
}

void NetworkManager::removeWiFiNetwork(const String& ssid) {
    if (wifi_manager) {
        wifi_manager->removeNetwork(ssid);
    }
}

void NetworkManager::clearWiFiNetworks() {
    if (wifi_manager) {
        wifi_manager->clearNetworks();
    }
}

int NetworkManager::getWiFiRSSI() const {
    return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

String NetworkManager::getWiFiSSID() const {
    return WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
}

IPAddress NetworkManager::getWiFiIP() const {
    return WiFi.status() == WL_CONNECTED ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
}

void NetworkManager::printNetworkInfo() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "NetworkManager", "=== Network Information ===");
    logger->log(LOG_INFO, "NetworkManager", "WiFi Connected: %s", wifi_connected ? "yes" : "no");
    
    if (wifi_connected) {
        logger->log(LOG_INFO, "NetworkManager", "WiFi SSID: %s", getWiFiSSID().c_str());
        logger->log(LOG_INFO, "NetworkManager", "WiFi IP: %s", getWiFiIP().toString().c_str());
        logger->log(LOG_INFO, "NetworkManager", "WiFi RSSI: %d dBm", getWiFiRSSI());
        logger->log(LOG_INFO, "NetworkManager", "Network Stability: %.2f", current_quality.stability_score);
    }
    
    logger->log(LOG_INFO, "NetworkManager", "Server Connected: %s", server_connected ? "yes" : "no");
    logger->log(LOG_INFO, "NetworkManager", "Server Host: %s:%d", SERVER_HOST, SERVER_PORT);
    logger->log(LOG_INFO, "NetworkManager", "==========================");
}

void NetworkManager::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "NetworkManager", "=== Network Statistics ===");
    logger->log(LOG_INFO, "NetworkManager", "WiFi Reconnects: %u", wifi_reconnect_count);
    logger->log(LOG_INFO, "NetworkManager", "Server Reconnects: %u", server_reconnect_count);
    logger->log(LOG_INFO, "NetworkManager", "TCP Errors: %u", tcp_error_count);
    logger->log(LOG_INFO, "NetworkManager", "Bytes Sent: %u", bytes_sent);
    logger->log(LOG_INFO, "NetworkManager", "Bytes Received: %u", bytes_received);
    logger->log(LOG_INFO, "NetworkManager", "Connection Drops: %u", current_quality.connection_drops);
    logger->log(LOG_INFO, "NetworkManager", "Current RSSI: %d dBm", current_quality.rssi);
    logger->log(LOG_INFO, "NetworkManager", "Network Stability: %.2f", current_quality.stability_score);
    logger->log(LOG_INFO, "NetworkManager", "==========================");
}

bool NetworkManager::validateConnection() const {
    if (wifi_connected && WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    if (server_connected && !client.connected()) {
        return false;
    }
    
    return true;
}

std::vector<String> NetworkManager::getAvailableNetworks() const {
    std::vector<String> available_networks;
    
    // This would typically involve a WiFi scan
    // For now, return configured networks
    for (size_t i = 0; i < wifi_manager->getNetworkCount(); i++) {
        available_networks.push_back(wifi_manager->getCurrentNetwork().ssid);
    }
    
    return available_networks;
}

bool NetworkManager::testConnectionQuality() {
    // Simple connection quality test
    // In a full implementation, this would do more sophisticated testing
    
    if (!wifi_connected) {
        return false;
    }
    
    // Test by pinging a known host or measuring round-trip time
    // For now, just return true if connected
    return true;
}

float NetworkManager::estimateBandwidth() {
    // Simple bandwidth estimation
    // In a full implementation, this would measure actual throughput
    
    if (!wifi_connected) {
        return 0.0f;
    }
    
    // Estimate based on RSSI
    if (current_quality.rssi > -50) {
        return 50000.0f;  // 50 Mbps
    } else if (current_quality.rssi > -60) {
        return 30000.0f;  // 30 Mbps
    } else if (current_quality.rssi > -70) {
        return 10000.0f;  // 10 Mbps
    } else {
        return 1000.0f;   // 1 Mbps
    }
}