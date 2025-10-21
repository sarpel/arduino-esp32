#include "ConnectionPool.h"
#include "../utils/EnhancedLogger.h"

ConnectionPool::ConnectionPool() 
    : primary_connection_id(0), backup_connection_id(1),
      connection_timeout(5000), idle_timeout(30000),
      health_check_interval(10000), last_health_check(0),
      total_reconnects(0), failovers(0) {
    
    for (uint8_t i = 0; i < MAX_CONNECTIONS; i++) {
        connections.push_back(std::make_shared<PooledConnection>());
        connections[i]->id = i;
    }
}

bool ConnectionPool::initialize() {
    for (auto& conn : connections) {
        conn->state = ConnectionState::IDLE;
        conn->error_count = 0;
    }
    return true;
}

void ConnectionPool::shutdown() {
    closeAllConnections();
}

bool ConnectionPool::addConnection(const IPAddress& server_ip, uint16_t port) {
    for (auto& conn : connections) {
        if (conn->state == ConnectionState::IDLE) {
            conn->server_ip = server_ip;
            conn->server_port = port;
            return true;
        }
    }
    return false;
}

bool ConnectionPool::removeConnection(uint8_t connection_id) {
    if (connection_id >= connections.size()) {
        return false;
    }
    
    if (connections[connection_id]->client.connected()) {
        connections[connection_id]->client.stop();
    }
    
    connections[connection_id]->state = ConnectionState::IDLE;
    return true;
}

void ConnectionPool::closeAllConnections() {
    for (auto& conn : connections) {
        if (conn->client.connected()) {
            conn->client.stop();
        }
        conn->state = ConnectionState::IDLE;
    }
}

bool ConnectionPool::connectPrimary() {
    if (primary_connection_id >= connections.size()) {
        return false;
    }
    
    PooledConnection* conn = connections[primary_connection_id].get();
    if (!conn) {
        return false;
    }
    
    if (conn->client.connect(conn->server_ip, conn->server_port)) {
        conn->state = ConnectionState::CONNECTED;
        conn->connection_time = millis();
        conn->last_activity_time = millis();
        return true;
    }
    
    conn->state = ConnectionState::FAILED;
    conn->error_count++;
    total_reconnects++;
    return false;
}

bool ConnectionPool::connectBackup() {
    if (backup_connection_id >= connections.size()) {
        return false;
    }
    
    PooledConnection* conn = connections[backup_connection_id].get();
    if (!conn) {
        return false;
    }
    
    if (conn->client.connect(conn->server_ip, conn->server_port)) {
        conn->state = ConnectionState::BACKUP;
        conn->connection_time = millis();
        conn->last_activity_time = millis();
        return true;
    }
    
    conn->state = ConnectionState::FAILED;
    conn->error_count++;
    total_reconnects++;
    return false;
}

bool ConnectionPool::failoverToBackup() {
    failovers++;
    
    if (primary_connection_id >= connections.size()) {
        return false;
    }
    
    PooledConnection* primary = connections[primary_connection_id].get();
    if (primary && primary->client.connected()) {
        primary->client.stop();
    }
    
    primary->state = ConnectionState::FAILED;
    
    if (connectBackup()) {
        backup_connection_id = primary_connection_id;
        primary_connection_id = backup_connection_id;
        return true;
    }
    
    return false;
}

bool ConnectionPool::reconnectIfNeeded() {
    if (primary_connection_id >= connections.size()) {
        return false;
    }
    
    PooledConnection* conn = connections[primary_connection_id].get();
    if (!conn) {
        return false;
    }
    
    if (!conn->client.connected()) {
        return connectPrimary();
    }
    
    return true;
}

PooledConnection* ConnectionPool::getPrimaryConnection() {
    if (primary_connection_id < connections.size()) {
        return connections[primary_connection_id].get();
    }
    return nullptr;
}

PooledConnection* ConnectionPool::getBackupConnection() {
    if (backup_connection_id < connections.size()) {
        return connections[backup_connection_id].get();
    }
    return nullptr;
}

PooledConnection* ConnectionPool::getHealthiestConnection() {
    PooledConnection* healthiest = nullptr;
    uint32_t min_errors = UINT32_MAX;
    
    for (auto& conn : connections) {
        if (conn->state == ConnectionState::CONNECTED && conn->error_count < min_errors) {
            healthiest = conn.get();
            min_errors = conn->error_count;
        }
    }
    
    return healthiest;
}

