#include "network.h"
#include "logger.h"
#include "esp_task_wdt.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <sys/time.h>

// Simple LCG for jitter generation (no <random> to keep footprint small)
static uint32_t _nb_rng = 2166136261u;
static inline uint32_t nb_rand() {
    _nb_rng = _nb_rng * 1664525u + 1013904223u;
    return _nb_rng;
}
static inline unsigned long apply_jitter(unsigned long base_ms) {
    #if SERVER_BACKOFF_JITTER_PCT > 0
    uint32_t r = nb_rand();
    int32_t jitter_range = (int32_t)(base_ms * SERVER_BACKOFF_JITTER_PCT / 100);
    int32_t jitter = (int32_t)(r % (2 * (uint32_t)jitter_range + 1)) - jitter_range; // [-range, +range]
    long with_jitter = (long)base_ms + jitter;
    if (with_jitter < (long)SERVER_RECONNECT_MIN) with_jitter = SERVER_RECONNECT_MIN;
    if ((unsigned long)with_jitter > SERVER_RECONNECT_MAX) with_jitter = SERVER_RECONNECT_MAX;
    return (unsigned long)with_jitter;
    #else
    return base_ms;
    #endif
}

// ExponentialBackoff implementation
ExponentialBackoff::ExponentialBackoff(unsigned long min_ms, unsigned long max_ms)
    : min_delay(min_ms), max_delay(max_ms), current_delay(min_ms), consecutive_failures(0) {}

unsigned long ExponentialBackoff::getNextDelay() {
    if (consecutive_failures > 0) {
        current_delay = min(current_delay * 2, max_delay);
    }
    consecutive_failures++;
    // Apply jitter to avoid sync storms
    return apply_jitter(current_delay);
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

// TCP Connection State Machine members
TCPConnectionState NetworkManager::tcp_state = TCPConnectionState::DISCONNECTED;
unsigned long NetworkManager::tcp_state_change_time = 0;
unsigned long NetworkManager::tcp_connection_established_time = 0;
uint32_t NetworkManager::tcp_state_changes = 0;

void NetworkManager::initialize() {
    LOG_INFO("Initializing network...");

    // Initialize adaptive buffer management
    AdaptiveBuffer::initialize(I2S_BUFFER_SIZE);

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
        // Enter safe backoff mode instead of rebooting; keep serial alive
        unsigned long backoff = 1000UL * (wifi_retry_count - WIFI_MAX_RETRIES);
        if (backoff > 30000UL) backoff = 30000UL;
        // Add small jitter to avoid herd reconnects
        backoff = apply_jitter(backoff);
        LOG_CRITICAL("WiFi connection failed after %d attempts - backing off %lu ms (no reboot)", WIFI_MAX_RETRIES, backoff);
        wifi_retry_timer.setInterval(backoff);
        wifi_retry_timer.start();
        wifi_retry_count = WIFI_MAX_RETRIES; // clamp to avoid overflow
        return;
    }
}

bool NetworkManager::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::monitorWiFiQuality() {
    if (!rssi_check_timer.check()) return;
    if (!isWiFiConnected()) return;

    int32_t rssi = WiFi.RSSI();

    // Update adaptive buffer based on signal strength
    AdaptiveBuffer::updateBufferSize(rssi);

    if (rssi < RSSI_WEAK_THRESHOLD) {
        LOG_WARN("Weak WiFi signal: %d dBm - increasing buffer, avoiding forced disconnect", rssi);
        // No forced disconnect; rely on natural link loss and adaptive buffering
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

    // Update state to CONNECTING
    updateTCPState(TCPConnectionState::CONNECTING);

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

        // Update state to CONNECTED
        updateTCPState(TCPConnectionState::CONNECTED);

        // Configure TCP keepalive for dead connection detection
        int sockfd = client.fd();
        if (sockfd >= 0) {
            int keepAlive = 1;
            int keepIdle = TCP_KEEPALIVE_IDLE;      // seconds
            int keepInterval = TCP_KEEPALIVE_INTERVAL;  // seconds
            int keepCount = TCP_KEEPALIVE_COUNT;     // count

            setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));

            // Set send timeout to avoid indefinite blocking writes
            struct timeval snd_to;
            snd_to.tv_sec = TCP_WRITE_TIMEOUT / 1000;
            snd_to.tv_usec = (TCP_WRITE_TIMEOUT % 1000) * 1000;
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &snd_to, sizeof(snd_to));

            // Optional receive timeout (not used yet but safer defaults)
            struct timeval rcv_to;
            rcv_to.tv_sec = 5;
            rcv_to.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &rcv_to, sizeof(rcv_to));

            LOG_DEBUG("TCP keepalive and timeouts configured");
        }

        return true;
    } else {
        LOG_ERROR("Server connection failed");
        server_connected = false;

        // Update state to ERROR
        handleTCPError("connectToServer");

        // Set next retry time with exponential backoff + jitter
        unsigned long next_delay = server_backoff.getNextDelay();
        server_retry_timer.setInterval(next_delay);
        server_retry_timer.start();

        LOG_INFO("Next server connection attempt in %lu ms", next_delay);
        return false;
    }
}

