#include "network.h"
#include "logger.h"
#include "esp_task_wdt.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>

// ExponentialBackoff implementation
ExponentialBackoff::ExponentialBackoff(unsigned long min_ms, unsigned long max_ms)
    : min_delay(min_ms), max_delay(max_ms), current_delay(min_ms), consecutive_failures(0) {}

unsigned long ExponentialBackoff::getNextDelay() {
    if (consecutive_failures > 0) {
        current_delay = min(current_delay * 2, max_delay);
    }
    consecutive_failures++;
    return current_delay;
}

void ExponentialBackoff::reset() {
    consecutive_failures = 0;
    current_delay = min_delay;
}

// NetworkManager static members
bool NetworkManager::server_connected = false;
unsigned long NetworkManager::last_successful_write = 0;
NonBlockingTimer NetworkManager::wifi_retry_timer(WIFI_RETRY_DELAY, true);
NonBlockingTimer NetworkManager::server_retry_timer(SERVER_RECONNECT_MIN, false);
NonBlockingTimer NetworkManager::rssi_check_timer(RSSI_CHECK_INTERVAL, true);
ExponentialBackoff NetworkManager::server_backoff;
WiFiClient NetworkManager::client;
uint32_t NetworkManager::wifi_reconnect_count = 0;
uint32_t NetworkManager::server_reconnect_count = 0;
uint32_t NetworkManager::tcp_error_count = 0;
int NetworkManager::wifi_retry_count = 0;

void NetworkManager::initialize() {
    LOG_INFO("Initializing network...");

    // Configure WiFi for reliability
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);  // Prevent power-save disconnects
    WiFi.persistent(false); // Reduce flash wear

    // Configure static IP if enabled
    #ifdef USE_STATIC_IP
    IPAddress local_IP(STATIC_IP);
    IPAddress gateway(GATEWAY_IP);
    IPAddress subnet(SUBNET_MASK);
    IPAddress dns(DNS_IP);
    if (WiFi.config(local_IP, gateway, subnet, dns)) {
        LOG_INFO("Static IP configured: %s", local_IP.toString().c_str());
    } else {
        LOG_ERROR("Static IP configuration failed - falling back to DHCP");
    }
    #endif

    // Start WiFi connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifi_retry_timer.start();
    wifi_retry_count = 0;

    LOG_INFO("Network initialization started");
}

void NetworkManager::handleWiFiConnection() {
    // If already connected, just return
    if (WiFi.status() == WL_CONNECTED) {
        if (wifi_retry_count > 0) {
            // Just connected after retries
            LOG_INFO("WiFi connected after %d attempts", wifi_retry_count);
            wifi_reconnect_count++;
            wifi_retry_count = 0;
        }
        return;
    }

    // Not connected - handle reconnection with non-blocking timer
    if (!wifi_retry_timer.check()) {
        return; // Not time to retry yet
    }

    // Feed watchdog to prevent resets during connection
    esp_task_wdt_reset();

    if (wifi_retry_count == 0) {
        LOG_WARN("WiFi disconnected - attempting reconnection...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        server_connected = false;
        client.stop();
    }

    wifi_retry_count++;

    if (wifi_retry_count > WIFI_MAX_RETRIES) {
        LOG_CRITICAL("WiFi connection failed after %d attempts - rebooting", WIFI_MAX_RETRIES);
        delay(1000);
        ESP.restart();
    }
}

bool NetworkManager::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::monitorWiFiQuality() {
    if (!rssi_check_timer.check()) return;
    if (!isWiFiConnected()) return;

    int32_t rssi = WiFi.RSSI();

    if (rssi < RSSI_WEAK_THRESHOLD) {
        LOG_WARN("Weak WiFi signal: %d dBm - triggering preemptive reconnection", rssi);
        WiFi.disconnect();
        WiFi.reconnect();
    } else if (rssi < -70) {
        LOG_WARN("WiFi signal degraded: %d dBm", rssi);
    }
}

bool NetworkManager::connectToServer() {
    if (!isWiFiConnected()) {
        return false;
    }

    // Check if it's time to retry (using exponential backoff)
    if (!server_retry_timer.isExpired()) {
        return false;
    }

    LOG_INFO("Attempting to connect to server %s:%d (attempt %d)...",
             SERVER_HOST, SERVER_PORT, server_backoff.getFailureCount() + 1);

    // Feed watchdog during connection attempt
    esp_task_wdt_reset();

    if (client.connect(SERVER_HOST, SERVER_PORT)) {
        LOG_INFO("Server connection established");
        server_connected = true;
        last_successful_write = millis();
        server_backoff.reset();
        server_reconnect_count++;

        // Configure TCP keepalive for dead connection detection
        int sockfd = client.fd();
        if (sockfd >= 0) {
            int keepAlive = 1;
            int keepIdle = 5;      // Start probing after 5s idle
            int keepInterval = 5;  // Probe every 5s
            int keepCount = 3;     // Drop after 3 failed probes

            setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));

            LOG_DEBUG("TCP keepalive configured");
        }

        return true;
    } else {
        LOG_ERROR("Server connection failed");
        server_connected = false;

        // Set next retry time with exponential backoff
        unsigned long next_delay = server_backoff.getNextDelay();
        server_retry_timer.setInterval(next_delay);
        server_retry_timer.start();

        LOG_INFO("Next server connection attempt in %lu ms", next_delay);
        return false;
    }
}

void NetworkManager::disconnectFromServer() {
    if (server_connected || client.connected()) {
        LOG_INFO("Disconnecting from server");
        client.stop();
        server_connected = false;
    }
}

bool NetworkManager::isServerConnected() {
    // Double-check: our flag AND actual connection state
    if (server_connected && !client.connected()) {
        LOG_WARN("Server connection lost unexpectedly");
        server_connected = false;
        server_retry_timer.setInterval(SERVER_RECONNECT_MIN);
        server_retry_timer.start();
    }
    return server_connected;
}

WiFiClient& NetworkManager::getClient() {
    return client;
}

bool NetworkManager::writeData(const uint8_t* data, size_t length) {
    if (!isServerConnected()) {
        return false;
    }

    size_t bytes_sent = client.write(data, length);

    if (bytes_sent == length) {
        last_successful_write = millis();
        return true;
    } else {
        tcp_error_count++;
        LOG_ERROR("TCP write incomplete: sent %u of %u bytes", bytes_sent, length);

        // Check for write timeout
        if (millis() - last_successful_write > TCP_WRITE_TIMEOUT) {
            LOG_ERROR("TCP write timeout - closing stale connection");
            disconnectFromServer();
        }

        return false;
    }
}

uint32_t NetworkManager::getWiFiReconnectCount() {
    return wifi_reconnect_count;
}

uint32_t NetworkManager::getServerReconnectCount() {
    return server_reconnect_count;
}

uint32_t NetworkManager::getTCPErrorCount() {
    return tcp_error_count;
}