PooledConnection* ConnectionPool::getConnection(uint8_t id) {
    if (id < connections.size()) {
        return connections[id].get();
    }
    return nullptr;
}

bool ConnectionPool::sendData(uint8_t connection_id, const uint8_t* data, size_t length) {
    PooledConnection* conn = getConnection(connection_id);
    if (!conn || !conn->client.connected()) {
        return false;
    }
    
    size_t written = conn->client.write(data, length);
    conn->last_activity_time = millis();
    
    if (written == length) {
        conn->bytes_sent += written;
        return true;
    }
    
    conn->error_count++;
    return false;
}

bool ConnectionPool::sendDataPrimary(const uint8_t* data, size_t length) {
    return sendData(primary_connection_id, data, length);
}

bool ConnectionPool::receiveData(uint8_t connection_id, uint8_t* buffer, size_t buffer_size, size_t& bytes_read) {
    PooledConnection* conn = getConnection(connection_id);
    if (!conn || !conn->client.connected()) {
        bytes_read = 0;
        return false;
    }
    
    bytes_read = conn->client.readBytes(buffer, buffer_size);
    conn->last_activity_time = millis();
    conn->bytes_received += bytes_read;
    
    return bytes_read > 0;
}

void ConnectionPool::updateStatistics(uint8_t connection_id, size_t bytes_sent, size_t bytes_received, bool error) {
    PooledConnection* conn = getConnection(connection_id);
    if (!conn) {
        return;
    }
    
    conn->bytes_sent += bytes_sent;
    conn->bytes_received += bytes_received;
    conn->last_activity_time = millis();
    
    if (error) {
        conn->error_count++;
    }
}

ConnectionState ConnectionPool::getConnectionState(uint8_t connection_id) {
    PooledConnection* conn = getConnection(connection_id);
    return conn ? conn->state : ConnectionState::IDLE;
}

bool ConnectionPool::isConnectionActive(uint8_t connection_id) {
    PooledConnection* conn = getConnection(connection_id);
    if (!conn) {
        return false;
    }
    
    return conn->client.connected() && 
           (conn->state == ConnectionState::CONNECTED || conn->state == ConnectionState::BACKUP);
}

uint8_t ConnectionPool::getActiveConnectionCount() {
    uint8_t count = 0;
    for (const auto& conn : connections) {
        if (conn->client.connected()) {
            count++;
        }
    }
    return count;
}

void ConnectionPool::updateConnectionHealth() {
    for (auto& conn : connections) {
        if (!isConnectionHealthy(*conn)) {
            if (conn->client.connected()) {
                conn->client.stop();
            }
            conn->state = ConnectionState::FAILED;
        }
    }
}

bool ConnectionPool::isConnectionHealthy(const PooledConnection& conn) {
    if (!conn.client.connected()) {
        return false;
    }
    
    unsigned long idle_duration = millis() - conn.last_activity_time;
    if (idle_duration > idle_timeout) {
        return false;
    }
    
    return conn.error_count < 10;
}

void ConnectionPool::performHealthCheck() {
    unsigned long current_time = millis();
    if (current_time - last_health_check < health_check_interval) {
        return;
    }
    
    last_health_check = current_time;
    updateConnectionHealth();
}

void ConnectionPool::printPoolStatus() const {
    EnhancedLogger& logger = EnhancedLogger::getInstance();
    
    logger.log(LogLevel::LOG_INFO, "=== Connection Pool Status ===");
    logger.log(LogLevel::LOG_INFO, "Active Connections: %u", getActiveConnectionCount());
    logger.log(LogLevel::LOG_INFO, "Total Reconnects: %u", total_reconnects);
    logger.log(LogLevel::LOG_INFO, "Failovers: %u", failovers);
    
    for (size_t i = 0; i < connections.size(); i++) {
        const auto& conn = connections[i];
        logger.log(LogLevel::LOG_INFO, "Connection %u: State=%d, Errors=%u, Sent=%u, Received=%u",
                  i, static_cast<int>(conn->state), conn->error_count, 
                  conn->bytes_sent, conn->bytes_received);
    }
}