void NetworkManager::disconnectFromServer() {
    if (server_connected || client.connected()) {
        // Update state to CLOSING
        updateTCPState(TCPConnectionState::CLOSING);

        LOG_INFO("Disconnecting from server");
        client.stop();
        server_connected = false;

        // Update state to DISCONNECTED
        updateTCPState(TCPConnectionState::DISCONNECTED);
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

    // Write in chunks to minimize long blocking writes and respect SO_SNDTIMEO
    size_t total_sent = 0;
    while (total_sent < length) {
        size_t chunk = min((size_t)TCP_CHUNK_SIZE, length - total_sent);
        size_t sent = client.write(data + total_sent, chunk);
        if (sent == 0) {
            LOG_ERROR("TCP write returned 0 (timeout or error) after %u/%u bytes", (unsigned)total_sent, (unsigned)length);
            handleTCPError("writeData");
            if (millis() - last_successful_write > TCP_WRITE_TIMEOUT) {
                LOG_ERROR("TCP write timeout - closing stale connection");
                disconnectFromServer();
            }
            return false;
        }
        total_sent += sent;
    }

    last_successful_write = millis();
    return true;
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

// ===== TCP Connection State Machine Implementation =====

void NetworkManager::updateTCPState(TCPConnectionState new_state) {
    if (tcp_state != new_state) {
        TCPConnectionState old_state = tcp_state;
        tcp_state = new_state;
        tcp_state_change_time = millis();
        tcp_state_changes++;

        // Log state transitions
        const char* old_name = "UNKNOWN";
        const char* new_name = "UNKNOWN";

        switch (old_state) {
            case TCPConnectionState::DISCONNECTED: old_name = "DISCONNECTED"; break;
            case TCPConnectionState::CONNECTING: old_name = "CONNECTING"; break;
            case TCPConnectionState::CONNECTED: old_name = "CONNECTED"; break;
            case TCPConnectionState::ERROR: old_name = "ERROR"; break;
            case TCPConnectionState::CLOSING: old_name = "CLOSING"; break;
        }

        switch (new_state) {
            case TCPConnectionState::DISCONNECTED: new_name = "DISCONNECTED"; break;
            case TCPConnectionState::CONNECTING: new_name = "CONNECTING"; break;
            case TCPConnectionState::CONNECTED: new_name = "CONNECTED"; break;
            case TCPConnectionState::ERROR: new_name = "ERROR"; break;
            case TCPConnectionState::CLOSING: new_name = "CLOSING"; break;
        }

        LOG_INFO("TCP state transition: %s → %s", old_name, new_name);

        // Update connection established time when entering CONNECTED state
        if (new_state == TCPConnectionState::CONNECTED) {
            tcp_connection_established_time = millis();
        }
    }
}

void NetworkManager::handleTCPError(const char* error_source) {
    tcp_error_count++;
    LOG_ERROR("TCP error from %s", error_source);
    updateTCPState(TCPConnectionState::ERROR);
}

bool NetworkManager::validateConnection() {
    // Validate that connection state matches actual TCP connection
    bool is_actually_connected = client.connected();
    bool state_says_connected = (tcp_state == TCPConnectionState::CONNECTED);

    if (state_says_connected && !is_actually_connected) {
        LOG_WARN("TCP state mismatch: state=CONNECTED but client.connected()=false");
        updateTCPState(TCPConnectionState::DISCONNECTED);
        return false;
    }

    if (!state_says_connected && is_actually_connected) {
        LOG_WARN("TCP state mismatch: state!= CONNECTED but client.connected()=true");
        updateTCPState(TCPConnectionState::CONNECTED);
        return true;
    }

    return is_actually_connected;
}

TCPConnectionState NetworkManager::getTCPState() {
    validateConnection();  // Synchronize state with actual connection
    return tcp_state;
}

bool NetworkManager::isTCPConnecting() {
    return tcp_state == TCPConnectionState::CONNECTING;
}

bool NetworkManager::isTCPConnected() {
    validateConnection();  // Synchronize before returning
    return tcp_state == TCPConnectionState::CONNECTED;
}

bool NetworkManager::isTCPError() {
    return tcp_state == TCPConnectionState::ERROR;
}

unsigned long NetworkManager::getTimeSinceLastWrite() {
    return millis() - last_successful_write;
}

unsigned long NetworkManager::getConnectionUptime() {
    if (tcp_state != TCPConnectionState::CONNECTED) {
        return 0;
    }
    return millis() - tcp_connection_established_time;
}

uint32_t NetworkManager::getTCPStateChangeCount() {
    return tcp_state_changes;
}

// ===== Adaptive Buffer Management =====

void NetworkManager::updateAdaptiveBuffer() {
    if (!isWiFiConnected()) return;
    AdaptiveBuffer::updateBufferSize(WiFi.RSSI());
}

size_t NetworkManager::getAdaptiveBufferSize() {
    return AdaptiveBuffer::getBufferSize();
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

// TCP Connection State Machine members
TCPConnectionState NetworkManager::tcp_state = TCPConnectionState::DISCONNECTED;
unsigned long NetworkManager::tcp_state_change_time = 0;
unsigned long NetworkManager::tcp_connection_established_time = 0;
uint32_t NetworkManager::tcp_state_changes = 0;

void NetworkManager::initialize() {
    LOG_INFO("Initializing network...");

    // Initialize adaptive buffer management
    AdaptiveBuffer::initialize(I2S_BUFFER_SIZE);

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
        // Enter safe backoff mode instead of rebooting; keep serial alive
        unsigned long backoff = 1000UL * (wifi_retry_count - WIFI_MAX_RETRIES);
        if (backoff > 30000UL) backoff = 30000UL;
        LOG_CRITICAL("WiFi connection failed after %d attempts - backing off %lu ms (no reboot)", WIFI_MAX_RETRIES, backoff);
        wifi_retry_timer.setInterval(backoff);
        wifi_retry_timer.start();
        wifi_retry_count = WIFI_MAX_RETRIES; // clamp to avoid overflow
        return;
    }
}

bool NetworkManager::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::monitorWiFiQuality() {
    if (!rssi_check_timer.check()) return;
    if (!isWiFiConnected()) return;

    int32_t rssi = WiFi.RSSI();

    // Update adaptive buffer based on signal strength
    AdaptiveBuffer::updateBufferSize(rssi);

    if (rssi < RSSI_WEAK_THRESHOLD) {
        LOG_WARN("Weak WiFi signal: %d dBm - increasing buffer, avoiding forced disconnect", rssi);
        // No forced disconnect; rely on natural link loss and adaptive buffering
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

    // Update state to CONNECTING
    updateTCPState(TCPConnectionState::CONNECTING);

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

        // Update state to CONNECTED
        updateTCPState(TCPConnectionState::CONNECTED);

        // Configure TCP keepalive for dead connection detection
        int sockfd = client.fd();
        if (sockfd >= 0) {
            int keepAlive = 1;
            int keepIdle = TCP_KEEPALIVE_IDLE;      // seconds
            int keepInterval = TCP_KEEPALIVE_INTERVAL;  // seconds
            int keepCount = TCP_KEEPALIVE_COUNT;     // count

            setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));

            // Set send timeout to avoid indefinite blocking writes
            struct timeval snd_to;
            snd_to.tv_sec = TCP_WRITE_TIMEOUT / 1000;
            snd_to.tv_usec = (TCP_WRITE_TIMEOUT % 1000) * 1000;
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &snd_to, sizeof(snd_to));

            LOG_DEBUG("TCP keepalive and send timeout configured");
        }

        return true;
    } else {
        LOG_ERROR("Server connection failed");
        server_connected = false;

        // Update state to ERROR
        handleTCPError("connectToServer");

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
        // Update state to CLOSING
        updateTCPState(TCPConnectionState::CLOSING);

        LOG_INFO("Disconnecting from server");
        client.stop();
        server_connected = false;

        // Update state to DISCONNECTED
        updateTCPState(TCPConnectionState::DISCONNECTED);
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

    // Attempt write; Arduino WiFiClient.write is non-blocking per chunk but may block overall; rely on SO_SNDTIMEO
    size_t bytes_sent = client.write(data, length);

    if (bytes_sent == length) {
        last_successful_write = millis();
        return true;
    } else {
        LOG_ERROR("TCP write incomplete: sent %u of %u bytes", (unsigned)bytes_sent, (unsigned)length);

        // Handle write error
        handleTCPError("writeData");

        // Close connection if no successful write within timeout window
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

// ===== TCP Connection State Machine Implementation =====

void NetworkManager::updateTCPState(TCPConnectionState new_state) {
    if (tcp_state != new_state) {
        TCPConnectionState old_state = tcp_state;
        tcp_state = new_state;
        tcp_state_change_time = millis();
        tcp_state_changes++;

        // Log state transitions
        const char* old_name = "UNKNOWN";
        const char* new_name = "UNKNOWN";

        switch (old_state) {
            case TCPConnectionState::DISCONNECTED: old_name = "DISCONNECTED"; break;
            case TCPConnectionState::CONNECTING: old_name = "CONNECTING"; break;
            case TCPConnectionState::CONNECTED: old_name = "CONNECTED"; break;
            case TCPConnectionState::ERROR: old_name = "ERROR"; break;
            case TCPConnectionState::CLOSING: old_name = "CLOSING"; break;
        }

        switch (new_state) {
            case TCPConnectionState::DISCONNECTED: new_name = "DISCONNECTED"; break;
            case TCPConnectionState::CONNECTING: new_name = "CONNECTING"; break;
            case TCPConnectionState::CONNECTED: new_name = "CONNECTED"; break;
            case TCPConnectionState::ERROR: new_name = "ERROR"; break;
            case TCPConnectionState::CLOSING: new_name = "CLOSING"; break;
        }

        LOG_INFO("TCP state transition: %s → %s", old_name, new_name);

        // Update connection established time when entering CONNECTED state
        if (new_state == TCPConnectionState::CONNECTED) {
            tcp_connection_established_time = millis();
        }
    }
}

void NetworkManager::handleTCPError(const char* error_source) {
    tcp_error_count++;
    LOG_ERROR("TCP error from %s", error_source);
    updateTCPState(TCPConnectionState::ERROR);
}

bool NetworkManager::validateConnection() {
    // Validate that connection state matches actual TCP connection
    bool is_actually_connected = client.connected();
    bool state_says_connected = (tcp_state == TCPConnectionState::CONNECTED);

    if (state_says_connected && !is_actually_connected) {
        LOG_WARN("TCP state mismatch: state=CONNECTED but client.connected()=false");
        updateTCPState(TCPConnectionState::DISCONNECTED);
        return false;
    }

    if (!state_says_connected && is_actually_connected) {
        LOG_WARN("TCP state mismatch: state!= CONNECTED but client.connected()=true");
        updateTCPState(TCPConnectionState::CONNECTED);
        return true;
    }

    return is_actually_connected;
}

TCPConnectionState NetworkManager::getTCPState() {
    validateConnection();  // Synchronize state with actual connection
    return tcp_state;
}

bool NetworkManager::isTCPConnecting() {
    return tcp_state == TCPConnectionState::CONNECTING;
}

bool NetworkManager::isTCPConnected() {
    validateConnection();  // Synchronize before returning
    return tcp_state == TCPConnectionState::CONNECTED;
}

bool NetworkManager::isTCPError() {
    return tcp_state == TCPConnectionState::ERROR;
}

unsigned long NetworkManager::getTimeSinceLastWrite() {
    return millis() - last_successful_write;
}

unsigned long NetworkManager::getConnectionUptime() {
    if (tcp_state != TCPConnectionState::CONNECTED) {
        return 0;
    }
    return millis() - tcp_connection_established_time;
}

uint32_t NetworkManager::getTCPStateChangeCount() {
    return tcp_state_changes;
}

// ===== Adaptive Buffer Management =====

void NetworkManager::updateAdaptiveBuffer() {
    if (!isWiFiConnected()) return;
    AdaptiveBuffer::updateBufferSize(WiFi.RSSI());
}

size_t NetworkManager::getAdaptiveBufferSize() {
    return AdaptiveBuffer::getBufferSize();
}
